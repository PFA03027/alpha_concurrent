/*!
 * @file	lf_one_side_deque.hpp
 * @brief	semi Lock free one side deque
 * @author	Teruaki Ata
 * @date	Created on 2021/03/19
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef INC_LF_ONE_SIDE_DEQUE_HPP_
#define INC_LF_ONE_SIDE_DEQUE_HPP_

#include <atomic>
#include <memory>
#include <tuple>

#include "lf_fifo.hpp"
#include "lf_stack.hpp"

namespace alpha {
namespace concurrent {

/*!
 * @breif	semi-lock free one side deque
 *
 * Type T should be copy assignable.
 *
 * In case that template parameter ALLOW_TO_ALLOCATE is true, @n
 * In case of no avialable free node that carries a value, new node is allocated from heap internally. @n
 * In this case, this queue may be locked. And push() may trigger this behavior. @n
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
 * To resolve ABA issue, this one side deque uses hazard pointer approach.
 */
template <typename T, bool ALLOW_TO_ALLOCATE = true, bool HAS_OWNERSHIP = true>
class one_side_deque {
public:
	using head_stack_t = stack_list<T, ALLOW_TO_ALLOCATE, HAS_OWNERSHIP>;
	using tail_fifo_t  = fifo_list<T, ALLOW_TO_ALLOCATE, HAS_OWNERSHIP>;

	using input_type = typename head_stack_t::input_type;
	using value_type = typename head_stack_t::value_type;

	/*!
	 * @breif	Constructor
	 *
	 * In case that ALLOW_TO_ALLOCATE is false, argument pre_alloc_nodes must be equal or than 1.
	 * This value should be at least the number of CPUs.
	 * Also, it is recommended to double the number of threads to access.
	 */
	one_side_deque(
		unsigned int pre_alloc_nodes = 1   //!< [in]	number of pre-allocated internal free node
		)
	  : head_side_( pre_alloc_nodes )
	  , tail_size_( pre_alloc_nodes )
	{
	}

	/*!
	 * @breif	Push a value to back side of this one side deque
	 *
	 * cont_arg will copy to one side deque.
	 *
	 * @note
	 * In case that template parameter ALLOW_TO_ALLOCATE is true, this I/F is valid.
	 */
	template <bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push_back(
		const input_type& cont_arg   //!< [in]	a value to push this one side deque
		) -> typename std::enable_if<BOOL_VALUE, void>::type
	{
		tail_size_.push( cont_arg );
		return;
	}
	template <bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push_back(
		input_type&& cont_arg   //!< [in]	a value to push this one side deque
		) -> typename std::enable_if<BOOL_VALUE, void>::type
	{
		tail_size_.push( std::move( cont_arg ) );
		return;
	}

	/*!
	 * @breif	Push a value to back side of this one side deque
	 *
	 * cont_arg will copy to one side deque.
	 *
	 * @return	success or fail to push
	 * @retval	true	success to push copy value of cont_arg to one side deque
	 * @retval	false	fail to push cont_arg value to one side deque
	 *
	 * @note
	 * @li	In case that template parameter ALLOW_TO_ALLOCATE is false, this I/F is valid.
	 * @li	In case that return value is false, User side has a role to recover this condition by User side itself, e.g. backoff approach.
	 */
	template <bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push_back(
		const input_type& cont_arg   //!< [in]	a value to push this one side deque
		) -> typename std::enable_if<!BOOL_VALUE, bool>::type
	{
		return tail_size_.push( cont_arg );
	}
	template <bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push_back(
		input_type&& cont_arg   //!< [in]	a value to push this one side deque
		) -> typename std::enable_if<!BOOL_VALUE, bool>::type
	{
		return tail_size_.push( std::move( cont_arg ) );
	}

	/*!
	 * @breif	Push a value to front side of this one side deque
	 *
	 * cont_arg will copy to one side deque.
	 *
	 * @note
	 * In case that template parameter ALLOW_TO_ALLOCATE is true, this I/F is valid.
	 */
	template <bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push_front(
		const input_type& cont_arg   //!< [in]	a value to push this one side deque
		) -> typename std::enable_if<BOOL_VALUE, void>::type
	{
		head_side_.push( cont_arg );
		return;
	}
	template <bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push_front(
		input_type&& cont_arg   //!< [in]	a value to push this one side deque
		) -> typename std::enable_if<BOOL_VALUE, void>::type
	{
		head_side_.push( std::move( cont_arg ) );
		return;
	}

	/*!
	 * @breif	Push a value to front side of this one side deque
	 *
	 * cont_arg will copy to one side deque.
	 *
	 * @return	success or fail to push
	 * @retval	true	success to push copy value of cont_arg to one side deque
	 * @retval	false	fail to push cont_arg value to one side deque
	 *
	 * @note
	 * @li	In case that template parameter ALLOW_TO_ALLOCATE is false, this I/F is valid.
	 * @li	In case that return value is false, User side has a role to recover this condition by User side itself, e.g. backoff approach.
	 */
	template <bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push_front(
		const input_type& cont_arg   //!< [in]	a value to push this one side deque
		) -> typename std::enable_if<!BOOL_VALUE, bool>::type
	{
		return head_side_.push( cont_arg );
	}
	template <bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push_front(
		input_type&& cont_arg   //!< [in]	a value to push this one side deque
		) -> typename std::enable_if<!BOOL_VALUE, bool>::type
	{
		return head_side_.push( std::move( cont_arg ) );
	}

	/*!
	 * @breif	Pop a value from front side of this one side deque
	 *
	 * @return	1st element: true=success to pop a value, false=no value to pop
	 * @return	2nd element: a value that is pop. In case that 1st element is true, 2nd element is valid.
	 */
	std::tuple<bool, value_type> pop_front( void )
	{
		std::tuple<bool, value_type> ans;
		ans = head_side_.pop();
		if ( std::get<0>( ans ) ) {
			return ans;
		}

		ans = tail_size_.pop();
		return ans;
	}

	/*!
	 * @breif	number of the queued values in one side deque
	 *
	 * @warning
	 * This one side deque will be access by several thread concurrently. So, true number of this one side deque may be changed when caller uses the returned value.
	 */
	int get_size( void ) const
	{
		return head_side_.get_size() + tail_size_.get_size();
	}

	/*!
	 * @breif	get the total number of the allocated internal nodes
	 *
	 * @warning
	 * This one side deque will be access by several thread concurrently. So, true number of this one side deque may be changed when caller uses the returned value.
	 */
	int get_allocated_num( void )
	{
		return head_side_.get_allocated_num() + tail_size_.get_allocated_num();
	}

private:
	one_side_deque( const one_side_deque& ) = delete;
	one_side_deque( one_side_deque&& )      = delete;
	one_side_deque& operator=( const one_side_deque& ) = delete;
	one_side_deque& operator=( one_side_deque&& ) = delete;

	head_stack_t head_side_;
	tail_fifo_t  tail_size_;
};

}   // namespace concurrent
}   // namespace alpha

#endif /* INC_LF_ONE_SIDE_DEQUE_HPP_ */
