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
	using value_type = typename std::decay<T>::type;

	one_way_list_node( void )
	  : next_( nullptr )
	  , guard_val_(false)
	  , target_()
	{
		static_assert( std::is_copy_assignable<value_type>::value, "T need to be copy assignable." );
	}

	one_way_list_node( const value_type& cont_arg )
	  : next_( nullptr )
	  , guard_val_(false)
	  , target_( cont_arg )
	{
	}

	virtual ~one_way_list_node()
	{
		next_.store( nullptr, std::memory_order_release );
		return;
	}

	value_type get_value( void ) const
	{
		guard_val_.load( std::memory_order_acquire );
		return target_;
	}

	value_type& ref_value( void )
	{
		guard_val_.load( std::memory_order_acquire );
		return target_;
	}

	const value_type& ref_value( void ) const
	{
		guard_val_.load( std::memory_order_acquire );
		return target_;
	}

	void set_value( const value_type& value_arg )
	{
		target_ = value_arg;
		guard_val_.store( false, std::memory_order_release );
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
	std::atomic<one_way_list_node*> next_;
	std::atomic<bool>	guard_val_;
	value_type                      target_;
};

template <typename T>
struct one_way_list_node_markable : public node_of_list {
	using value_type = typename std::decay<T>::type;

	one_way_list_node_markable( void )
	  : next_( 0 )
	  , guard_val_(false)
	  , target_()
	{
		static_assert( std::is_copy_assignable<value_type>::value, "T need to be copy assignable." );

		//		assert( next_.is_lock_free() );   // std::atomic_uintptr_t is not lock-free.
	}

	one_way_list_node_markable( const value_type& cont_arg )
	  : next_( 0 )
	  , guard_val_(false)
	  , target_(cont_arg)
	{
	}

	virtual ~one_way_list_node_markable()
	{
		next_.store(0,std::memory_order_release);
		return;
	}

	value_type get_value( void ) const
	{
		guard_val_.load(std::memory_order_acquire);
		return target_;
	}

	value_type& ref_value( void )
	{
		guard_val_.load(std::memory_order_acquire);
		return target_;
	}

	const value_type& ref_value( void ) const
	{
		guard_val_.load(std::memory_order_acquire);
		return target_;
	}

	void set_value( const value_type& value_arg )
	{
		target_ = value_arg;
		guard_val_.store( true, std::memory_order_release);
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
	std::atomic_uintptr_t next_;
	std::atomic_bool	  guard_val_;
	value_type            target_;
};

//////////////////////////////////////////////////////////////////////
// Primary template for default deleter
template <typename T>
class default_deleter {
public:
	void operator()( T& x ) {}
};

// Specialized for pointer allocated by new
template <typename T>
class default_deleter<T*> {
public:
	using pointer = T*;
	void operator()( pointer& x )
	{
		delete x;
		x = nullptr;
	}
};

// Specialized for array allocated by new []
template <typename T>
class default_deleter<T[]> {
public:
	using pointer = typename std::decay<T[]>::type;
	void operator()( pointer& x )
	{
		delete[] x;
		x = nullptr;
	}
};

// Specialized for array[N]
template <typename T, int N>
class default_deleter<T[N]> {
public:
	using pointer = typename std::decay<T[N]>::type;
	void operator()( pointer& x )
	{
	}
};

//////////////////////////////////////////////////////////////////////
template <typename T>
class mover_by_copy {
public:
	void operator()( T* from, T* to )
	{
		*to = *from;
	}
};

// Primary template
template <typename T>
class mover_by_move {
public:
	void operator()( T& from, T& to )
	{
		to = std::move( from );
	}
};

// Specialized template for pointer
template <typename T>
class mover_by_move<T*> {
public:
	using pointer = T*;
	void operator()( pointer& from, pointer& to )
	{
		to   = from;
		from = nullptr;
	}
};

template <typename T>
class mover_by_move<T[]> {
public:
	using pointer = typename std::decay<T[]>::type;

	void operator()( pointer& from, pointer& to )
	{
		to   = from;
		from = nullptr;
	}
};

template <typename T>
using default_mover = typename std::conditional<
	std::is_move_assignable<typename std::decay<T>::type>::value,
	mover_by_move<T>,
	mover_by_copy<T>>::type;

}   // namespace internal

}   // namespace concurrent
}   // namespace alpha

#endif /* ONE_WAY_LIST_NODE_HPP_ */
