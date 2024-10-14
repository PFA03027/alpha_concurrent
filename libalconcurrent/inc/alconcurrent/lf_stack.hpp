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

template <typename T>
class x_stack_list {
public:
	static_assert( ( !std::is_class<T>::value ) ||
	                   ( std::is_class<T>::value &&
	                     std::is_default_constructible<T>::value && std::is_move_constructible<T>::value && std::is_move_assignable<T>::value ),
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

	bool is_empty( void ) const
	{
		return lf_stack_impl_.is_empty();
	}

private:
	using node_type    = internal::od_node<T>;
	using node_pointer = node_type*;

	using node_stack_lockfree_t = internal::od_node_stack_lockfree_base<node_type, typename node_type::hazard_handler_next_t>;
	using node_pool_t           = internal::od_node_pool<node_type, typename node_type::raw_next_t>;

	node_stack_lockfree_t lf_stack_impl_;
	node_pool_t           unused_node_pool_;
};

}   // namespace internal

template <typename T>
class stack_list : public internal::x_stack_list<T> {
public:
	stack_list( void ) = default;
	stack_list( size_t reserve_size ) noexcept
	  : stack_list()
	{
	}
};
template <typename T>
class stack_list<T[]> : public internal::x_stack_list<T*> {
public:
	using value_type = T[];

	stack_list( void ) = default;
	stack_list( size_t reserve_size ) noexcept
	  : stack_list()
	{
	}
};
template <typename T, size_t N>
class stack_list<T[N]> : public internal::x_stack_list<T*> {
public:
	using value_type = T[N];

	stack_list( void ) = default;
	stack_list( size_t reserve_size ) noexcept
	  : stack_list()
	{
	}
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_LF_STACK_HPP_ */
