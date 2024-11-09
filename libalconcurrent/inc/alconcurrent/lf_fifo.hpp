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
#include "internal/free_node_storage.hpp"
#include "internal/od_lockfree_fifo.hpp"
#include "internal/od_node_essence.hpp"
#include "internal/od_node_pool.hpp"
#include "internal/one_way_list_node.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

template <typename T, typename VALUE_DELETER = deleter_nothing<T>>
class x_fifo_list {
public:
	// static_assert( ( !std::is_class<T>::value ) ||
	//                    ( std::is_class<T>::value &&
	//                      std::is_default_constructible<T>::value && std::is_copy_constructible<T>::value && std::is_copy_assignable<T>::value ),
	//                "T should be default constructible, move constructible and move assignable at least" );
	static_assert( ( !std::is_class<T>::value ) ||
	                   ( std::is_class<T>::value &&
	                     std::is_default_constructible<T>::value ),
	               "T should be default constructible, move constructible and move assignable at least" );

	using value_type = T;

	constexpr x_fifo_list( void ) noexcept
	  : lf_fifo_impl_()
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	  , allocated_node_count_( 0 )
#endif
	{
	}

	~x_fifo_list()
	{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		internal::LogOutput( log_type::DUMP, "x_stack_list: allocated_node_count = %zu", allocated_node_count_.load() );
#endif

		VALUE_DELETER                deleter;
		std::tuple<bool, value_type> tmp = pop();
		while ( std::get<0>( tmp ) ) {
			deleter( std::get<1>( tmp ) );
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
		node_pointer p_new_nd = node_pool_t::pop();
		if ( p_new_nd != nullptr ) {
			p_new_nd->set_value( v_arg );
		} else {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			allocated_node_count_++;
#endif
			p_new_nd = new node_type( v_arg );
		}
		lf_fifo_impl_.push_back( p_new_nd );
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
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			allocated_node_count_++;
#endif
			p_new_nd = new node_type( std::move( v_arg ) );
		}
		lf_fifo_impl_.push_back( p_new_nd );
	}

	template <bool IsMoveConstructible = std::is_move_constructible<value_type>::value,
	          bool IsMoveAssignable    = std::is_move_assignable<value_type>::value,
	          typename std::enable_if<
				  IsMoveConstructible && IsMoveAssignable>::type* = nullptr>
	std::tuple<bool, value_type> pop( void )
	{
		value_type   popped_value_storage;
		node_pointer p_popped_node = lf_fifo_impl_.pop_front( &popped_value_storage );
		if ( p_popped_node == nullptr ) return std::tuple<bool, value_type> { false, value_type {} };

		node_pool_t::push( p_popped_node );
		return std::tuple<bool, value_type> { true, std::move( popped_value_storage ) };
	}

	template <bool IsMoveConstructible = std::is_move_constructible<value_type>::value,
	          bool IsMoveAssignable    = std::is_move_assignable<value_type>::value,
	          bool IsCopyConstructible = std::is_copy_constructible<value_type>::value,
	          bool IsCopyAssignable    = std::is_copy_assignable<value_type>::value,
	          typename std::enable_if<
				  !( IsMoveConstructible && IsMoveAssignable ) && ( IsCopyConstructible && IsCopyAssignable )>::type* = nullptr>
	std::tuple<bool, value_type> pop( void )
	{
		value_type   popped_value_storage;
		node_pointer p_popped_node = lf_fifo_impl_.pop_front( &popped_value_storage );
		if ( p_popped_node == nullptr ) return std::tuple<bool, value_type> { false, value_type {} };

		node_pool_t::push( p_popped_node );
		return std::tuple<bool, value_type> { true, popped_value_storage };
	}

	bool is_empty( void ) const
	{
		return lf_fifo_impl_.is_empty();
	}

private:
	using node_type    = od_node_type1<T>;
	using node_pointer = od_node_type1<T>*;

	class node_fifo_lockfree_t : private od_lockfree_fifo {
	public:
		node_fifo_lockfree_t( void )
		  : od_lockfree_fifo( new x_fifo_list::node_type )
		{
		}

		void push_back( x_fifo_list::node_pointer p_nd ) noexcept
		{
			od_lockfree_fifo::push_back( p_nd );
		}
		x_fifo_list::node_pointer pop_front( x_fifo_list::value_type* p_context_local_data ) noexcept
		{
			od_lockfree_fifo::node_pointer p_node = od_lockfree_fifo::pop_front( p_context_local_data );
			if ( p_node == nullptr ) return nullptr;
			return static_cast<x_fifo_list::node_pointer>( p_node );
		}

		x_fifo_list::node_pointer release_sentinel_node( void ) noexcept
		{
			od_lockfree_fifo::node_pointer p_node = od_lockfree_fifo::release_sentinel_node();
			if ( p_node == nullptr ) return nullptr;
			return static_cast<x_fifo_list::node_pointer>( p_node );
		}

		bool is_empty( void ) const
		{
			return od_lockfree_fifo::is_empty();
		}

	private:
		template <bool IsMovable = std::is_move_assignable<T>::value, typename std::enable_if<IsMovable>::type* = nullptr>
		void callback_to_pick_up_value_impl( x_fifo_list::node_type& node_stored_value, x_fifo_list::value_type& context_local_data )
		{
			context_local_data = std::move( node_stored_value ).get_value();
		}
		template <bool IsCopyable                                          = std::is_copy_assignable<T>::value,
		          bool IsMovable                                           = std::is_move_assignable<T>::value,
		          typename std::enable_if<!IsMovable && IsCopyable>::type* = nullptr>
		void callback_to_pick_up_value_impl( x_fifo_list::node_type& node_stored_value, x_fifo_list::value_type& context_local_data )
		{
			context_local_data = node_stored_value.get_value();
		}

		void callback_to_pick_up_value( od_lockfree_fifo::node_pointer p_node_stored_value, void* p_context_local_data ) override
		{
			x_fifo_list::node_type& node_stored_value = *( static_cast<x_fifo_list::node_pointer>( p_node_stored_value ) );   // od_lockfree_fifoに保管されているノードはnode_pointerであることを保証しているため、static_castを使用する。

			x_fifo_list::value_type* p_storage_for_popped_value_ = reinterpret_cast<x_fifo_list::value_type*>( p_context_local_data );
			callback_to_pick_up_value_impl( node_stored_value, *p_storage_for_popped_value_ );
		}
	};   // namespace internal

	using node_pool_t = od_node_pool<node_type>;

	node_fifo_lockfree_t lf_fifo_impl_;

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	std::atomic<size_t> allocated_node_count_;
#endif
};   // namespace concurrent

}   // namespace internal

template <typename T, typename VALUE_DELETER = internal::deleter_nothing<T>>
class fifo_list : public internal::x_fifo_list<T, VALUE_DELETER> {
public:
	fifo_list( void ) = default;
	fifo_list( size_t reserve_size ) noexcept
	  : fifo_list()
	{
	}
};
template <>
class fifo_list<void*> : public internal::x_fifo_list<void*, internal::deleter_nothing<void>> {
public:
	fifo_list( void ) = default;
	fifo_list( size_t reserve_size ) noexcept
	  : fifo_list()
	{
	}
};
template <typename T>
class fifo_list<T*> : public internal::x_fifo_list<T*, std::default_delete<T>> {
public:
	fifo_list( void ) = default;
	fifo_list( size_t reserve_size ) noexcept
	  : fifo_list()
	{
	}
};
template <typename T>
class fifo_list<T[]> : public internal::x_fifo_list<T*, std::default_delete<T[]>> {
public:
	using value_type = T[];

	fifo_list( void ) = default;
	fifo_list( size_t reserve_size ) noexcept
	  : fifo_list()
	{
	}
};
template <typename T, size_t N>
class fifo_list<T[N]> : public internal::x_fifo_list<std::array<T, N>, internal::deleter_nothing<std::array<T, N>>> {
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

		internal::x_fifo_list<std::array<T, N>, internal::deleter_nothing<std::array<T, N>>>::push( std::move( tmp ) );
	}
	void push(
		value_type&& cont_arg   //!< [in]	a value to push this FIFO queue
	)
	{
		std::array<T, N> tmp;
		for ( size_t i = 0; i < N; i++ ) {
			tmp[i] = std::move( cont_arg[i] );
		}

		internal::x_fifo_list<std::array<T, N>, internal::deleter_nothing<std::array<T, N>>>::push( std::move( tmp ) );
	}

	std::tuple<bool, value_type> pop( void )
	{
		std::tuple<bool, value_type> ans;
		std::get<0>( ans ) = false;

		std::tuple<bool, std::array<T, N>> ret = internal::x_fifo_list<std::array<T, N>, internal::deleter_nothing<std::array<T, N>>>::pop();
		if ( !std::get<0>( ret ) ) {
			return ans;
		}

		std::get<0>( ans ) = true;
		for ( size_t i = 0; i < N; i++ ) {
			std::get<1>( ans )[i] = std::move( std::get<1>( ret )[i] );
		}
		return ans;
	}
};
template <typename T, typename DELETER>
class fifo_list<T*, DELETER> : public internal::x_fifo_list<T*, DELETER> {
public:
	fifo_list( void ) = default;
	fifo_list( size_t reserve_size ) noexcept
	  : fifo_list()
	{
	}
};
template <typename T, typename DELETER>
class fifo_list<T[], DELETER> : public internal::x_fifo_list<T*, DELETER> {
public:
	using value_type = T[];

	fifo_list( void ) = default;
	fifo_list( size_t reserve_size ) noexcept
	  : fifo_list()
	{
	}
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_LF_FIFO_HPP_ */
