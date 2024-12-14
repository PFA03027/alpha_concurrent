/*!
 * @file	lf_fifo.hpp
 * @brief	semi Lock free FIFO
 * @author	Teruaki Ata
 * @date	Created on 2020/12/31
 * @details
 *
 * Copyright (C) 2020 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef ALCONCCURRENT_INC_LF_FIFO_HPP_
#define ALCONCCURRENT_INC_LF_FIFO_HPP_

#include <array>
#include <atomic>
#include <memory>
#include <tuple>

#include "hazard_ptr.hpp"
#include "internal/alcc_optional.hpp"
#include "internal/od_lockfree_fifo.hpp"
#include "internal/od_node_essence.hpp"
#include "internal/od_node_pool.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

template <typename T>
class x_lockfree_fifo {
public:
	static_assert( ( !std::is_class<T>::value ) ||
	                   ( std::is_class<T>::value &&
	                     ( ( std::is_copy_constructible<T>::value && std::is_copy_assignable<T>::value ) ||
	                       ( std::is_move_constructible<T>::value && std::is_move_assignable<T>::value ) ) ),
	               "T should be copy constructible and copy assignable, or, move constructible and move assignable" );

	using value_type = T;

	x_lockfree_fifo( void ) noexcept
	  :
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	  allocated_node_count_( 0 )
	  ,
#endif
	  lf_fifo_impl_()
	{
	}

	~x_lockfree_fifo()
	{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		internal::LogOutput( log_type::DUMP, "x_lockfree_fifo: allocated_node_count = %zu", allocated_node_count_.load() );
#endif

		auto tmp = pop();
		while ( tmp.has_value() ) {
			tmp = pop();
		}

		node_pool_t::push( lf_fifo_impl_.release_sentinel_node() );
	}

	template <bool IsCopyConstructible = std::is_copy_constructible<value_type>::value,
	          bool IsCopyAssignable    = std::is_copy_assignable<value_type>::value,
	          typename std::enable_if<
				  IsCopyConstructible && IsCopyAssignable>::type* = nullptr>
	void push( const T& v_arg )
	{
		lf_fifo_impl_.push_back( allocate_node( v_arg ) );
	}

	template <bool IsMoveConstructible = std::is_move_constructible<value_type>::value,
	          bool IsMoveAssignable    = std::is_move_assignable<value_type>::value,
	          typename std::enable_if<
				  IsMoveConstructible && IsMoveAssignable>::type* = nullptr>
	void push( T&& v_arg )
	{
		lf_fifo_impl_.push_back( allocate_node( std::move( v_arg ) ) );
	}

	template <bool IsCopyConstructible = std::is_copy_constructible<value_type>::value,
	          bool IsCopyAssignable    = std::is_copy_assignable<value_type>::value,
	          typename std::enable_if<
				  IsCopyConstructible && IsCopyAssignable>::type* = nullptr>
	void push_head( const T& v_arg )
	{
		node_pointer p_old_sentinel = lf_fifo_impl_.push_front( allocate_node(), allocate_node( v_arg ) );
		node_pool_t::push( p_old_sentinel );
	}

	template <bool IsMoveConstructible = std::is_move_constructible<value_type>::value,
	          bool IsMoveAssignable    = std::is_move_assignable<value_type>::value,
	          typename std::enable_if<
				  IsMoveConstructible && IsMoveAssignable>::type* = nullptr>
	void push_head( T&& v_arg )
	{
		node_pointer p_old_sentinel = lf_fifo_impl_.push_front( allocate_node(), allocate_node( std::move( v_arg ) ) );
		node_pool_t::push( p_old_sentinel );
	}

	template <bool IsMoveConstructible = std::is_move_constructible<value_type>::value,
	          bool IsMoveAssignable    = std::is_move_assignable<value_type>::value,
	          typename std::enable_if<
				  IsMoveConstructible && IsMoveAssignable>::type* = nullptr>
	alcc_optional<value_type> pop( void )
	{
		value_type   popped_value_storage;
		node_pointer p_popped_node = lf_fifo_impl_.pop_front( &popped_value_storage );
		if ( p_popped_node == nullptr ) return alcc_nullopt;

		node_pool_t::push( p_popped_node );
		return alcc_optional<value_type> { std::move( popped_value_storage ) };
	}

	template <bool IsMoveConstructible = std::is_move_constructible<value_type>::value,
	          bool IsMoveAssignable    = std::is_move_assignable<value_type>::value,
	          bool IsCopyConstructible = std::is_copy_constructible<value_type>::value,
	          bool IsCopyAssignable    = std::is_copy_assignable<value_type>::value,
	          typename std::enable_if<
				  !( IsMoveConstructible && IsMoveAssignable ) && ( IsCopyConstructible && IsCopyAssignable )>::type* = nullptr>
	alcc_optional<value_type> pop( void )
	{
		value_type   popped_value_storage;
		node_pointer p_popped_node = lf_fifo_impl_.pop_front( &popped_value_storage );
		if ( p_popped_node == nullptr ) return alcc_nullopt;

		node_pool_t::push( p_popped_node );
		return alcc_optional<value_type> { popped_value_storage };
	}

	bool is_empty( void ) const
	{
		return lf_fifo_impl_.is_empty();
	}

	static void clear_node_pool_as_possible_as( void )
	{
		node_pool_t::clear_as_possible_as();
	}

private:
	using node_type    = od_node_type1<T>;
	using node_pointer = od_node_type1<T>*;

	static node_pointer allocate_node( void )
	{
		node_pointer p_new_nd = node_pool_t::pop();
		if ( p_new_nd == nullptr ) {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			allocated_node_count_++;
#endif
			p_new_nd = new node_type;
		}
		return p_new_nd;
	}
	static node_pointer allocate_node( const T& v_arg )
	{
		node_pointer p_new_nd = node_pool_t::pop();
		if ( p_new_nd != nullptr ) {
			p_new_nd->set_value( v_arg );
		} else {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			allocated_node_count_++;
#endif
			p_new_nd = new node_type( v_arg );
		}
		return p_new_nd;
	}
	static node_pointer allocate_node( T&& v_arg )
	{
		node_pointer p_new_nd = node_pool_t::pop();
		if ( p_new_nd != nullptr ) {
			p_new_nd->set_value( std::move( v_arg ) );
		} else {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			allocated_node_count_++;
#endif
			p_new_nd = new node_type( std::move( v_arg ) );
		}
		return p_new_nd;
	}

	class node_fifo_lockfree_t : private od_lockfree_fifo {
	public:
		node_fifo_lockfree_t( void )
		  : od_lockfree_fifo( x_lockfree_fifo::allocate_node() )
		{
		}

		void push_back( x_lockfree_fifo::node_pointer p_nd ) noexcept
		{
			od_lockfree_fifo::push_back( p_nd );
		}
		x_lockfree_fifo::node_pointer push_front( x_lockfree_fifo::node_pointer p_sentinel, x_lockfree_fifo::node_pointer p_nd_w_value ) noexcept
		{
			od_lockfree_fifo::node_pointer p_old_sentinel = od_lockfree_fifo::push_front( p_sentinel, p_nd_w_value );
			if ( p_old_sentinel == nullptr ) return nullptr;
			return static_cast<x_lockfree_fifo::node_pointer>( p_old_sentinel );
		}
		x_lockfree_fifo::node_pointer pop_front( x_lockfree_fifo::value_type* p_context_local_data ) noexcept
		{
			od_lockfree_fifo::node_pointer p_node = od_lockfree_fifo::pop_front( p_context_local_data );
			if ( p_node == nullptr ) return nullptr;
			return static_cast<x_lockfree_fifo::node_pointer>( p_node );
		}

		x_lockfree_fifo::node_pointer release_sentinel_node( void ) noexcept
		{
			od_lockfree_fifo::node_pointer p_node = od_lockfree_fifo::release_sentinel_node();
			if ( p_node == nullptr ) return nullptr;
			return static_cast<x_lockfree_fifo::node_pointer>( p_node );
		}

		bool is_empty( void ) const
		{
			return od_lockfree_fifo::is_empty();
		}

	private:
		template <bool IsMovable = std::is_move_assignable<T>::value, typename std::enable_if<IsMovable>::type* = nullptr>
		void callback_to_pick_up_value_impl( x_lockfree_fifo::node_type& node_stored_value, x_lockfree_fifo::value_type& context_local_data )
		{
			context_local_data = std::move( node_stored_value ).get_value();
		}
		template <bool IsCopyable                                          = std::is_copy_assignable<T>::value,
		          bool IsMovable                                           = std::is_move_assignable<T>::value,
		          typename std::enable_if<!IsMovable && IsCopyable>::type* = nullptr>
		void callback_to_pick_up_value_impl( x_lockfree_fifo::node_type& node_stored_value, x_lockfree_fifo::value_type& context_local_data )
		{
			context_local_data = node_stored_value.get_value();
		}

		void callback_to_pick_up_value( od_lockfree_fifo::node_pointer p_node_stored_value, void* p_context_local_data ) override
		{
			x_lockfree_fifo::node_type& node_stored_value = *( static_cast<x_lockfree_fifo::node_pointer>( p_node_stored_value ) );   // od_lockfree_fifoに保管されているノードはnode_pointerであることを保証しているため、static_castを使用する。

			x_lockfree_fifo::value_type* p_storage_for_popped_value_ = reinterpret_cast<x_lockfree_fifo::value_type*>( p_context_local_data );
			callback_to_pick_up_value_impl( node_stored_value, *p_storage_for_popped_value_ );
		}
	};   // namespace internal

	using node_pool_t = od_node_pool<node_type>;

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	std::atomic<size_t> allocated_node_count_;
#endif
	node_fifo_lockfree_t lf_fifo_impl_;

};   // namespace concurrent

}   // namespace internal

template <typename T>
class fifo_list : public internal::x_lockfree_fifo<T> {
public:
	fifo_list( void ) = default;
	fifo_list( size_t reserve_size ) noexcept
	  : fifo_list()
	{
	}
};
template <typename T>
class fifo_list<T[]> : public internal::x_lockfree_fifo<T*> {
public:
	using value_type = T[];

	fifo_list( void ) = default;
	fifo_list( size_t reserve_size ) noexcept
	  : fifo_list()
	{
	}
};
template <typename T, size_t N>
class fifo_list<T[N]> : public internal::x_lockfree_fifo<std::array<T, N>> {
public:
	using value_type = T[N];

	fifo_list( void ) = default;
	fifo_list( size_t reserve_size ) noexcept
	  : fifo_list()
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

		internal::x_lockfree_fifo<std::array<T, N>>::push( std::move( tmp ) );
	}
	void push(
		value_type&& cont_arg   //!< [in]	a value to push this FIFO queue
	)
	{
		std::array<T, N> tmp;
		for ( size_t i = 0; i < N; i++ ) {
			tmp[i] = std::move( cont_arg[i] );
		}

		internal::x_lockfree_fifo<std::array<T, N>>::push( std::move( tmp ) );
	}

	bool pop( value_type& a )
	{
		alcc_optional<std::array<T, N>> ret = internal::x_lockfree_fifo<std::array<T, N>>::pop();
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

#endif /* SRC_LF_FIFO_HPP_ */
