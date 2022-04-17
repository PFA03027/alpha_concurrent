/*!
 * @file	hazard_ptr.hpp
 * @brief	hazard pointer
 * @author	Teruaki Ata
 * @date	Created on 2020/12/24
 * @details
 *
 *  US Patent of Hazard pointer algrithm: US20040107227A1 is now abandoned.
 *  https://patents.google.com/patent/US20040107227
 *
 * Copyright (C) 2020 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef SRC_HAZARD_PTR_HPP_
#define SRC_HAZARD_PTR_HPP_

#include <pthread.h>

#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <thread>

#include "dynamic_tls.hpp"

#define NUM_OF_PRE_ALLOCATED_NODES ( 0 )   //!< 事前に用意する管理ノード数。なお、空きノードがあれば、それを流用するので、mallocが発生しない。
#define ENABLE_ATOMIC_HAZARD_POINTER       //!< hazard pointerをatomic<T*>で管理する。ThreadSanitizerの誤検知対策。

namespace alpha {
namespace concurrent {

/*!
 * @breif	hazard pointer support class
 *
 * 削除権が、１つのインスタンスのみに制約できる場合に使用できるハザードポインタ。
 * 参照権に対しては、制限なし。ただし、参照が終了した場合は、ハザードポインタをクリアしなければならない。
 * 参照権のクリアには、clear_hazard_ptr()を使用する。
 *
 * （ハザードポインタ自身はlock-freeであるが）lock-freeとして動作するには、テンプレートパラメータで指定されたクラスのデストラクタがlock-freeでなければならない。
 *
 * インスタンスが削除される前に、アクセスするスレッドは、参照権の破棄としてrelease_owner()を呼び出すこと。
 *
 */
template <typename T, int N>
class hazard_ptr {
public:
	using hzrd_type                    = T;
	using hzrd_pointer                 = T*;
	static constexpr int hzrd_max_slot = N;

	hazard_ptr( void )
	  : p_hzd_ptr_node_( threadlocal_destructor_functor() )
	{
	}

	~hazard_ptr( void )
	{
	}

	/*!
	 * @breif	Make a reservation to acquire the reference right.
	 *
	 * 参照権を獲得予約をするために、ハザードポインタに登録する。
	 *
	 * 本I/Fを呼び出しただけでは、参照権を獲得したことにはならない。 @n
	 * 本I/Fを呼び出した後に、登録したポインタがまだ有効であるかどうかを確認すること。 @n
	 * @li	確認の結果、有効である場合は、参照権を獲得したとみなせる。
	 * @li	確認の結果、有効ではなかった場合、clear_hazard_ptr()を呼び出すか、新しいポインタを新たなハザードポインタとして本I/Fで再設定し、現在保持しているハザードポインタを解放すること。
	 *
	 * @note
	 * 確実なclear_hazard_ptr()呼び出しをサポートするクラスとして、 hazard_ptr_scoped_ref<T> を用意している。
	 */
	inline void regist_ptr_as_hazard_ptr( T* p_target, int idx )
	{
		if ( idx >= N ) {
			internal::LogOutput( log_type::ERR, "Error: the requested index is over max index." );
			return;
		}

		node_for_hazard_ptr* p_hzd_ptr_node_ = check_local_storage();

		p_hzd_ptr_node_->set_hazard_ptr( p_target, idx );
	}

	/*!
	 * @breif	Release a reservation to acquire the reference right or the reference right itself.
	 *
	 * @note
	 * 確実なclear_hazard_ptr()呼び出しをサポートするクラスとして、 hazard_ptr_scoped_ref<T> を用意している。
	 */
	inline void clear_hazard_ptr( int idx )
	{
		node_for_hazard_ptr* p_hzd_ptr_node_ = check_local_storage();
		p_hzd_ptr_node_->clear_hazard_ptr( idx );
	}

	/*!
	 * @breif	Release a reservation to acquire the reference right or the reference right itself.
	 *
	 * @note
	 * 確実なclear_hazard_ptr()呼び出しをサポートするクラスとして、 hazard_ptr_scoped_ref<T> を用意している。
	 */
	inline void clear_hazard_ptr_all( void )
	{
		node_for_hazard_ptr* p_hzd_ptr_node_ = check_local_storage();
		p_hzd_ptr_node_->clear_hazard_ptr_all();
	}

	/*!
	 * @breif	Check whether a pointer is in this hazard list
	 *
	 * @retval	true	p_chk_ptr is in this hazard list.
	 * @retval	false	p_chk_ptr is not in this hazard list.
	 */
	bool check_ptr_in_hazard_list( T* p_chk_ptr )
	{
		return get_head_instance().check_ptr_in_hazard_list( p_chk_ptr );
	}

	int debug_get_glist_size( void )
	{
		return get_head_instance().get_node_count();
	}

private:
	enum class ocupied_status {
		UNUSED,
		USING
	};

	/*!
	 * @breif	ハザードポインタとしてキープするためのノード。
	 *
	 * スレッド毎に確保され、使用権が占有される。
	 *
	 * @note
	 * このノードの集合を管理するリストは、ノード自体を削除することを行わない設計となっている。
	 *
	 */
	class node_for_hazard_ptr {
	public:
		node_for_hazard_ptr( void )
		  : status_( ocupied_status::USING )
		  , next_( nullptr )
#ifdef ENABLE_ATOMIC_HAZARD_POINTER
#else
		  , guard_val_( 0 )
#endif
		{
			clear_hazard_ptr_all();
		}

		void set_hazard_ptr( T* p_target_arg, int idx )
		{
#ifdef ENABLE_ATOMIC_HAZARD_POINTER
			p_target_[idx].store( p_target_arg, std::memory_order_release );
#else
			p_target_[idx] = p_target_arg;
			guard_val_.store( true, std::memory_order_release );
#endif
			return;
		}

		void clear_hazard_ptr( int idx )
		{
#ifdef ENABLE_ATOMIC_HAZARD_POINTER
			p_target_[idx].store( nullptr, std::memory_order_release );
#else
			p_target_[idx] = nullptr;
			guard_val_.store( true, std::memory_order_release );
#endif
			return;
		}

		void clear_hazard_ptr_all( void )
		{
#ifdef ENABLE_ATOMIC_HAZARD_POINTER
			for ( auto& e : p_target_ ) {
				e.store( nullptr, std::memory_order_release );
			}
#else
			for ( auto& e : p_target_ ) {
				e = nullptr;
			}
			guard_val_.store( true, std::memory_order_release );
#endif
			return;
		}

		/*!
		 * @breif	Check whether a pointer is in this hazard
		 *
		 * @retval	true	p_chk_ptr is in this hazard.
		 * @retval	false	p_chk_ptr is not in this hazard.
		 */
		inline bool check_hazard_ptr( T* p_chk_ptr )
		{
			if ( get_status() == ocupied_status::UNUSED ) return false;

#ifdef ENABLE_ATOMIC_HAZARD_POINTER
			for ( auto& e : p_target_ ) {
				if ( e.load( std::memory_order_acquire ) == p_chk_ptr ) return true;
			}
#else
			guard_val_.load( std::memory_order_acquire );
			for ( auto& e : p_target_ ) {
				if ( e == p_chk_ptr ) return true;
			}
#endif

			return false;
		}

		node_for_hazard_ptr* get_next( void )
		{
			return next_.load( std::memory_order_acquire );
		}

		void set_next( node_for_hazard_ptr* p_new_next )
		{
			next_.store( p_new_next, std::memory_order_release );
			return;
		}

		bool next_CAS( node_for_hazard_ptr** pp_expect_ptr, node_for_hazard_ptr* p_desired_ptr )
		{
			return next_.compare_exchange_weak( *pp_expect_ptr, p_desired_ptr );
		}

		void release_owner( void )
		{
			clear_hazard_ptr_all();
			status_.store( ocupied_status::UNUSED, std::memory_order_release );
			return;
		}

		bool try_to_get_owner( void )
		{
			ocupied_status cur;
			cur = get_status();
			if ( cur != ocupied_status::UNUSED ) {
				return false;
			}

			return status_.compare_exchange_strong( cur, ocupied_status::USING );
		}

		ocupied_status get_status( void )
		{
			return status_.load( std::memory_order_acquire );
		}

	private:
		std::atomic<ocupied_status>       status_;   //!< status for used or not used yet
		std::atomic<node_for_hazard_ptr*> next_;     //!< pointer to next node
#ifdef ENABLE_ATOMIC_HAZARD_POINTER
		std::atomic<T*> p_target_[N];   //!< hazard pointer strage
#else
		std::atomic<bool> guard_val_;     //!< atomic variable of fence to sync below hazard pointers. value itself is no meaning.
		T*                p_target_[N];   //!< hazard pointer strage
#endif
	};

	////////////////////////////////////////////////////////////////////////////////
	struct hazard_node_head {
		hazard_node_head( void )
		  : head_( nullptr )
		  , node_count_( 0 )
		{
			for ( int i = 0; i < NUM_OF_PRE_ALLOCATED_NODES; i++ ) {
				auto p_hzrd_ptr_node = add_one_new_hazard_ptr_node();
				p_hzrd_ptr_node->release_owner();
			}
		}
		~hazard_node_head()
		{
			node_for_hazard_ptr* p_cur = head_.load( std::memory_order_acquire );
			while ( p_cur != nullptr ) {
				node_for_hazard_ptr* p_nxt = p_cur->get_next();
				delete p_cur;
				p_cur = p_nxt;
			}
			return;
		}

		static hazard_node_head& get_instance( void )
		{
			static hazard_node_head singleton;
			return singleton;
		}

		node_for_hazard_ptr* allocate_hazard_ptr_node( void )
		{
			// 空きノードを探す。
			node_for_hazard_ptr* p_ans = head_.load( std::memory_order_acquire );
			while ( p_ans != nullptr ) {
				if ( p_ans->get_status() == ocupied_status::UNUSED ) {
					if ( p_ans->try_to_get_owner() ) {
						internal::LogOutput( log_type::DEBUG, "node is allocated." );
						return p_ans;
					}
				}
				p_ans = p_ans->get_next();
			}

			// 空きノードが見つからなかったので、新しいノードを用意する。
			p_ans = add_one_new_hazard_ptr_node();

			internal::LogOutput( log_type::DEBUG, "glist is added." );
			return p_ans;
		}

		/*!
		 * @breif	Check whether a pointer is in this hazard list
		 *
		 * @retval	true	p_chk_ptr is in this hazard list.
		 * @retval	false	p_chk_ptr is not in this hazard list.
		 */
		bool check_ptr_in_hazard_list( T* p_chk_ptr )
		{
			node_for_hazard_ptr* p_ans = head_.load( std::memory_order_acquire );
			while ( p_ans != nullptr ) {
				if ( p_ans->get_status() == ocupied_status::USING ) {
					if ( p_ans->check_hazard_ptr( p_chk_ptr ) ) {
						return true;
					}
				}
				p_ans = p_ans->get_next();
			}
			return false;
		}

		int get_node_count( void )
		{
			return node_count_.load( std::memory_order_acquire );
		}

	private:
		/*!
		 * @breif	新しいノードを用意し、リストに追加する。
		 *
		 * 追加したノードは、USING状態で返される。
		 *
		 * @return	追加した新しいノードへのポインタ
		 */
		node_for_hazard_ptr* add_one_new_hazard_ptr_node( void )
		{
			node_for_hazard_ptr* p_ans        = new node_for_hazard_ptr();
			node_for_hazard_ptr* p_next_check = head_.load( std::memory_order_acquire );
			bool                 cas_success  = false;
			do {
				p_ans->set_next( p_next_check );
				cas_success = head_.compare_exchange_strong( p_next_check, p_ans );
			} while ( !cas_success );   // CASが成功するまで繰り返す。
			node_count_++;

			internal::LogOutput( log_type::DEBUG, "glist is added." );
			return p_ans;
		}

		hazard_node_head( const hazard_node_head& ) = delete;
		hazard_node_head( hazard_node_head&& )      = delete;
		hazard_node_head& operator=( const hazard_node_head& ) = delete;
		hazard_node_head& operator=( hazard_node_head&& ) = delete;

		std::atomic<node_for_hazard_ptr*> head_;
		std::atomic<int>                  node_count_;
	};

	hazard_node_head head_;

	hazard_node_head& get_head_instance( void )
	{
		return head_;
	}

	inline node_for_hazard_ptr* check_local_storage( void )
	{
		node_for_hazard_ptr* p_ans = p_hzd_ptr_node_.get_tls_instance( nullptr );
		if ( p_ans == nullptr ) {
			p_ans                              = get_head_instance().allocate_hazard_ptr_node();
			p_hzd_ptr_node_.get_tls_instance() = p_ans;
		}
		return p_ans;
	}

	struct threadlocal_destructor_functor {
		using value_type      = node_for_hazard_ptr*;
		using value_reference = value_type&;
		
		bool release( value_reference data )
		{
			data->release_owner();
			return true;
		}

		void destruct( value_reference data )
		{
			return;
		}
	};

	dynamic_tls<node_for_hazard_ptr*, threadlocal_destructor_functor> p_hzd_ptr_node_;
};

/*!
 * @breif	scoped reference control support class for hazard_ptr
 *
 * スコープベースでの、参照権の解放制御をサポートするクラスのI/Fクラス
 */
template <typename T>
class hazard_ptr_scoped_ref_if {
public:
	virtual void regist_ptr_as_hazard_ptr( T* p_target ) = 0;

	hazard_ptr_scoped_ref_if( void )    = default;
	virtual ~hazard_ptr_scoped_ref_if() = default;

private:
	hazard_ptr_scoped_ref_if( const hazard_ptr_scoped_ref_if& ) = delete;
	hazard_ptr_scoped_ref_if( hazard_ptr_scoped_ref_if&& )      = delete;
	hazard_ptr_scoped_ref_if& operator=( const hazard_ptr_scoped_ref_if& ) = delete;
	hazard_ptr_scoped_ref_if& operator=( hazard_ptr_scoped_ref_if&& ) = delete;
};

/*!
 * @breif	scoped reference control support class for hazard_ptr
 *
 * スコープベースでの、参照権の解放制御をサポートするクラスの実体定義クラス
 */
template <typename T, int N>
class hazard_ptr_scoped_ref : public hazard_ptr_scoped_ref_if<T> {
public:
	hazard_ptr_scoped_ref( hazard_ptr<T, N>& ref, int idx_arg )
	  : idx_( idx_arg )
	  , monitor_ref_( ref )
	{
	}

	~hazard_ptr_scoped_ref()
	{
		monitor_ref_.clear_hazard_ptr( idx_ );
	}

	void regist_ptr_as_hazard_ptr( T* p_target ) override
	{
		monitor_ref_.regist_ptr_as_hazard_ptr( p_target, idx_ );
	}

private:
	int               idx_;
	hazard_ptr<T, N>& monitor_ref_;
};

#if 0
/*!
 * @breif	推論補助
 */
template <class HZD_PTR>
hazard_ptr_scoped_ref( HZD_PTR&, int ) -> hazard_ptr_scoped_ref<typename HZD_PTR::hzrd_type, HZD_PTR::hzrd_max_slot>;
#endif
}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_HAZARD_PTR_HPP_ */
