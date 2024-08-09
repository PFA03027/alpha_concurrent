/*!
 * @file	lf_stack.hpp
 * @brief	semi Lock free Stack
 * @author	Teruaki Ata
 * @date	Created on 2021/01/04
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef ALCONCCURRENT_INC_LF_STACK_HPP_
#define ALCONCCURRENT_INC_LF_STACK_HPP_

#include <atomic>
#include <memory>
#include <tuple>

#include "hazard_ptr.hpp"
#include "internal/free_node_storage.hpp"
#include "internal/one_way_list_node.hpp"

#include "internal/od_node_base.hpp"
#include "internal/od_node_pool.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

/*!
 * @brief	リスト型のFIFOキューの基本要素となるFIFOキュークラス
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
	using node_type                     = one_way_list_node<T, HAS_OWNERSHIP>;
	using input_type                    = typename node_type::input_type;
	using value_type                    = typename node_type::value_type;
	using node_pointer                  = node_type*;
	using hazard_ptr_storage_t          = hazard_ptr_storage<node_type, hzrd_max_slot_>;

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
	 * @brief	FIFOキューへノードをPushする。
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
	 * @brief	FIFOキューからポインタを取り出す。
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
		scoped_hazard_ref scoped_ref_next( scoped_ref_first, (int)hazard_ptr_idx::POP_FUNC_NEXT );

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
	 * @brief	Check whether a pointer is in this hazard list
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
	lifo_nd_list( const lifo_nd_list& )           = delete;
	lifo_nd_list( lifo_nd_list&& )                = delete;
	lifo_nd_list operator=( const lifo_nd_list& ) = delete;
	lifo_nd_list operator=( lifo_nd_list&& )      = delete;

	using scoped_hazard_ref = hazard_ptr_scoped_ref<node_type, hzrd_max_slot_>;

	enum class hazard_ptr_idx : int {
		PUSH_FUNC_FIRST = 0,
		POP_FUNC_FIRST  = 1,
		POP_FUNC_NEXT   = 2
	};

	std::atomic<node_pointer> head_;
	std::atomic<int>          size_count_;

	hazard_ptr_storage_t hzrd_ptr_;
};

}   // namespace internal

/*!
 * @brief	semi-lock free Stack type queue
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
	using lifo_type  = internal::lifo_nd_list<T, HAS_OWNERSHIP>;
	using input_type = typename lifo_type::input_type;
	using value_type = typename lifo_type::value_type;

	/*!
	 * @brief	Constructor
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
			internal::LogOutput( log_type::WARN, "Warning: in case that ALLOW_TO_ALLOCATE is false, argument pre_alloc_nodes must be equal or than 1, now %d. Therefore it will be modified to 1. This value should be at least the number of CPUs. Also, it is recommended to double the number of threads to access.", pre_alloc_nodes );
			pre_alloc_nodes = 1;
		}

		free_nd_.init_and_pre_allocate<lifo_node_type>( pre_alloc_nodes );
		return;
	}

	/*!
	 * @brief	Push a value to this LIFO stack
	 *
	 * @note
	 * In case that template parameter ALLOW_TO_ALLOCATE is true, this I/F is valid.
	 */
	template <bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push(
		const input_type& cont_arg   //!< [in]	a value to push this LIFO queue
		) -> typename std::enable_if<BOOL_VALUE, void>::type
	{
		push_common( cont_arg );
		return;
	}
	template <bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push(
		input_type&& cont_arg   //!< [in]	a value to push this LIFO queue
		) -> typename std::enable_if<BOOL_VALUE, void>::type
	{
		push_common( std::move( cont_arg ) );
		return;
	}

	/*!
	 * @brief	Push a value to this LIFO stack
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
		const input_type& cont_arg   //!< [in]	a value to push this LIFO queue
		) -> typename std::enable_if<!BOOL_VALUE, bool>::type
	{
		return push_common( cont_arg );
	}
	template <bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push(
		input_type&& cont_arg   //!< [in]	a value to push this LIFO queue
		) -> typename std::enable_if<!BOOL_VALUE, bool>::type
	{
		return push_common( std::move( cont_arg ) );
	}

	/*!
	 * @brief	Pop a value from this FIFO queue
	 *
	 * @return	1st element: true=success to pop a value, false=no value to pop
	 * @return	2nd element: a value that is pop. In case that 1st element is true, 2nd element is valid.
	 */
	std::tuple<bool, value_type> pop( void )
	{
		lifo_node_pointer p_poped_node = lifo_.pop();

		if ( p_poped_node == nullptr ) return std::tuple<bool, value_type>( false, value_type {} );

		// p_poped_nodeが保持していた値の引き出し、かつ所有権を移動
		auto       ticket    = p_poped_node->get_ticket();
		value_type ans_value = p_poped_node->exchange_ticket_and_move_value( ticket );

		free_nd_.recycle( (free_node_pointer)p_poped_node );

		return std::tuple<bool, value_type>( true, std::move( ans_value ) );
	}

	/*!
	 * @brief	number of the queued values in FIFO
	 *
	 * @warning
	 * This FIFO will be access by several thread concurrently. So, true number of this FIFO queue may be changed when caller uses the returned value.
	 */
	int get_size( void ) const
	{
		return lifo_.get_size();
	}

	/*!
	 * @brief	get the total number of the allocated internal nodes
	 *
	 * @warning
	 * This FIFO will be access by several thread concurrently. So, true number of this FIFO queue may be changed when caller uses the returned value.
	 */
	int get_allocated_num( void )
	{
		return free_nd_.get_allocated_num();
	}

private:
	stack_list( const stack_list& )            = delete;
	stack_list( stack_list&& )                 = delete;
	stack_list& operator=( const stack_list& ) = delete;
	stack_list& operator=( stack_list&& )      = delete;

	using free_nd_storage_type = internal::free_nd_storage;
	using free_node_type       = typename free_nd_storage_type::node_type;
	using free_node_pointer    = typename free_nd_storage_type::node_pointer;
	using lifo_node_type       = typename lifo_type::node_type;
	using lifo_node_pointer    = typename lifo_type::node_pointer;

	template <typename TRANSFER_T, bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push_common(
		TRANSFER_T cont_arg   //!< [in]	a value to push this LIFO queue
		) -> typename std::enable_if<BOOL_VALUE, void>::type
	{
		lifo_node_pointer p_new_node = free_nd_.allocate<lifo_node_type>(
			true,
			[this]( lifo_node_pointer p_chk_node ) {
				return !( this->lifo_.check_hazard_list( p_chk_node ) );
				//				return false;
			} );

		p_new_node->set_value( std::forward<TRANSFER_T>( cont_arg ) );

		lifo_.push( p_new_node );
		return;
	}

	template <typename TRANSFER_T, bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push_common(
		TRANSFER_T cont_arg   //!< [in]	a value to push this LIFO queue
		) -> typename std::enable_if<!BOOL_VALUE, bool>::type
	{
		lifo_node_pointer p_new_node = free_nd_.allocate<lifo_node_type>(
			false,
			[this]( lifo_node_pointer p_chk_node ) {
				return !( this->lifo_.check_hazard_list( p_chk_node ) );
				//				return false;
			} );

		if ( p_new_node != nullptr ) {
			p_new_node->set_value( std::forward<TRANSFER_T>( cont_arg ) );

			lifo_.push( p_new_node );
			return true;
		} else {
			return false;
		}
	}

	lifo_type            lifo_;
	free_nd_storage_type free_nd_;
};

template <typename T>
class x_stack_list {
public:
	static_assert( std::is_default_constructible<T>::value && std::is_move_constructible<T>::value && std::is_move_assignable<T>::value,
	               "T should be default constructible, move constructible and move assignable at least" );

	using value_type = T;

	constexpr x_stack_list( void ) noexcept
	  : lf_stack_impl_()
	  , unused_node_pool_()
	{
	}

	x_stack_list( size_t reserve_size ) noexcept
	  : x_stack_list()
	{
	}
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	~x_stack_list()
	{
		if ( node_pool_t::profile_info_count() != 0 ) {
			internal::LogOutput( log_type::TEST, "%s", node_pool_t::profile_info_string().c_str() );
			node_pool_t::clear_as_possible_as();
		}
	}
#endif

	template <bool IsCopyConstructivle = std::is_copy_constructible<T>::value, bool IsCopyAssignable = std::is_copy_assignable<T>::value, typename std::enable_if<IsCopyConstructivle && IsCopyAssignable>::type* = nullptr>
	void push( const T& v_arg )
	{
		node_pointer p_new_nd = unused_node_pool_.pop();
		if ( p_new_nd != nullptr ) {
			p_new_nd->get() = v_arg;
		} else {
			p_new_nd = new node_type( nullptr, v_arg );
		}
		lf_stack_impl_.push_front( p_new_nd );
	}
	template <bool IsMoveConstructivle = std::is_move_constructible<T>::value, bool IsMoveAssignable = std::is_copy_assignable<T>::value, typename std::enable_if<IsMoveConstructivle && IsMoveAssignable>::type* = nullptr>
	void push( T&& v_arg )
	{
		node_pointer p_new_nd = unused_node_pool_.pop();
		if ( p_new_nd != nullptr ) {
			p_new_nd->get() = std::move( v_arg );
		} else {
			p_new_nd = new node_type( nullptr, std::move( v_arg ) );
		}
		lf_stack_impl_.push_front( p_new_nd );
	}

	template <bool IsMoveConstructivle = std::is_move_constructible<T>::value, typename std::enable_if<IsMoveConstructivle>::type* = nullptr>
	std::tuple<bool, value_type> pop( void )
	{
		// TがMove可能である場合に選択されるAPI実装
		node_pointer p_poped_node = lf_stack_impl_.pop_front();
		if ( p_poped_node == nullptr ) return std::tuple<bool, value_type> { false, value_type {} };

		std::tuple<bool, value_type> ans { true, std::move( p_poped_node->get() ) };
		unused_node_pool_.push( p_poped_node );
		return ans;
	}
	template <bool IsMoveConstructivle = std::is_move_constructible<T>::value, bool IsCopyConstructivle = std::is_copy_constructible<T>::value,
	          typename std::enable_if<!IsMoveConstructivle && IsCopyConstructivle>::type* = nullptr>
	std::tuple<bool, value_type> pop( void )
	{
		// TがMove不可能であるが、Copy可能である場合に選択されるAPI実装
		node_pointer p_poped_node = lf_stack_impl_.pop_front();
		if ( p_poped_node == nullptr ) return std::tuple<bool, value_type> { false, value_type {} };

		std::tuple<bool, value_type> ans { true, p_poped_node->get() };
		unused_node_pool_.push( p_poped_node );
		return ans;
	}

private:
	using node_type    = internal::od_node<T>;
	using node_pointer = node_type*;

	using node_stack_lockfree_t = internal::od_node_stack_lockfree_base<node_type, typename node_type::hazard_handler_next_t>;
	using node_pool_t           = internal::od_node_pool<node_type, typename node_type::raw_next_t>;

	node_stack_lockfree_t lf_stack_impl_;
	node_pool_t           unused_node_pool_;
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_LF_STACK_HPP_ */
