/**
 * @file retire_mgr.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-04
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/internal/alloc_only_allocator.hpp"
#include "alconcurrent/internal/cpp_std_configure.hpp"
#include "alconcurrent/internal/hazard_ptr_internal.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

/**
 * @brief グローバル変数専用のretire pointer管理クラス
 *
 */
class alignas( internal::atomic_variable_align ) global_retire_mgr {
public:
	class locker {
	public:
		bool owns_lock() const noexcept
		{
			return ul_.owns_lock();
		}

		void transfer( retire_node_abst* p_list_head )
		{
			if ( !owns_lock() ) {
				throw std::logic_error( "Need to get mutex lock" );
			}
			parent_.unlock_append_node( p_list_head );
		}

		bool recycle_one( void )
		{
			if ( !owns_lock() ) {
				LogOutput( log_type::ERR, "Need to get mutex lock" );
				return false;
			}

			return parent_.unlock_recycle_one();
		}

	private:
		locker( global_retire_mgr& parent_arg )
		  : parent_( parent_arg )
		  , ul_( parent_.mtx_ )
		{
		}
		locker( global_retire_mgr& parent_arg, std::try_to_lock_t )
		  : parent_( parent_arg )
		  , ul_( parent_.mtx_, std::try_to_lock )
		{
		}

		global_retire_mgr&           parent_;
		std::unique_lock<std::mutex> ul_;

		friend class global_retire_mgr;
	};

	constexpr global_retire_mgr( void ) noexcept
	  : mtx_()
	  , p_head_( nullptr )
	{
	}

	~global_retire_mgr()
	{
		// delete all without any check
		retire_node_abst* p_cur = p_head_;
		while ( p_cur != nullptr ) {
			retire_node_abst* p_nxt = p_cur->p_next_;
			delete p_cur;
			p_cur = p_nxt;
		}
	}

	/**
	 * @brief receive retire node list
	 *
	 * @pre p_next_ of last node in p_list_head should be nullptr;
	 *
	 * @warning this API is NOT lock-free
	 *
	 * @param p_list_head
	 */
	void transfer( retire_node_abst* p_list_head )
	{
		std::lock_guard<std::mutex> lg( mtx_ );
		unlock_append_node( p_list_head );
	}

	locker try_lock( void )
	{
		return locker( *this, std::try_to_lock );
	}

private:
	void unlock_append_node( retire_node_abst* p_node )
	{
		if ( p_head_ == nullptr ) {
			p_head_ = p_node;
		} else {
			// add new node to last
			retire_node_abst* p_cur = p_head_;
			while ( p_cur->p_next_ != nullptr ) {
				p_cur = p_cur->p_next_;
			}
			p_cur->p_next_ = p_node;
		}
	}
	bool unlock_recycle_one( void )
	{
		if ( p_head_ == nullptr ) {
			return false;
		}

		if ( internal::global_scope_hazard_ptr_chain::CheckPtrIsHazardPtr( p_head_->get_retire_pointer() ) ) {
			// because pointer is in hazard pointer list, therefore recycle is impossible for head
			return false;
		}

		retire_node_abst* p_parge = p_head_;
		p_head_                   = p_parge->p_next_;

		delete p_parge;

		return true;
	}

	std::mutex        mtx_;
	retire_node_abst* p_head_;
};

/**
 * @brief スレッドローカルストレージ専用のretire pointer管理クラス
 *
 * スレッドローカルストレージで宣言されることを前提としているため、このクラスは排他制御を行わない。
 *
 * @note
 * このクラスのインスタンスが終了するとき、まだ保持しているretire pointerが残っている場合、
 * グローバルストレージ専用クラスにリストを移管してから、デストラクタを終了する。
 */
class alignas( internal::atomic_variable_align ) thread_local_retire_mgr {
public:
	explicit constexpr thread_local_retire_mgr( global_retire_mgr& transfer_distination_arg ) noexcept
	  : transfer_distination_( transfer_distination_arg )
	  , p_head_( nullptr )
	{
	}

	~thread_local_retire_mgr()
	{
		while ( recycle_one() ) {}
		if ( p_head_ == nullptr ) {
			return;
		}

		transfer_distination_.transfer( p_head_ );
		p_head_ = nullptr;
	}

	void retire( retire_node_abst* p_new_retire )
	{
		recycle_one();

		if ( p_head_ == nullptr ) {
			p_head_ = p_new_retire;
		} else {
			// add new node to last
			retire_node_abst* p_cur = p_head_;
			while ( p_cur->p_next_ != nullptr ) {
				p_cur = p_cur->p_next_;
			}
			p_cur->p_next_ = p_new_retire;
		}

		recycle_one();
	}

	/**
	 * @brief retireしたポインタの１つを遅延廃棄を行う。
	 *
	 * 削除順は、fifoの順となる。
	 *
	 * @return true 廃棄を実行した。
	 * @return false 廃棄を実行できなかった。
	 *
	 * @note
	 * 現時点では、協調削除の方式をとっている。そのため、削除コストはretireを行った全スレッドで負担する構造。
	 *
	 * @todo
	 * 削除専用スレッドの対応
	 *
	 * @warning retireが呼ばれるか、このI/Fを意図的に呼び出さない限り、削除が行われないため、使い方によっては、スレッド終了まで削除が行われない現象が起きうる。
	 */
	bool recycle_one( void )
	{
		if ( p_head_ == nullptr ) {
			auto locker_handle = transfer_distination_.try_lock();
			if ( locker_handle.owns_lock() ) {
				locker_handle.recycle_one();
			}
			return false;
		}

		if ( internal::global_scope_hazard_ptr_chain::CheckPtrIsHazardPtr( p_head_->get_retire_pointer() ) ) {
			auto locker_handle = transfer_distination_.try_lock();
			if ( locker_handle.owns_lock() ) {
				retire_node_abst* p_transfer = p_head_;
				p_head_                      = p_transfer->p_next_;
				p_transfer->p_next_          = nullptr;   // important!!
				locker_handle.transfer( p_transfer );
			}
			// because pointer is in hazard pointer list, therefore recycle is impossible for head
			return false;
		}

		retire_node_abst* p_parge = p_head_;
		p_head_                   = p_parge->p_next_;

		delete p_parge;

		return true;
	}

private:
	global_retire_mgr& transfer_distination_;
	retire_node_abst*  p_head_;
};

///////////////////////////////////////////////////////////////////////////////////////
ALCC_INTERNAL_CONSTINIT global_retire_mgr                    g_retire_mgr_inst;
thread_local ALCC_INTERNAL_CONSTINIT thread_local_retire_mgr tl_reitre_mgr_insts { g_retire_mgr_inst };

void retire_mgr::prune( void )
{
	tl_reitre_mgr_insts.recycle_one();
	auto locker_handle = g_retire_mgr_inst.try_lock();
	if ( locker_handle.owns_lock() ) {
		while ( locker_handle.recycle_one() ) {}
	}
}

void retire_mgr::retire( retire_node_abst* p_new_retire )
{
	tl_reitre_mgr_insts.retire( p_new_retire );
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
