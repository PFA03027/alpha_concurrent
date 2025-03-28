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

#include <array>
#include <atomic>
#include <memory>
#include <tuple>

#include "hazard_ptr.hpp"
#include "internal/alcc_optional.hpp"
#include "internal/od_lockfree_stack.hpp"
#include "internal/od_node_pool.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

template <typename T>
class x_lockfree_stack {
public:
	static_assert( ( !std::is_class<T>::value ) ||
	                   ( std::is_class<T>::value &&
	                     ( ( std::is_copy_constructible<T>::value && std::is_copy_assignable<T>::value ) ||
	                       ( std::is_move_constructible<T>::value && std::is_move_assignable<T>::value ) ) ),
	               "T should be copy constructible and copy assignable, or, move constructible and move assignable" );

	using value_type = T;

	constexpr x_lockfree_stack( void ) noexcept
	  : lf_stack_impl_()
	  , allocated_node_count_( 0 )
	{
	}
	x_lockfree_stack( size_t reserve_size )
	  : x_lockfree_stack()
	{
		pre_allocate_nodes( reserve_size );
	}

	~x_lockfree_stack()
	{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		if ( node_pool_t::profile_info_count() != 0 ) {
			internal::LogOutput( log_type::DUMP, "%s", node_pool_t::profile_info_string().c_str() );
			node_pool_t::clear_as_possible_as();
		}
		internal::LogOutput( log_type::DUMP, "x_lockfree_stack: allocated_node_count = %zu", allocated_node_count_.load() );
#endif

		auto tmp = pop();
		while ( tmp.has_value() ) {
			tmp = pop();
		}

		for ( size_t i = 0; i < allocated_node_count_.load(); i++ ) {   // allocateしたノードをすべて開放する
			delete node_pool_t::pop();
		}
	}

	template <bool IsCopyConstructible = std::is_copy_constructible<value_type>::value,
	          bool IsCopyAssignable    = std::is_copy_assignable<value_type>::value,
	          typename std::enable_if<
				  IsCopyConstructible && IsCopyAssignable>::type* = nullptr>
	void push( const T& v_arg )
	{
		node_pointer p_new_nd = node_pool_t::pop();
		if ( p_new_nd != nullptr ) {
			p_new_nd->set_value( v_arg );
		} else {
			allocated_node_count_++;
			p_new_nd = new node_type( v_arg );
		}
		lf_stack_impl_.push_front( p_new_nd );
	}

	template <bool IsMoveConstructible = std::is_move_constructible<value_type>::value,
	          bool IsMoveAssignable    = std::is_move_assignable<value_type>::value,
	          typename std::enable_if<
				  IsMoveConstructible && IsMoveAssignable>::type* = nullptr>
	void push( T&& v_arg )
	{
		node_pointer p_new_nd = node_pool_t::pop();
		if ( p_new_nd != nullptr ) {
			p_new_nd->set_value( std::move( v_arg ) );
		} else {
			allocated_node_count_++;
			p_new_nd = new node_type( std::move( v_arg ) );
		}
		lf_stack_impl_.push_front( p_new_nd );
	}

	template <typename... Args>
	void emplace( Args&&... args )
	{
		node_pointer p_new_nd = node_pool_t::pop();
		if ( p_new_nd != nullptr ) {
			p_new_nd->emplace_value( std::forward<Args>( args )... );
		} else {
			allocated_node_count_++;
			p_new_nd = new node_type( alcc_in_place, std::forward<Args>( args )... );
		}
		lf_stack_impl_.push_front( p_new_nd );
	}

	template <bool IsMoveConstructible = std::is_move_constructible<value_type>::value,
	          bool IsMoveAssignable    = std::is_move_assignable<value_type>::value,
	          typename std::enable_if<
				  IsMoveConstructible && IsMoveAssignable>::type* = nullptr>
	alcc_optional<value_type> pop( void )
	{
		// TがMove可能である場合に選択されるAPI実装
		auto p_poped_node_orig = lf_stack_impl_.pop_front();
		if ( p_poped_node_orig == nullptr ) return alcc_nullopt;

		node_pointer              p_poped_node = static_cast<node_pointer>( p_poped_node_orig );   // このクラスが保持するノードは、すべてnode_pointerであることをpush関数で保証しているので、dynamic_castは不要。
		alcc_optional<value_type> ans { std::move( p_poped_node->get_value() ) };

		node_pool_t::push( p_poped_node );

		return ans;
	}

	template <bool IsMoveConstructible = std::is_move_constructible<value_type>::value,
	          bool IsMoveAssignable    = std::is_move_assignable<value_type>::value,
	          bool IsCopyConstructible = std::is_copy_constructible<value_type>::value,
	          bool IsCopyAssignable    = std::is_copy_assignable<value_type>::value,
	          typename std::enable_if<
				  !( IsMoveConstructible && IsMoveAssignable ) && ( IsCopyConstructible && IsCopyAssignable )>::type* = nullptr>
	alcc_optional<value_type> pop( void )
	{
		// TがMove不可能であるが、Copy可能である場合に選択されるAPI実装
		auto p_poped_node_orig = lf_stack_impl_.pop_front();
		if ( p_poped_node_orig == nullptr ) return alcc_nullopt;

		node_pointer              p_poped_node = static_cast<node_pointer>( p_poped_node_orig );   // このクラスが保持するノードは、すべてnode_pointerであることをpush関数で保証しているので、dynamic_castは不要。
		alcc_optional<value_type> ans { p_poped_node->get_value() };

		node_pool_t::push( p_poped_node );

		return ans;
	}

	size_t count_size( void ) const
	{
		return lf_stack_impl_.count_size();
	}

	bool is_empty( void ) const
	{
		return lf_stack_impl_.is_empty();
	}

	/*!
	 * @brief	get the total number of the allocated internal nodes
	 *
	 * @warning
	 * This List will be access by several thread concurrently. So, true number of this List may be changed when caller uses the returned value.
	 */
	size_t get_allocated_num( void )
	{
		return allocated_node_count_.load();
	}

	static void clear_node_pool_as_possible_as( void )
	{
		node_pool_t::clear_as_possible_as();
	}

private:
	using node_type    = od_node_type1<T>;
	using node_pointer = od_node_type1<T>*;

	using node_stack_lockfree_t = od_lockfree_stack;
	using node_pool_t           = od_node_pool<node_type>;

	void pre_allocate_nodes( size_t n )
	{
		for ( size_t i = 0; i < n; i++ ) {
			node_pool_t::push( new node_type );
		}
		allocated_node_count_ += n;
	}

	node_stack_lockfree_t lf_stack_impl_;          //!< lock free stack
	std::atomic<size_t>   allocated_node_count_;   //!< number of allocated nodes
};

}   // namespace internal

template <typename T>
class stack_list : public internal::x_lockfree_stack<T> {
public:
	stack_list( void ) noexcept = default;
	stack_list( size_t reserve_size )
	  : internal::x_lockfree_stack<T>( reserve_size )
	{
	}
};
template <typename T>
class stack_list<T[]> : public internal::x_lockfree_stack<T*> {
public:
	using value_type = T[];

	stack_list( void ) noexcept = default;
	stack_list( size_t reserve_size )
	  : internal::x_lockfree_stack<T*>( reserve_size )
	{
	}
};
template <typename T, size_t N>
class stack_list<T[N]> : public internal::x_lockfree_stack<std::array<T, N>> {
public:
	using value_type = T[N];

	stack_list( void ) noexcept = default;
	stack_list( size_t reserve_size )
	  : internal::x_lockfree_stack<std::array<T, N>>( reserve_size )
	{
	}

	void push(
		const value_type& cont_arg   //!< [in]	a value to push this FIFO queue
	)
	{
		std::array<T, N> tmp;
		for ( size_t i = 0; i < N; i++ ) {
			tmp[i] = cont_arg[i];
		}

		internal::x_lockfree_stack<std::array<T, N>>::push( std::move( tmp ) );
	}
	void push(
		value_type&& cont_arg   //!< [in]	a value to push this FIFO queue
	)
	{
		std::array<T, N> tmp;
		for ( size_t i = 0; i < N; i++ ) {
			tmp[i] = std::move( cont_arg[i] );
		}

		internal::x_lockfree_stack<std::array<T, N>>::push( std::move( tmp ) );
	}

	bool pop( value_type& a )
	{
		alcc_optional<std::array<T, N>> ret = internal::x_lockfree_stack<std::array<T, N>>::pop();
		if ( !ret.has_value() ) {
			return false;
		}

		for ( size_t i = 0; i < N; i++ ) {
			a[i] = std::move( ret.value()[i] );
		}
		return true;
	}
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_LF_STACK_HPP_ */
