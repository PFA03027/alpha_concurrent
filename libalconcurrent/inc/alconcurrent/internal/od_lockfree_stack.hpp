/**
 * @file od_lockfree_stack.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-10-20
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCURRENT_INC_INTERNAL_OD_LOCKFREE_STACK_HPP_
#define ALCONCURRENT_INC_INTERNAL_OD_LOCKFREE_STACK_HPP_

#include "alconcurrent/internal/od_node_essence.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

class od_lockfree_stack final {
public:
	using node_type    = od_node_link_by_hazard_handler;
	using node_pointer = node_type*;

	constexpr od_lockfree_stack( void ) noexcept
	  : hph_head_()
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	  , count_( 0 )
	  , pushpop_call_count_( 0 )
	  , pushpop_loop_count_( 0 )
#endif
	{
	}

	/**
	 * @brief move constructor
	 *
	 * @warning
	 * This move constructor is NOT thread-safe, because this api is not consider the concurrency.
	 */
	od_lockfree_stack( od_lockfree_stack&& src ) noexcept;
	~od_lockfree_stack();

	od_lockfree_stack( const od_lockfree_stack& )            = delete;
	od_lockfree_stack& operator=( const od_lockfree_stack& ) = delete;
	od_lockfree_stack& operator=( od_lockfree_stack&& src )  = delete;

	/**
	 * @brief ノードリストの先頭に、ノード一つを追加する
	 *
	 * @pre
	 * p_ndは、他スレッドからアクセスされない。
	 * 自インスタンスは、他スレッドからアクセスされない。
	 *
	 * @param p_nd
	 */
	void push_front( node_pointer p_nd ) noexcept;

	/**
	 * @brief pop from front
	 *
	 * @warning because there is a possibility that a returned node is in hazard pointer, hazard_ptr_handler_t hph_next_ in returned node is not set nullptr.
	 * And should not write not only nullptr but also any next pointer information during return pointer is in hazard pointer.
	 *
	 * @return popped node_pointer. please see warning also.
	 */
	ALCC_INTERNAL_NODISCARD_ATTR node_pointer pop_front( void ) noexcept;

	size_t count_size( void ) const;

	bool is_empty( void ) const;

	size_t profile_info_count( void ) const;

private:
	using hazard_ptr_handler_t = typename node_type::hazard_ptr_handler_t;
	using hazard_pointer       = typename node_type::hazard_pointer;

	hazard_ptr_handler_t hph_head_;
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	std::atomic<size_t> count_;
	std::atomic<size_t> pushpop_call_count_;
	std::atomic<size_t> pushpop_loop_count_;
#endif
};

class od_lockfree_stack_m final {
public:
	using node_type    = od_node_1bit_markable_link_by_hazard_handler;
	using node_pointer = node_type*;

	constexpr od_lockfree_stack_m( void ) noexcept
	  : hph_head_()
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	  , count_( 0 )
	  , pushpop_call_count_( 0 )
	  , pushpop_loop_count_( 0 )
#endif
	{
	}

	/**
	 * @brief move constructor
	 *
	 * @warning
	 * This move constructor is NOT thread-safe, because this api is not consider the concurrency.
	 */
	od_lockfree_stack_m( od_lockfree_stack_m&& src ) noexcept;
	~od_lockfree_stack_m();

	od_lockfree_stack_m( const od_lockfree_stack_m& )            = delete;
	od_lockfree_stack_m& operator=( const od_lockfree_stack_m& ) = delete;
	od_lockfree_stack_m& operator=( od_lockfree_stack_m&& src )  = delete;

	/**
	 * @brief ノードリストの先頭に、ノード一つを追加する
	 *
	 * @pre
	 * p_ndは、他スレッドからアクセスされない。
	 * 自インスタンスは、他スレッドからアクセスされない。
	 *
	 * @param p_nd
	 */
	void push_front( node_pointer p_nd ) noexcept;

	/**
	 * @brief pop from front
	 *
	 * @warning because there is a possibility that a returned node is in hazard pointer, hazard_ptr_handler_t hph_next_ in returned node is not set nullptr.
	 * And should not write not only nullptr but also any next pointer information during return pointer is in hazard pointer.
	 *
	 * @return popped node_pointer. please see warning also.
	 */
	ALCC_INTERNAL_NODISCARD_ATTR node_pointer pop_front( void ) noexcept;

	size_t count_size( void ) const;

	bool is_empty( void ) const;

	size_t profile_info_count( void ) const;

private:
	using hazard_ptr_handler_t  = typename node_type::hazard_ptr_handler_t;
	using hazard_pointer_w_mark = typename node_type::hazard_pointer_w_mark;

	hazard_ptr_handler_t hph_head_;
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	std::atomic<size_t> count_;
	std::atomic<size_t> pushpop_call_count_;
	std::atomic<size_t> pushpop_loop_count_;
#endif
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
