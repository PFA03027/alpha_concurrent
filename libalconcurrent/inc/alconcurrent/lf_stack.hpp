/*!
 * @file	lf_stack.hpp
 * @brief	semi Lock free Stack
 * @author	Teruaki Ata
 * @date	Created on 2021/01/04
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef SRC_LF_STACK_HPP_
#define SRC_LF_STACK_HPP_

#include <atomic>
#include <memory>
#include <tuple>

#include "free_node_storage.hpp"
#include "hazard_ptr.hpp"
#include "one_way_list_node.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

/*!
 * @breif	リスト型のFIFOキューの基本要素となるFIFOキュークラス
 *
 * Tは、copy assignableでなければならい。
 *
 * @note
 * https://www.slideshare.net/kumagi/lock-free-safe?next_slideshow=1 @n
 */
template <typename T, bool HAS_OWNERSHIP = true>
class lifo_nd_list {
public:
	static constexpr int hzrd_max_slot_ = 3;
	using value_type                    = typename std::decay<T>::type;
	using node_type                     = one_way_list_node<T, HAS_OWNERSHIP>;
	using node_pointer                  = node_type*;
	using hazard_ptr_storage            = hazard_ptr<node_type, hzrd_max_slot_>;

	lifo_nd_list( void )
	  : head_( nullptr )
	  , size_count_( 0 )
	{
		return;
	}

	~lifo_nd_list()
	{
		node_pointer p_cur = head_.load( std::memory_order_acquire );
		head_.store( nullptr, std::memory_order_release );

		if ( p_cur != nullptr ) {
			// 先頭ノードは番兵のため、nullptrであることはありえないが、チェックする。
			// ノード自体は、フリーノードストレージに戻さず削除するが、ここは戻さない仕様で割り切る。
			do {
				node_pointer const p_nxt = p_cur->get_next();
				delete p_cur;
				p_cur = p_nxt;
			} while ( p_cur != nullptr );
		}

		return;
	}

	/*!
	 * @breif	FIFOキューへノードをPushする。
	 *
	 * @warning
	 * 引数の管理ノードは、事前にcheck_hazard_list()を使用してハザードポインタに登録されていないことを確認していること。
	 */
	void push( node_pointer const p_push_node )
	{
		p_push_node->set_next( nullptr );

		scoped_hazard_ref scoped_ref_cur( hzrd_ptr_, (int)hazard_ptr_idx::PUSH_FUNC_FIRST );

		while ( true ) {
			node_pointer p_cur_top = head_.load( std::memory_order_acquire );
			scoped_ref_cur.regist_ptr_as_hazard_ptr( p_cur_top );
			if ( p_cur_top != head_.load( std::memory_order_acquire ) ) continue;

			p_push_node->set_next( p_cur_top );

			// head_を更新する。
			// ここで、プリエンプションして、tail_がA->B->A'となった時、p_cur_lastが期待値とは異なるが、
			// ハザードポインタにA相当を確保しているので、A'は現れない。よって、このようなABA問題は起きない。
			if ( head_.compare_exchange_weak( p_cur_top, p_push_node ) ) {
				size_count_++;
				return;   // 追加完了
			}
		}
		return;
	}

	/*!
	 * @breif	FIFOキューからポインタを取り出す。
	 *
	 * 返り値の管理ノードに保管されている。
	 *
	 * @return	取り出された管理ノードへのポインタ。nullptrの場合、LIFOキューが空だったことを示す。
	 *
	 * @warning
	 * 管理ノードは、使用中の可能性がある。ハザードポインタに登録されていないことの確認が完了するまで、管理ノードの破棄や、APIを呼び出しで、値を変更してはならない。
	 */
	node_pointer pop( void )
	{
		scoped_hazard_ref scoped_ref_first( hzrd_ptr_, (int)hazard_ptr_idx::POP_FUNC_FIRST );
		scoped_hazard_ref scoped_ref_next( hzrd_ptr_, (int)hazard_ptr_idx::POP_FUNC_NEXT );

		while ( true ) {
			node_pointer p_cur_first = head_.load( std::memory_order_acquire );
			scoped_ref_first.regist_ptr_as_hazard_ptr( p_cur_first );
			if ( p_cur_first != head_.load( std::memory_order_acquire ) ) continue;

			if ( p_cur_first == nullptr ) {
				// FIFOキューは空。
				return nullptr;
			}

			node_pointer p_cur_next = p_cur_first->get_next();
			scoped_ref_next.regist_ptr_as_hazard_ptr( p_cur_next );
			if ( p_cur_next != p_cur_first->get_next() ) continue;

			// ここで、プリエンプションして、head_がA->B->A'となった時、p_cur_nextが期待値とは異なるが、
			// ハザードポインタにA相当を確保しているので、A'は現れない。よって、このようなABA問題は起きない。
			if ( head_.compare_exchange_weak( p_cur_first, p_cur_next ) ) {
				size_count_--;
				// ここで、firstの取り出しと所有権確保が完了
				// ただし、ハザードポインタをチェックしていないため、まだ参照している人がいるかもしれない。
				return p_cur_first;
			}
		}
		return nullptr;
	}

	/*!
	 * @breif	Check whether a pointer is in this hazard list
	 *
	 * @retval	true	p_chk_ptr is in this hazard list.
	 * @retval	false	p_chk_ptr is not in this hazard list.
	 */
	bool check_hazard_list( node_pointer const p_chk_node )
	{
		return hzrd_ptr_.check_ptr_in_hazard_list( p_chk_node );
	}

	int get_size( void ) const
	{
		return size_count_.load( std::memory_order_acquire );
	}

private:
	lifo_nd_list( const lifo_nd_list& ) = delete;
	lifo_nd_list( lifo_nd_list&& )      = delete;
	lifo_nd_list operator=( const lifo_nd_list& ) = delete;
	lifo_nd_list operator=( lifo_nd_list&& ) = delete;

	using scoped_hazard_ref = hazard_ptr_scoped_ref<node_type, hzrd_max_slot_>;

	enum class hazard_ptr_idx : int {
		PUSH_FUNC_FIRST = 0,
		POP_FUNC_FIRST  = 1,
		POP_FUNC_NEXT   = 2
	};

	std::atomic<node_pointer> head_;
	std::atomic<int>          size_count_;

	hazard_ptr_storage hzrd_ptr_;
};

}   // namespace internal

/*!
 * @breif	semi-lock free Stack type queue
 *
 * Type T should be copy assignable.
 *
 * In case that template parameter ALLOW_TO_ALLOCATE is true, @n
 * In case of no avialable free node that carries a value, new node is allocated from heap internally. @n
 * In this case, this queue may be locked. And push() may trigger this behavior.
 *
 * On the other hand, used free node will be recycled without a memory allocation. In this case, push() is lock free.
 *
 * To reduce lock behavior, pre-allocated nodes are effective. @n
 * get_allocated_num() provides the number of the allocated nodes. This value is hint to configuration.
 *
 * In case that template parameter ALLOW_TO_ALLOCATE is false, @n
 * In case of no avialable free node, push() member function will return false. In this case, it fails to push a value @n
 * User side has a role to recover this condition by User side itself, e.g. backoff approach.
 *
 * @note
 * To resolve ABA issue, this Stack queue uses hazard pointer approach.
 */
template <typename T, bool ALLOW_TO_ALLOCATE = true, bool HAS_OWNERSHIP = true>
class stack_list {
public:
#if 0
	using value_type = T;
	using value_type = typename std::conditional<
		std::is_array<T>::value,
		std::decay<T>::type,
		T>::type;
#endif
	using value_type = typename std::decay<T>::type;

	/*!
	 * @breif	Constructor
	 *
	 * In case that ALLOW_TO_ALLOCATE is false, argument pre_alloc_nodes must be equal or than 1.
	 * This value should be at least the number of CPUs.
	 * Also, it is recommended to double the number of threads to access.
	 */
	stack_list(
		unsigned int pre_alloc_nodes = 1   //!< [in]	number of pre-allocated internal free node
	)
	{
		if ( !ALLOW_TO_ALLOCATE && ( pre_alloc_nodes < 1 ) ) {
			LogOutput( log_type::WARN, "Warning: in case that ALLOW_TO_ALLOCATE is false, argument pre_alloc_nodes must be equal or than 1, now %d. Therefore it will be modified to 1. This value should be at least the number of CPUs. Also, it is recommended to double the number of threads to access.", pre_alloc_nodes );
			pre_alloc_nodes = 1;
		}

		free_nd_.init_and_pre_allocate<lifo_node_type>( pre_alloc_nodes );
		return;
	}

	/*!
	 * @breif	Push a value to this LIFO stack
	 *
	 * cont_arg will copy to LIFO stack.
	 *
	 * @note
	 * In case that template parameter ALLOW_TO_ALLOCATE is true, this I/F is valid.
	 */
	template <bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push(
		const value_type& cont_arg   //!< [in]	a value to push this LIFO queue
		) -> typename std::enable_if<BOOL_VALUE, void>::type
	{
		lifo_node_pointer p_new_node = free_nd_.allocate<lifo_node_type>(
			true,
			[this]( lifo_node_pointer p_chk_node ) {
				return !( this->lifo_.check_hazard_list( p_chk_node ) );
				//				return false;
			} );

		p_new_node->set_value( cont_arg );

		lifo_.push( p_new_node );
		return;
	}

	/*!
	 * @breif	Push a value to this LIFO stack
	 *
	 * cont_arg will copy to LIFO stack.
	 *
	 * @return	success or fail to push
	 * @retval	true	success to push copy value of cont_arg to LIFO
	 * @retval	false	fail to push cont_arg value to LIFO
	 *
	 * @note
	 * @li	In case that template parameter ALLOW_TO_ALLOCATE is false, this I/F is valid.
	 * @li	In case that return value is false, User side has a role to recover this condition by User side itself, e.g. backoff approach.
	 */
	template <bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push(
		const value_type& cont_arg   //!< [in]	a value to push this LIFO queue
		) -> typename std::enable_if<!BOOL_VALUE, bool>::type
	{
		lifo_node_pointer p_new_node = free_nd_.allocate<lifo_node_type>(
			false,
			[this]( lifo_node_pointer p_chk_node ) {
				return !( this->lifo_.check_hazard_list( p_chk_node ) );
				//				return false;
			} );

		if ( p_new_node != nullptr ) {
			p_new_node->set_value( cont_arg );

			lifo_.push( p_new_node );
			return true;
		} else {
			return false;
		}
	}

	/*!
	 * @breif	Pop a value from this FIFO queue
	 *
	 * @return	1st element: true=success to pop a value, false=no value to pop
	 * @return	2nd element: a value that is pop. In case that 1st element is true, 2nd element is valid.
	 */
	std::tuple<bool, value_type> pop( void )
	{
		lifo_node_pointer p_poped_node = lifo_.pop();

		if ( p_poped_node == nullptr ) return std::tuple<bool, value_type>( false, value_type {} );

		value_type ans_value = p_poped_node->get_value();

		free_nd_.recycle( (free_node_pointer)p_poped_node );

		return std::tuple<bool, value_type>( true, ans_value );
	}

	/*!
	 * @breif	number of the queued values in FIFO
	 *
	 * @warning
	 * This FIFO will be access by several thread concurrently. So, true number of this FIFO queue may be changed when caller uses the returned value.
	 */
	int get_size( void ) const
	{
		return lifo_.get_size();
	}

	/*!
	 * @breif	get the total number of the allocated internal nodes
	 *
	 * @warning
	 * This FIFO will be access by several thread concurrently. So, true number of this FIFO queue may be changed when caller uses the returned value.
	 */
	int get_allocated_num( void )
	{
		return free_nd_.get_allocated_num();
	}

private:
	stack_list( const stack_list& ) = delete;
	stack_list( stack_list&& )      = delete;
	stack_list& operator=( const stack_list& ) = delete;
	stack_list& operator=( stack_list&& ) = delete;

	using free_nd_storage_type = internal::free_nd_storage;
	using free_node_type       = typename free_nd_storage_type::node_type;
	using free_node_pointer    = typename free_nd_storage_type::node_pointer;
	using lifo_type            = internal::lifo_nd_list<T, HAS_OWNERSHIP>;
	using lifo_node_type       = typename lifo_type::node_type;
	using lifo_node_pointer    = typename lifo_type::node_pointer;

	lifo_type            lifo_;
	free_nd_storage_type free_nd_;
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_LF_STACK_HPP_ */
