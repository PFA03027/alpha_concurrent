/*!
 * @file	hazard_ptr.hpp
 * @brief
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

#define NUM_OF_PRE_ALLOCATED_NODES ( 0 )   //!< 事前に用意する管理ノード数。空きノードがあれば、それを流用するので、mallocが発生しない。

namespace alpha {
namespace concurrent {

/*!
 * @breif	hazard pointer support class
 *
 * 削除権が、１つのインスタンスのみに制約できる場合に使用できるハザードポインタ。
 * 参照権に対しては、制限なし。ただし、参照が終了した場合は、ハザードポインタをクリアしなければならない。
 * 参照権のクリアには、clear_hazard_ptr()を使用する。
 *
 * ハザードポインタとして登録したポインタが指し示すオブジェクトは、delete演算子によって削除される。
 * ただし、delete演算子による削除は非同期で行われるため、try_delete_instance()を呼び出したとしても、その時点でdelete演算子による削除が行われるわけではない。
 * そのため、デストラクタによるリソース開放を行うクラスがテンプレートパラメータとして指定される場合、リソース開放が遅延されても、新たなリソース確保側と衝突を起こさない構成である必要がある。
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
			printf( "Error: the requested index is over max index." );
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
	struct node_for_hazard_ptr {
		T* p_target_[N];

		node_for_hazard_ptr( void )
		  : status_( ocupied_status::USING )
		  , next_( nullptr )
		{
			clear_hazard_ptr_all();
		}

		void set_hazard_ptr( T* p_target_arg, int idx )
		{
			p_target_[idx] = p_target_arg;
			std::atomic_thread_fence( std::memory_order_release );
			return;
		}

		void clear_hazard_ptr( int idx )
		{
			p_target_[idx] = nullptr;
			std::atomic_thread_fence( std::memory_order_release );
			return;
		}

		void clear_hazard_ptr_all( void )
		{
			for ( auto& e : p_target_ ) {
				e = nullptr;
				std::atomic_thread_fence( std::memory_order_release );
			}
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

			for ( auto& e : p_target_ ) {
				std::atomic_thread_fence( std::memory_order_acquire );
				if ( e == p_chk_ptr ) return true;
			}

			return false;
		}

		node_for_hazard_ptr* get_next( void )
		{
			return next_.load();
		}

		void set_next( node_for_hazard_ptr* p_new_next )
		{
			next_.store( p_new_next );
			return;
		}

		bool next_CAS( node_for_hazard_ptr** pp_expect_ptr, node_for_hazard_ptr* p_desired_ptr )
		{
			return next_.compare_exchange_weak( *pp_expect_ptr, p_desired_ptr );
		}

		static void destr_fn( void* parm )
		{
			//			printf( "thread local destructor now being called -- " );

			if ( parm == nullptr ) return;   // なぜかnullptrで呼び出された。多分pthread内でのrace conditionのせい。

			node_for_hazard_ptr* p_target_node = reinterpret_cast<node_for_hazard_ptr*>( parm );

			p_target_node->release_owner();

			//			printf( "thread local destructor is done.\n" );
			return;
		}

		void release_owner( void )
		{
			clear_hazard_ptr_all();
			status_.store( ocupied_status::UNUSED );
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
			return status_.load();
		}

	private:
		std::atomic<ocupied_status>       status_;
		std::atomic<node_for_hazard_ptr*> next_;
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

		static hazard_node_head& get_instance( void )
		{
			static hazard_node_head singleton;
			return singleton;
		}

		node_for_hazard_ptr* allocate_hazard_ptr_node( void )
		{
			// 空きノードを探す。
			node_for_hazard_ptr* p_ans = head_.load();
			while ( p_ans != nullptr ) {
				if ( p_ans->get_status() == ocupied_status::UNUSED ) {
					if ( p_ans->try_to_get_owner() ) {
						//						printf( "node is allocated.\n" );
						return p_ans;
					}
				}
				p_ans = p_ans->get_next();
			}

			// 空きノードが見つからなかったので、新しいノードを用意する。
			p_ans = add_one_new_hazard_ptr_node();

			//			printf( "glist is added.\n" );
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
			node_for_hazard_ptr* p_ans = head_.load();
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
			return node_count_.load();
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
			node_for_hazard_ptr* p_next_check = head_.load();
			bool                 cas_success  = false;
			do {
				p_ans->set_next( p_next_check );
				cas_success = head_.compare_exchange_strong( p_next_check, p_ans );
			} while ( !cas_success );   // CASが成功するまで繰り返す。
			node_count_++;

			//			printf( "glist is added.\n" );
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
		node_for_hazard_ptr* p_ans = p_hzd_ptr_node__.get_tls_instance(nullptr);
		if ( p_ans == nullptr ) {
			p_ans                               = get_head_instance().allocate_hazard_ptr_node();
			p_hzd_ptr_node__.get_tls_instance() = p_ans;
		}
		return p_ans;
	}

	dynamic_tls<node_for_hazard_ptr*> p_hzd_ptr_node__;
};

/*!
 * @breif	scoped reference control support class for hazard_ptr
 *
 * スコープベースでの、参照権の解放制御をサポートするクラス。
 * 削除権を確保する場合は、スコープアウトする前に、try_delete_instance()を呼び出すこと。
 */
template <typename T, int N>
class hazard_ptr_scoped_ref {
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

private:
	int               idx_;
	hazard_ptr<T, N>& monitor_ref_;
};

/*!
 * @breif	推論補助
 */
template <class HZD_PTR>
hazard_ptr_scoped_ref( HZD_PTR&, int ) -> hazard_ptr_scoped_ref<typename HZD_PTR::hzrd_type, HZD_PTR::hzrd_max_slot>;

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_HAZARD_PTR_HPP_ */