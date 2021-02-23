/*!
 * @file	one_way_list_node.hpp
 * @brief	node class definition for one way list type structure as a internal common class
 * @author	Teruaki Ata
 * @date	Created on 2021/01/09
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef ONE_WAY_LIST_NODE_HPP_
#define ONE_WAY_LIST_NODE_HPP_

#include <cassert>

#include <atomic>
#include <memory>
#include <tuple>
#include <type_traits>

#include "free_node_storage.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

template <typename T>
struct one_way_list_node : public node_of_list {
	using value_type = T;

	one_way_list_node( void )
	  : target_()
	  , next_( nullptr )
	{
	}

	one_way_list_node( const T& cont_arg )
	  : target_( cont_arg )
	  , next_( nullptr )
	{
	}

	virtual ~one_way_list_node()
	{
		return;
	}

	T get_value( void ) const
	{
		std::atomic_thread_fence( std::memory_order_acquire );
		return target_;
	}

	T& ref_value( void )
	{
		std::atomic_thread_fence( std::memory_order_acquire );
		return target_;
	}

	const T& ref_value( void ) const
	{
		std::atomic_thread_fence( std::memory_order_acquire );
		return target_;
	}

	void set_value( const T& value_arg )
	{
		target_ = value_arg;
		std::atomic_thread_fence( std::memory_order_release );
	}

	one_way_list_node* get_next( void ) const
	{
		return next_.load( std::memory_order_acquire );
	}

	void set_next( one_way_list_node* p_new_next )
	{
		next_.store( p_new_next, std::memory_order_release );
		return;
	}

	bool next_CAS( one_way_list_node** pp_expect_ptr, one_way_list_node* p_desired_ptr )
	{
		return next_.compare_exchange_weak( *pp_expect_ptr, p_desired_ptr );
	}

private:
	value_type                      target_;
	std::atomic<one_way_list_node*> next_;
};

template <typename T>
struct one_way_list_node_markable : public node_of_list {
	using value_type = T;

	one_way_list_node_markable( void )
	  : target_()
	  , next_( 0 )
	{
		assert( next_.is_lock_free() );   // std::atomic_uintptr_t is not lock-free.
	}

	one_way_list_node_markable( const T& cont_arg )
	  : target_( cont_arg )
	  , next_( 0 )
	{
	}

	virtual ~one_way_list_node_markable()
	{
		return;
	}

	T get_value( void ) const
	{
		std::atomic_thread_fence( std::memory_order_acquire );
		return target_;
	}

	T& ref_value( void )
	{
		std::atomic_thread_fence( std::memory_order_acquire );
		return target_;
	}

	const T& ref_value( void ) const
	{
		std::atomic_thread_fence( std::memory_order_acquire );
		return target_;
	}

	void set_value( const T& value_arg )
	{
		target_ = value_arg;
		std::atomic_thread_fence( std::memory_order_release );
	}

	std::tuple<one_way_list_node_markable*, bool> get_next( void ) const
	{
		std::uintptr_t ans_base = next_.load( std::memory_order_acquire );
		return std::tuple<one_way_list_node_markable*, bool>(
			(one_way_list_node_markable*)( ans_base & ( ~( (std::uintptr_t)1 ) ) ),
			( ans_base & ( std::uintptr_t )( 1 ) ) != 0 );
	}

	void set_next( one_way_list_node_markable* p_new_next )
	{
		next_.store( reinterpret_cast<std::uintptr_t>( p_new_next ), std::memory_order_release );
		return;
	}

	bool set_mark_on( void )
	{
		std::uintptr_t orig_next = next_.load( std::memory_order_acquire );

		if ( ( orig_next & ( std::uintptr_t )( 1 ) ) != 0 ) return false;   // すでに、削除マーク済みなので、falseを返す。

		std::uintptr_t marked_next = orig_next | ( std::uintptr_t )( 1 );

		return next_.compare_exchange_weak( orig_next, marked_next );
	}

	bool next_CAS( one_way_list_node_markable** pp_expect_ptr, one_way_list_node_markable* p_desired_ptr )
	{
		return next_.compare_exchange_weak( reinterpret_cast<std::uintptr_t&>( *pp_expect_ptr ), reinterpret_cast<std::uintptr_t>( p_desired_ptr ) );
	}

private:
	value_type            target_;
	std::atomic_uintptr_t next_;
};

template <typename T>
class deleter_no_delete {
public:
	void operator()( T& x ) {}
};

template <typename T>
class deleter_delete {
public:
	void operator()( T& x )
	{
		delete x;
		x = nullptr;
	}
};

template <typename T>
class deleter_delete_array {
public:
	void operator()( T& x )
	{
		delete[] x;
		x = nullptr;
	}
};

template <typename T>
using default_deleter = typename std::conditional<
	std::is_pointer<T>::value,
	deleter_delete<T>,
	typename std::conditional<
		std::is_array<T>::value,
		deleter_delete_array<T>,
		deleter_no_delete<T>>::type>::type;

}   // namespace internal

}   // namespace concurrent
}   // namespace alpha

#endif /* ONE_WAY_LIST_NODE_HPP_ */
