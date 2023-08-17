/**
 * @file lifo_free_node_stack.hpp
 * @author Teruaki Ata <PFA03027@nifty.com>
 * @brief header file of lock-free free node strage(LIFO type)
 * @version 0.1
 * @date 2023-08-11
 *
 * @copyright Copyright (c) 2023 Teruaki Ata <PFA03027@nifty.com>
 *
 */

#ifndef LIFO_FREE_NODE_STACK_HPP_
#define LIFO_FREE_NODE_STACK_HPP_

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
	 * @brief free node stackにノードを追加する。だたし、引数のp_nに対してハザードポインタチェックを行わない。
	 *
	 * @warning p_nがハザードポインタになっていないことを確認の上、呼び出すこと。
	 *
	 * @param p_n ノードへのポインタ。ハザードポインタになっていないこと。
	 */
	void push_to_free_node_stack_wo_hzd_chk( node_pointer p_n )
	{
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

		return;
	}

	/**
	 * @brief free node stackからノードを取り出す。
	 *
	 * @return node_pointer 取り出されたノードへのポインタ。nullptrの場合、フリーノードがないことを示す。
	 */
	node_pointer pop_from_free_node_stack( void )
	{
		scoped_hzd_type hzd_refing_p_head( hzd_ptrs_, HZD_IDX_POP_FUNC_HEAD );
		scoped_hzd_type hzd_refing_p_next( hzd_refing_p_head, HZD_IDX_POP_FUNC_NEXT );

		node_pointer p_new_head = nullptr;
		node_pointer p_cur_head = p_free_node_stack_head_.load( std::memory_order_acquire );   // この処理後、CASの前にp_cur_headに対してABAが発生するとアウト。p_cur_headはハザードポインタに確保し、p_cur_headに対応するnodeがpush経由でこの関数に入ってこないようにしないといけない。
		while ( p_cur_head != nullptr ) {                                                      // do...while文でcontinueを使うとwhile文の条件式が実行されてしまう。期待した制御にはならないため、while(true)で実装する必要がある。
			hzd_refing_p_head.regist_ptr_as_hazard_ptr( p_cur_head );
			node_pointer p_node_tmp = p_free_node_stack_head_.load( std::memory_order_acquire );
			if ( p_cur_head != p_node_tmp ) {
				p_cur_head = p_node_tmp;
				continue;
			}

			p_new_head = p_cur_head->get_next();
			hzd_refing_p_next.regist_ptr_as_hazard_ptr( p_new_head );
			if ( p_new_head != p_cur_head->get_next() ) {
				p_cur_head = p_free_node_stack_head_.load( std::memory_order_acquire );
				continue;
			}
			if ( p_free_node_stack_head_.compare_exchange_weak( p_cur_head, p_new_head ) ) {
				// 置き換えに成功したら、p_cur_headを確保できたので、ループを抜ける
				p_cur_head->set_next( nullptr );   // p_cur_headが確保できたノードのポインタが入っている。また、p_cur_headが確保できたので、書き換えOK。
				return p_cur_head;
			}
		}

		return nullptr;
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
		node_pointer p_ans = p_consignment_stack_head_;
		if ( p_ans == nullptr ) return p_ans;

		p_consignment_stack_head_ = p_ans->get_next();
		p_ans->set_next( nullptr );
		return p_ans;
	}

	void push( node_pointer p_n )
	{
		std::unique_lock<std::mutex> ul( mtx_consignment_stack_, std::defer_lock );
		if ( ul.try_lock() ) {
			// ロックが確保できたので、push先優先順位1位の共有ノードスタックにプッシュ
			nonlockchk_push_to_consignment_stack( p_n );
			node_pointer p_rcy = pop_from_tls_stack();
			if ( p_rcy != nullptr ) {
				nonlockchk_push_to_consignment_stack( p_rcy );
			}
		} else {
			if ( hzd_ptrs_.check_ptr_in_hazard_list( p_n ) ) {
				// 共有ノードスタックのロックが確保できなかった
				// p_nをスレッドローカルに戻す前に、フリーノードスタックにリサイクルを行う。
				// スレッドローカルからリサイクルウ候補を1つ取り出す。
				node_pointer p_rcy = pop_from_tls_stack();
				// 次にpush先優先順位2位スレッドローカルに戻す
				push_to_tls_stack( p_n );
				if ( p_rcy != nullptr ) {
					if ( hzd_ptrs_.check_ptr_in_hazard_list( p_rcy ) ) {
						// まだハザードポインタに残っているので、元に戻す
						push_to_tls_stack( p_rcy );
					} else {
						// ハザードポインタに乗っていないので、即座にpush先優先順位3位のフリーノードスタックにプッシュ。
						push_to_free_node_stack_wo_hzd_chk( p_rcy );
					}
				}
			} else {
				// ハザードポインタに乗っていないので、即座にpush先優先順位3位のフリーノードスタックにプッシュ。
				push_to_free_node_stack_wo_hzd_chk( p_n );
			}
		}
	}

	node_pointer pop( void )
	{
		// push先優先順位3位のスレッドローカルリストからノードを取り出し、フリーノードとして返せるかどうかをチェックする。
		node_pointer p_tls_tmp = pop_from_tls_stack();   // スレッドローカルリストからノードを取り出す
		if ( p_tls_tmp != nullptr ) {
			// ノードが取り出せたので、フリーノードとして返す。
			return p_tls_tmp;
		}
		{
			// push先優先順位2位の共有ノードスタックから取り出し、フリーノードとして返せるかどうかをチェックする。
			std::unique_lock<std::mutex> ul( mtx_consignment_stack_, std::defer_lock );
			if ( ul.try_lock() ) {
				node_pointer p_con_tmp = nonlockchk_pop_from_consignment_stack();
				if ( p_con_tmp != nullptr ) {
					// ノードが取り出せたので、フリーノードとして返す。
					return p_con_tmp;
				}
			}
		}

		// push先優先順位1位のフリーノードスタックからフリーノードを取り出す。
		node_pointer p_ans_cnd = pop_from_free_node_stack();
		if ( p_ans_cnd != nullptr ) {
			return p_ans_cnd;
		}

		{
			// push先優先順位2位の共有ノードスタックから取り出し、フリーノードとして返せるかどうか、再度をチェックする。
			std::unique_lock<std::mutex> ul( mtx_consignment_stack_, std::defer_lock );
			if ( ul.try_lock() ) {
				node_pointer p_con_tmp = nonlockchk_pop_from_consignment_stack();
				if ( p_con_tmp != nullptr ) {
					// ノードが取り出せたので、フリーノードとして返す。
					return p_con_tmp;
				}
			}
		}

		return nullptr;   // 結局取り出しには失敗した。
	}

	hzd_ptr_mgr_type                                           hzd_ptrs_;                       //!< ハザードポインタ管理構造体
	std::atomic<node_pointer>                                  p_free_node_stack_head_;         //!< 未使用状態のslot_header_of_arrayスタックの、先頭slot_header_of_arrayへのポインタ
	std::mutex                                                 mtx_consignment_stack_;          //!< スレッド終了時のハザード中のスロットを受け取るスタックの排他制御用mutex
	node_pointer                                               p_consignment_stack_head_;       //!< スレッド終了時のハザード中のスロットを受け取るスタックの、先頭slot_header_of_arrayへのポインタ。mutexロックの内側でアクセスするので、atomic変数にはしない。
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
