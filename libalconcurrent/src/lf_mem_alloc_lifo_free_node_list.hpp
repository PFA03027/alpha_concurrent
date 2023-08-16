/**
 * @file lf_mem_alloc_lifo_free_node_list.hpp
 * @author Teruaki Ata <PFA03027@nifty.com>
 * @brief header file of lock-free free node strage(LIFO type)
 * @version 0.1
 * @date 2023-08-11
 *
 * @copyright Copyright (c) 2023 Teruaki Ata <PFA03027@nifty.com>
 *
 */

#ifndef LF_MEM_ALLOC_LIFO_FREE_NODE_LIST_HPP_
#define LF_MEM_ALLOC_LIFO_FREE_NODE_LIST_HPP_

#include <mutex>
#include <type_traits>

#include "alconcurrent/hazard_ptr.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

struct is_callable_lifo_free_node_if_impl {
	template <typename NODE_T>
	static auto check_get_next( NODE_T* ) -> decltype( std::declval<NODE_T&>().get_next(), std::true_type() );
	template <typename NODE_T>
	static auto check_get_next( ... ) -> std::false_type;

	template <typename NODE_T>
	static auto check_set_next( NODE_T* ) -> decltype( std::declval<NODE_T&>().set_next( std::declval<NODE_T*>() ), std::true_type() );
	template <typename NODE_T>
	static auto check_set_next( ... ) -> std::false_type;

	template <typename NODE_T>
	static auto check_next_CAS( NODE_T* ) -> decltype( std::declval<NODE_T&>().next_CAS( std::declval<NODE_T**>(), std::declval<NODE_T*>() ), std::true_type() );
	template <typename NODE_T>
	static auto check_next_CAS( ... ) -> std::false_type;

	template <typename NODE_T>
	static auto check( NODE_T* ) -> typename std::enable_if<decltype( check_get_next( std::declval<NODE_T*>() ) )::value&& decltype( check_set_next( std::declval<NODE_T*>() ) )::value&& decltype( check_next_CAS( std::declval<NODE_T*>() ) )::value, typename std::true_type>::type;
	template <typename NODE_T>
	static auto check( ... ) -> std::false_type;
};

template <typename NODE_T>
struct is_callable_lifo_free_node_if_get_next : decltype( is_callable_lifo_free_node_if_impl::check_get_next<NODE_T>( std::declval<NODE_T*>() ) ) {};
template <typename NODE_T>
struct is_callable_lifo_free_node_if_set_next : decltype( is_callable_lifo_free_node_if_impl::check_set_next<NODE_T>( std::declval<NODE_T*>() ) ) {};
template <typename NODE_T>
struct is_callable_lifo_free_node_if_next_CAS : decltype( is_callable_lifo_free_node_if_impl::check_next_CAS<NODE_T>( std::declval<NODE_T*>() ) ) {};
template <typename NODE_T>
struct is_callable_lifo_free_node_if : decltype( is_callable_lifo_free_node_if_impl::check<NODE_T>( std::declval<NODE_T*>() ) ) {};   // ノードクラスTに対するコンセプトチェックメタ関数

#ifdef PERFORMANCE_ANALYSIS_LOG1
extern std::atomic<size_t> call_count_push_to_free_node_stack;
extern std::atomic<size_t> spin_count_push_to_free_node_stack;
#endif

/**
 * @brief フリーノードをLIFO(スタック構造)で管理するクラス。
 *
 * @tparam NODE_T is_callable_lifo_free_node_if<NODE_T>::trueとなることが必要要件となるノードクラス
 */
template <typename NODE_T>
struct free_node_stack {
	static_assert( is_callable_lifo_free_node_if<NODE_T>::value, "NODE_T should have 3 I/Fs" );

	enum {
		HZD_IDX_POP_FUNC_HEAD = 0,
		HZD_IDX_POP_FUNC_NEXT,
		HZD_IDX_PUSH_FUNC_HEAD,
		HZD_IDX_MAX,
	};
	struct threadlocal_no_allocate_handler {
		threadlocal_no_allocate_handler( free_node_stack* p_parent_arg );

		/**
		 * @brief 型Tのオブジェクトを生成するファクトリ関数
		 */
		uintptr_t allocate( void )
		{
			return 0;
		}

		/*!
		 * @brief スレッド終了時の破棄処理を行う関数
		 */
		void deallocate(
			uintptr_t p_data   //!< [in] 設定されたポインタ値
		);

		free_node_stack* const p_parent_;
	};

	using node_type        = NODE_T;
	using node_pointer     = NODE_T*;
	using hzd_ptr_mgr_type = hazard_ptr<node_type, HZD_IDX_MAX>;
	using scoped_hzd_type  = hazard_ptr_scoped_ref<node_type, HZD_IDX_MAX>;

	free_node_stack( alloc_only_chamber* p_allocator_arg )
	  : hzd_ptrs_( p_allocator_arg )
	  , p_free_node_stack_head_( nullptr )
	  , mtx_consignment_stack_()
	  , p_consignment_stack_head_( nullptr )
	  , tls_p_hazard_slot_stack_head_( threadlocal_no_allocate_handler( this ) )
	{
		static_assert( std::is_standard_layout<free_node_stack>::value, "slot_array_mgr should be standard-layout type" );
	}

	/**
	 * @brief スタックリストを登録する。
	 *
	 * コンストラクタ内で、他スレッドとノードを共有していることはない。
	 * よって、効率的にスタックリストを登録するために、作成済みのスタックリストを一括登録する場合に使用するためのI/F。
	 *
	 * @param p_top スタックリストの先頭のノードへのポインタ
	 */
	void unchk_push_stack_list_to_head( node_pointer p_top )
	{
		p_free_node_stack_head_.store( p_top, std::memory_order_release );
	}

	/**
	 * @brief free node stackにノードを追加する。
	 *
	 * @param p_n ノードへのポインタ
	 * @return node_pointer p_nがまだハザードポインタに登録されている場合、pushできないため、p_nを返す。返されたp_nは、スレッドローカルストレージ一時的に保管する等の対応を行うこと。
	 */
	node_pointer push_to_free_node_stack( node_pointer p_n )
	{
		if ( hzd_ptrs_.check_ptr_in_hazard_list( p_n ) ) {
			// ハザードポインタに登録されているため、push不可能として、p_nを返す。
			return p_n;
		}

#ifdef PERFORMANCE_ANALYSIS_LOG1
		call_count_push_to_free_node_stack.fetch_add( 1, std::memory_order_acq_rel );
#endif

		scoped_hzd_type hzd_refing_p_head( hzd_ptrs_, HZD_IDX_PUSH_FUNC_HEAD );
		node_pointer    p_cur_head = p_free_node_stack_head_.load( std::memory_order_acquire );
		while ( true ) {
#ifdef PERFORMANCE_ANALYSIS_LOG1
			spin_count_push_to_free_node_stack.fetch_add( 1, std::memory_order_acq_rel );
#endif

			hzd_refing_p_head.regist_ptr_as_hazard_ptr( p_cur_head );
			node_pointer p_tmp_head = p_free_node_stack_head_.load( std::memory_order_acquire );
			if ( p_cur_head != p_tmp_head ) {
				p_cur_head = p_tmp_head;
				continue;
			}

			p_n->set_next( p_cur_head );
			// この処理後、別スレッドでset_nextで書き換わるとアウト。
			// そのため、事前にp_nがほかのスレッドで参照されていないことをハザードポインタ経由で確認する必要あり。

			if ( p_free_node_stack_head_.compare_exchange_weak( p_cur_head, p_n ) ) {
				break;
			}
		}

		return nullptr;
	}

	/**
	 * @brief free node stackからノードを取り出す。
	 *
	 * @return node_pointer 取り出されたノードへのポインタ。nullptrの場合、フリーノードがないことを示す。
	 */
	node_pointer pop_from_free_node_stack( void )
	{
		scoped_hzd_type hzd_refing_p_head( hzd_ptrs_, HZD_IDX_POP_FUNC_HEAD );
		scoped_hzd_type hzd_refing_p_next( hzd_ptrs_, HZD_IDX_POP_FUNC_NEXT );

		node_pointer p_new_head = nullptr;
		node_pointer p_cur_head = p_free_node_stack_head_.load( std::memory_order_acquire );   // この処理後、CASの前にp_cur_headに対してABAが発生するとアウト。p_cur_headはハザードポインタに確保し、p_cur_headに対応するnodeがpush経由でこの関数に入ってこないようにしないといけない。
		while ( true ) {                                                                       // do...while文でcontinueを使うとwhile文の条件式が実行されてしまう。期待した制御にはならないため、while(true)で実装する必要がある。
			hzd_refing_p_head.regist_ptr_as_hazard_ptr( p_cur_head );
			node_pointer p_node_tmp = p_free_node_stack_head_.load( std::memory_order_acquire );
			if ( p_cur_head != p_node_tmp ) {
				p_cur_head = p_node_tmp;
				continue;
			}
			if ( p_cur_head == nullptr ) return nullptr;

			p_new_head = p_cur_head->get_next();
			hzd_refing_p_next.regist_ptr_as_hazard_ptr( p_new_head );
			if ( p_new_head != p_cur_head->get_next() ) {
				continue;
			}
			if ( p_free_node_stack_head_.compare_exchange_weak( p_cur_head, p_new_head ) ) {
				// 置き換えに成功したら、p_cur_headを確保できたので、ループを抜ける
				break;
			}
		}
		p_cur_head->set_next( nullptr );   // p_cur_headが確保できたノードのポインタが入っている。また、p_cur_headが確保できたので、書き換えOK。

		return p_cur_head;
	}

	void push_to_tls_stack( node_pointer p_n )
	{
		node_pointer p_cur_head = tls_p_hazard_slot_stack_head_.get_tls_instance();
		p_n->set_next( p_cur_head );
		tls_p_hazard_slot_stack_head_.set_value_to_tls_instance( p_n );
	}

	node_pointer pop_from_tls_stack( void )
	{
		node_pointer p_ans = tls_p_hazard_slot_stack_head_.get_tls_instance();
		if ( p_ans == nullptr ) return nullptr;

		node_pointer p_new_head = p_ans->get_next();
		p_ans->set_next( nullptr );
		tls_p_hazard_slot_stack_head_.set_value_to_tls_instance( p_new_head );
		return p_ans;
	}

	void nonlockchk_push_to_consignment_stack( node_pointer p_n )
	{
		p_n->set_next( p_consignment_stack_head_ );
		p_consignment_stack_head_ = p_n;
	}

	node_pointer nonlockchk_pop_from_consignment_stack( void )
	{
		if ( p_consignment_stack_head_ == nullptr ) return nullptr;

		node_pointer p_ans        = p_consignment_stack_head_;
		p_consignment_stack_head_ = p_ans->get_next();
		p_ans->set_next( nullptr );
		return p_ans;
	}

	void push( node_pointer p_n )
	{
		// いったんリサイクルトライをしてから、、、
		{
			std::unique_lock<std::mutex> ul( mtx_consignment_stack_, std::defer_lock );

			node_pointer p_tmp_ret = pop_from_tls_stack();
			if ( p_tmp_ret != nullptr ) {
				p_tmp_ret = push_to_free_node_stack( p_tmp_ret );
				if ( p_tmp_ret != nullptr ) {
					// まだハザードポインタにいるようなので、戻す。
					if ( ul.try_lock() ) {
						nonlockchk_push_to_consignment_stack( p_tmp_ret );
					} else {
						// 共有ノードスタックのロックが確保できなかったので、スレッドローカルに戻す
						push_to_tls_stack( p_tmp_ret );
					}
				}
			}
			if ( !ul.owns_lock() ) {
				ul.try_lock();
			}
			if ( ul.owns_lock() ) {
				p_tmp_ret = nonlockchk_pop_from_consignment_stack();
				if ( p_tmp_ret != nullptr ) {
					p_tmp_ret = push_to_free_node_stack( p_tmp_ret );
					if ( p_tmp_ret != nullptr ) {
						// まだハザードポインタにいるようなので、戻す。
						nonlockchk_push_to_consignment_stack( p_tmp_ret );
					}
				}
			}
		}

		// push作業を開始する。
		node_pointer p_tmp_ret = push_to_free_node_stack( p_n );
		if ( p_tmp_ret != nullptr ) {
			// まだハザードポインタにいるようなので、共有スタックリストかスレッドローカルリストに戻す。
			std::unique_lock<std::mutex> ul( mtx_consignment_stack_, std::try_to_lock );
			if ( ul.owns_lock() ) {
				nonlockchk_push_to_consignment_stack( p_tmp_ret );
			} else {
				push_to_tls_stack( p_tmp_ret );
			}
		}
	}

	node_pointer pop( void )
	{
		// いったんリサイクルトライをしてから、、、
		{
			std::unique_lock<std::mutex> ul( mtx_consignment_stack_, std::defer_lock );

			node_pointer p_tmp_ret = pop_from_tls_stack();   // スレッドローカルリストからノードを取り出す
			if ( p_tmp_ret != nullptr ) {
				if ( hzd_ptrs_.check_ptr_in_hazard_list( p_tmp_ret ) ) {
					// まだハザードポインタにいるようなので、戻す。
					if ( ul.try_lock() ) {
						// 共有ノードスタックのロックが取得できたので、共有ノードスタックからフリーノードリストに戻せるか試す。
						node_pointer p_tmp_ret2 = nonlockchk_pop_from_consignment_stack();
						nonlockchk_push_to_consignment_stack( p_tmp_ret );   // スレッドローカルリストから取り出されたノードを先に共有ノードスタックに戻しておく。
						if ( p_tmp_ret2 != nullptr ) {
							p_tmp_ret2 = push_to_free_node_stack( p_tmp_ret2 );
							if ( p_tmp_ret2 != nullptr ) {
								// 共有ノードスタックから取り出したノードはまだハザードポインタにいるので、共有ノードスタックに戻す。
								nonlockchk_push_to_consignment_stack( p_tmp_ret2 );
							}
						}
					} else {
						// 共有ノードスタックのロックが確保できなかったので、スレッドローカルに戻す
						push_to_tls_stack( p_tmp_ret );
					}
				} else {
					// ハザードポインタにいないので、すでにfree_nodeとなっている。
					return p_tmp_ret;
				}
			}

			if ( !ul.owns_lock() ) {
				ul.try_lock();
			}
			if ( ul.owns_lock() ) {
				p_tmp_ret = nonlockchk_pop_from_consignment_stack();   // 共有ノードスタックからノードを取り出す。
				if ( p_tmp_ret != nullptr ) {
					if ( hzd_ptrs_.check_ptr_in_hazard_list( p_tmp_ret ) ) {
						// まだハザードポインタにいるようなので、戻す。
						nonlockchk_push_to_consignment_stack( p_tmp_ret );
					} else {
						// ハザードポインタにいないので、p_tmp_retはfree_nodeとなっている。
						return p_tmp_ret;
					}
				}
			}
		}

		// free nodeからpop作業を開始する。
		return pop_from_free_node_stack();
	}

	hzd_ptr_mgr_type                                           hzd_ptrs_;                       //!< ハザードポインタ管理構造体
	std::atomic<node_pointer>                                  p_free_node_stack_head_;         //!< 未使用状態のslot_header_of_arrayスタックの、先頭slot_header_of_arrayへのポインタ
	std::mutex                                                 mtx_consignment_stack_;          //!< スレッド終了時のハザード中のスロットを受け取るスタックの排他制御用mutex
	node_pointer                                               p_consignment_stack_head_;       //!< スレッド終了時のハザード中のスロットを受け取るスタックの、先頭slot_header_of_arrayへのポインタ
	dynamic_tls<node_pointer, threadlocal_no_allocate_handler> tls_p_hazard_slot_stack_head_;   //!< ハザード中のスロットを受け取るスレッド毎のスタックの、先頭slot_header_of_arrayへのポインタ
};

template <typename NODE_T>
free_node_stack<NODE_T>::threadlocal_no_allocate_handler::threadlocal_no_allocate_handler( free_node_stack* p_parent_arg )
  : p_parent_( p_parent_arg )
{
}

template <typename NODE_T>
void free_node_stack<NODE_T>::threadlocal_no_allocate_handler::deallocate( uintptr_t p_data )
{
	node_pointer p_n = reinterpret_cast<node_pointer>( p_data );
	if ( p_n == nullptr ) return;

	std::lock_guard<std::mutex> lg( p_parent_->mtx_consignment_stack_ );
	p_parent_->nonlockchk_push_to_consignment_stack( p_n );
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
