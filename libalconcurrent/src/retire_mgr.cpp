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
#include <condition_variable>
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
#include "alconcurrent/internal/retire_mgr.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

class retire_node_list {
public:
	constexpr retire_node_list( void ) noexcept
	  : p_head_( nullptr )
	  , p_tail_( nullptr )
	{
	}
	explicit constexpr retire_node_list( retire_node_abst* p_node ) noexcept
	  : p_head_( p_node )
	  , p_tail_( find_tail_pointer( p_node ) )
	{
	}
	retire_node_list( retire_node_list&& src ) noexcept
	  : p_head_( src.p_head_ )
	  , p_tail_( src.p_tail_ )
	{
		src.p_head_ = nullptr;
		src.p_tail_ = nullptr;
	}

	~retire_node_list()
	{
		clear();
	}

	void merge_push_back( retire_node_abst* p_node )
	{
		if ( p_node == nullptr ) return;

		// add new node to last
		if ( p_head_ == nullptr ) {
			p_head_ = p_node;
		} else {
			p_tail_->p_next_ = p_node;
		}
		p_tail_ = find_tail_pointer( p_node );
	}
	void merge_push_back( retire_node_list&& src )
	{
		if ( src.p_head_ == nullptr ) return;

		if ( p_head_ == nullptr ) {
			p_head_ = src.p_head_;
			p_tail_ = src.p_tail_;
		} else {
			p_tail_->p_next_ = src.p_head_;
			p_tail_          = src.p_tail_;
		}
		src.p_head_ = nullptr;
		src.p_tail_ = nullptr;
	}
	void clear( void ) noexcept
	{
		retire_node_abst* p_cur = p_head_;
		p_head_                 = nullptr;
		p_tail_                 = nullptr;
		while ( p_cur != nullptr ) {
			retire_node_abst* p_nxt = p_cur->p_next_;
			delete p_cur;
			p_cur = p_nxt;
		}
	}

	bool recycle_head_one( void ) noexcept
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
		if ( p_head_ == nullptr ) {
			p_tail_ = nullptr;
		}

		delete p_parge;

		return true;
	}

	bool is_empty( void ) const noexcept
	{
		return p_head_ == nullptr;
	}

private:
	static constexpr retire_node_abst* find_tail_pointer( retire_node_abst* p_head_arg ) noexcept
	{
		if ( p_head_arg == nullptr ) return nullptr;

		retire_node_abst* p_cur = p_head_arg;
		while ( p_cur->p_next_ != nullptr ) {
			p_cur = p_cur->p_next_;
		}
		return p_cur;
	}

	retire_node_abst* p_head_;
	retire_node_abst* p_tail_;
};

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

		retire_node_list& ref( void )
		{
			if ( !owns_lock() ) {
				throw std::logic_error( "Need to get mutex lock" );
			}
			return target_node_list_;
		}

		void wait_notify( void )
		{
			if ( !owns_lock() ) {
				throw std::logic_error( "Need to get mutex lock" );
			}
			parent_.cv.wait( ul_ );
		}

		~locker()
		{
			if ( !target_node_list_.is_empty() ) {
				parent_.cv.notify_all();
			}
		}

	private:
		locker( global_retire_mgr& parent_arg, retire_node_list& target_node_list_arg )
		  : parent_( parent_arg )
		  , target_node_list_( target_node_list_arg )
		  , ul_( parent_.mtx_ )
		{
		}
		locker( global_retire_mgr& parent_arg, retire_node_list& target_node_list_arg, std::try_to_lock_t )
		  : parent_( parent_arg )
		  , target_node_list_( target_node_list_arg )
		  , ul_( parent_.mtx_, std::try_to_lock )
		{
		}

		global_retire_mgr&           parent_;
		retire_node_list&            target_node_list_;
		std::unique_lock<std::mutex> ul_;

		friend class global_retire_mgr;
	};

	global_retire_mgr( void ) noexcept
	  : mtx_()
	  , cv()
	  , retire_list_()
	  , ap_lockfree_head_( nullptr )
	{
	}

	~global_retire_mgr()
	{
		// delete all without any check
		retire_node_list { ap_lockfree_head_.load( std::memory_order_acquire ) };
		ap_lockfree_head_.store( nullptr, std::memory_order_release );
	}

	locker try_lock( void )
	{
		return locker( *this, retire_list_, std::try_to_lock );
	}

	locker lock( void )
	{
		return locker( *this, retire_list_ );
	}

	void push_lockfree( retire_node_abst* p_list_head )
	{
		if ( p_list_head == nullptr ) return;

		retire_node_abst* p_expect = ap_lockfree_head_.load( std::memory_order_acquire );
		p_list_head->p_next_       = p_expect;
		while ( !ap_lockfree_head_.compare_exchange_weak( p_expect, p_list_head, std::memory_order_release, std::memory_order_relaxed ) ) {
			p_list_head->p_next_ = p_expect;
		}
		cv.notify_all();
	}

	/**
	 * @brief ロックフリーリストの先頭へ、１つのノードの追加を試みる
	 *
	 * @param p_list_head retire_node_abstのリストの先頭ノードへのポインタ
	 * @return retire_node_abst* 追加が成功した場合、p_list_headの次のノードのポインタを返す。追加に失敗した場合、p_list_headを返す
	 */
	retire_node_abst* push_lockfree_one_try( retire_node_abst* const p_list_head )
	{
		if ( p_list_head == nullptr ) return nullptr;

		retire_node_abst* const p_next_of_head = p_list_head->p_next_;

		retire_node_abst* p_expect = ap_lockfree_head_.load( std::memory_order_acquire );
		p_list_head->p_next_       = p_expect;
		if ( ap_lockfree_head_.compare_exchange_weak( p_expect, p_list_head, std::memory_order_release, std::memory_order_relaxed ) ) {
			cv.notify_all();
			return p_next_of_head;
		}

		p_list_head->p_next_ = p_next_of_head;
		return p_list_head;
	}

	retire_node_abst* parge_lockfree( void )
	{
		retire_node_abst* p_expect = ap_lockfree_head_.load( std::memory_order_acquire );
		while ( !ap_lockfree_head_.compare_exchange_weak( p_expect, nullptr, std::memory_order_release, std::memory_order_relaxed ) ) {
		}
		return p_expect;
	}

	static void prune_thread( void );
	static void notify_prune_thread( void );

private:
	std::mutex              mtx_;
	std::condition_variable cv;
	retire_node_list        retire_list_;

	alignas( internal::atomic_variable_align ) std::atomic<retire_node_abst*> ap_lockfree_head_;
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
		// スレッドローカルストレージを参照しているようなdeleterの場合、スレッドが終了時にはdeleterが正常に動作しなくなるため、
		// 単純にグローバルストレージに移管することだけを行う。
		if ( p_head_ == nullptr ) return;

		transfer_distination_.lock().ref().merge_push_back( p_head_ );
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

#ifdef ALCONCURRENT_CONF_ENABLE_PRUNE_THREAD
#else
		recycle_one();
#endif
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
#ifdef ALCONCURRENT_CONF_ENABLE_PRUNE_THREAD
		if ( p_head_ == nullptr ) {
			return false;
		}

		auto locker_handle = transfer_distination_.try_lock();
		if ( locker_handle.owns_lock() ) {
			locker_handle.ref().merge_push_back( p_head_ );
			p_head_ = nullptr;
			return true;
		}

		retire_node_abst* p_orig_head = p_head_;
		p_head_                       = transfer_distination_.push_lockfree_one_try( p_head_ );
		return ( p_head_ == p_orig_head ) ? false : true;
#else
		// pruneスレッドに任せるのではなく、個々のスレッドと共同でリサイクルを行う場合のアルゴリズム。
		if ( p_head_ == nullptr ) {
			auto locker_handle = transfer_distination_.try_lock();
			if ( locker_handle.owns_lock() ) {
				locker_handle.ref().recycle_head_one();
			}
			return false;
		}

		if ( internal::global_scope_hazard_ptr_chain::CheckPtrIsHazardPtr( p_head_->get_retire_pointer() ) ) {
			auto locker_handle = transfer_distination_.try_lock();
			if ( locker_handle.owns_lock() ) {
				retire_node_abst* p_transfer = p_head_;
				p_head_                      = p_transfer->p_next_;
				p_transfer->p_next_          = nullptr;   // important!!
				locker_handle.ref().merge_push_back( p_transfer );
			}
			// because pointer is in hazard pointer list, therefore recycle is impossible for head
			return false;
		}

		retire_node_abst* p_parge = p_head_;
		p_head_                   = p_parge->p_next_;

		delete p_parge;

		return true;
#endif
	}

private:
	global_retire_mgr& transfer_distination_;
	retire_node_abst*  p_head_;
};

///////////////////////////////////////////////////////////////////////////////////////
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
ALCC_INTERNAL_CONSTINIT alignas( internal::atomic_variable_align ) std::atomic<size_t> g_call_count_retire( 0 );
#endif

class prune_thread_inst_t {
public:
	prune_thread_inst_t( void )
	  : prune_thread_obj_()
	{
	}

	void start( void )
	{
		prune_thread_obj_ = std::thread( retire_mgr::prune_thread );
	}

	void stop( void )
	{
		retire_mgr::request_stop_prune_thread();
		if ( prune_thread_obj_.joinable() ) {
			prune_thread_obj_.join();
		}
	}

	~prune_thread_inst_t()
	{
		retire_mgr::request_stop_prune_thread();
		if ( prune_thread_obj_.joinable() ) {
			prune_thread_obj_.join();
		}
	}

	void increment_call_count( void )
	{
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
		g_call_count_retire++;
#endif
	}

private:
	std::thread prune_thread_obj_;
};

static prune_thread_inst_t g_prune_thread_obj;

class prune_thread_mgr {
public:
	prune_thread_mgr( void )
	{
		g_prune_thread_obj.start();
	}

	~prune_thread_mgr()
	{
		g_prune_thread_obj.stop();
	}

	void increment_call_count( void )
	{
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
		g_call_count_retire++;
#endif
	}

private:
	std::thread prune_thread_obj_;
};

///////////////////////////////////////////////////////////////////////////////////////
ALCC_INTERNAL_CONSTINIT global_retire_mgr                    g_retire_mgr_inst;
thread_local ALCC_INTERNAL_CONSTINIT thread_local_retire_mgr tl_reitre_mgr_insts { g_retire_mgr_inst };

void retire_mgr::retire_impl( retire_node_abst* p_new_retire )
{
#ifdef ALCONCURRENT_CONF_ENABLE_PRUNE_THREAD
	static prune_thread_mgr pt_mgr_obj;
	pt_mgr_obj.increment_call_count();
#else
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	g_call_count_retire++;
#endif
#endif

	tl_reitre_mgr_insts.retire( p_new_retire );
}

void global_retire_mgr::prune_thread( void )
{
	retire_node_list recycle_list;
	{
		auto lk = g_retire_mgr_inst.lock();
		lk.wait_notify();

		recycle_list.merge_push_back( std::move( lk.ref() ) );
	}

	recycle_list.merge_push_back( g_retire_mgr_inst.parge_lockfree() );

	while ( recycle_list.recycle_head_one() ) {}

	g_retire_mgr_inst.lock().ref().merge_push_back( std::move( recycle_list ) );
}

void global_retire_mgr::notify_prune_thread( void )
{
	g_retire_mgr_inst.cv.notify_all();
}

std::atomic<bool> retire_mgr::loop_flag_prune_thread_( true );

void retire_mgr::prune_thread( void )
{
	loop_flag_prune_thread_.store( true );
	while ( loop_flag_prune_thread_.load() ) {
		global_retire_mgr::prune_thread();
	}
}

void retire_mgr::request_stop_prune_thread( void )
{
	loop_flag_prune_thread_.store( false );
	global_retire_mgr::notify_prune_thread();
}

void retire_mgr::stop_prune_thread( void )
{
	g_prune_thread_obj.stop();
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
