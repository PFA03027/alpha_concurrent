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
#include <stdexcept>
#include <tuple>
#include <type_traits>

#include "free_node_storage.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

struct is_check_concept_impl {
	template <typename T>
	static auto check_atomic_req( T* ) -> typename std::enable_if<
		std::is_trivially_copyable<T>::value &&
			std::is_copy_constructible<T>::value &&
			std::is_move_constructible<T>::value &&
			std::is_copy_assignable<T>::value &&
			std::is_move_assignable<T>::value,
		typename std::true_type>::type;

	template <typename T>
	static auto check_atomic_req( ... ) -> std::false_type;
};

template <typename T>
struct is_atomic_appliable : decltype( is_check_concept_impl::check_atomic_req<T>( nullptr ) ) {
};

template <typename T>
struct is_atomic_lockfree : std::false_type {
};

template <>
struct is_atomic_lockfree<bool> : std::conditional<ATOMIC_BOOL_LOCK_FREE == 2, std::true_type, std::false_type>::type {
};

template <>
struct is_atomic_lockfree<char> : std::conditional<ATOMIC_CHAR_LOCK_FREE == 2, std::true_type, std::false_type>::type {
};

#ifdef ATOMIC_CHAR8_T_LOCK_FREE
template <>
struct is_atomic_lockfree<char8_t> : std::conditional<ATOMIC_CHAR8_T_LOCK_FREE == 2, std::true_type, std::false_type>::type {
};
#endif

template <>
struct is_atomic_lockfree<char16_t> : std::conditional<ATOMIC_CHAR16_T_LOCK_FREE == 2, std::true_type, std::false_type>::type {
};

template <>
struct is_atomic_lockfree<char32_t> : std::conditional<ATOMIC_CHAR32_T_LOCK_FREE == 2, std::true_type, std::false_type>::type {
};

template <>
struct is_atomic_lockfree<wchar_t> : std::conditional<ATOMIC_WCHAR_T_LOCK_FREE == 2, std::true_type, std::false_type>::type {
};

template <>
struct is_atomic_lockfree<short> : std::conditional<ATOMIC_SHORT_LOCK_FREE == 2, std::true_type, std::false_type>::type {
};

template <>
struct is_atomic_lockfree<int> : std::conditional<ATOMIC_INT_LOCK_FREE == 2, std::true_type, std::false_type>::type {
};

template <>
struct is_atomic_lockfree<long> : std::conditional<ATOMIC_LONG_LOCK_FREE == 2, std::true_type, std::false_type>::type {
};

template <>
struct is_atomic_lockfree<long long> : std::conditional<ATOMIC_LLONG_LOCK_FREE == 2, std::true_type, std::false_type>::type {
};

template <typename T>
struct is_atomic_lockfree<T*> : std::conditional<ATOMIC_POINTER_LOCK_FREE == 2, std::true_type, std::false_type>::type {
};

/*!
 * @brief T型が、std::atomic<T>がlock freeで利用できる場合に使用する値保持クラス
 *
 * このクラスは、T型がpointer型であってもポインタ先のリソースの所有権を持たないクラス。
 *
 * @tparam T 保持する情報の型
 */
template <typename T>
struct value_holder_available_lf_atomic {
	using value_type  = typename std::decay<T>::type;
	using ticket_type = value_type;

	value_holder_available_lf_atomic( void )
	  : a_val_()
	{
	}

	value_holder_available_lf_atomic( const T& cont_arg )
	  : a_val_( cont_arg )
	{
	}

	value_holder_available_lf_atomic( T&& cont_arg )
	  : a_val_( std::move( cont_arg ) )
	{
	}

	value_type get_value( void ) const
	{
		return a_val_.load( std::memory_order_acquire );
	}

	ticket_type get_ticket( void ) const
	{
		return a_val_.load( std::memory_order_acquire );
	}

	value_type exchange_ticket_to_value( ticket_type tkt ) const
	{
		return tkt;
	}

	void set_value( value_type value_arg )
	{
		a_val_.store( value_arg, std::memory_order_release );
	}

protected:
	void holder_setup_by_alloc( void )
	{
	}

	void holder_lost_ownership( void )
	{
	}

	void holder_teardown_by_recycle( void )
	{
	}

private:
	std::atomic<value_type> a_val_;
};

// specialized for non ownership pointer
template <typename T>
struct value_holder_available_lf_atomic<T*> {
	using value_type  = typename std::decay<T*>::type;
	using ticket_type = value_type;

	value_holder_available_lf_atomic( void )
	  : a_val_( nullptr )
	{
	}

#if 1
	value_holder_available_lf_atomic( const value_type cont_arg )
	  : a_val_( cont_arg )
	{
	}
#else
	value_holder_available_lf_atomic( const value_type& cont_arg )
	  : a_val_( cont_arg )
	{
	}

	value_holder_available_lf_atomic( value_type&& cont_arg )
	  : a_val_( std::move( cont_arg ) )
	{
	}
#endif

	value_type get_value( void ) const
	{
		return a_val_.load( std::memory_order_acquire );
	}

	ticket_type get_ticket( void ) const
	{
		return a_val_.load( std::memory_order_acquire );
	}

	value_type exchange_ticket_to_value( ticket_type tkt ) const
	{
		return tkt;
	}

	void set_value( value_type value_arg )
	{
		a_val_.store( value_arg, std::memory_order_release );
	}

protected:
	void holder_setup_by_alloc( void )
	{
		a_val_.store( nullptr, std::memory_order_release );
	}

	void holder_lost_ownership( void )
	{
		a_val_.store( nullptr, std::memory_order_release );
	}

	void holder_teardown_by_recycle( void )
	{
		a_val_.store( nullptr, std::memory_order_release );
	}

private:
	std::atomic<value_type> a_val_;
};

/*!
 * @brief T型が、std::atomic<T>がlock freeで利用できる場合に使用する値保持クラス
 *
 * このクラスは、pointer型専用で、かつポインタ先のリソースの所有権を持つクラス。
 *
 * @tparam T 保持する情報の型
 */
template <typename T>
struct value_holder_available_lf_atomic_pointer_ownership {
	using value_type  = typename std::decay<T>::type;
	using ticket_type = value_type;

	value_holder_available_lf_atomic_pointer_ownership( void )
	  : a_val_( nullptr )
	{
		static_assert( std::is_pointer<T>::value, "T is not a pointer" );
	}

	value_holder_available_lf_atomic_pointer_ownership( const T& cont_arg )
	  : a_val_( cont_arg )
	{
	}

	value_holder_available_lf_atomic_pointer_ownership( T&& cont_arg )
	  : a_val_( std::move( cont_arg ) )
	{
	}

	~value_holder_available_lf_atomic_pointer_ownership()
	{
		holder_teardown_by_recycle();
	}

	value_type get_value( void ) const
	{
		return a_val_.load( std::memory_order_acquire );
	}

	ticket_type get_ticket( void ) const
	{
		return a_val_.load( std::memory_order_acquire );
	}

	value_type exchange_ticket_to_value( ticket_type tkt ) const
	{
		return tkt;
	}

	void set_value( value_type value_arg )
	{
		value_type p_my = a_val_.load( std::memory_order_acquire );
		while ( !a_val_.compare_exchange_strong( p_my, value_arg ) ) {}
		if ( p_my != nullptr ) {
			delete p_my;
		}
	}

protected:
	void holder_setup_by_alloc( void )
	{
		a_val_.store( nullptr, std::memory_order_release );
	}

	void holder_lost_ownership( void )
	{
		a_val_.store( nullptr, std::memory_order_release );
	}

	void holder_teardown_by_recycle( void )
	{
		value_type p_my = a_val_.load( std::memory_order_acquire );
		if ( p_my == nullptr ) {
			return;
		}

		if ( a_val_.compare_exchange_strong( p_my, nullptr ) ) {
			delete p_my;
		}
	}

private:
	std::atomic<value_type> a_val_;
};

/*!
 * @brief T型が、std::atomic<T>がlock freeで利用できる場合に使用する値保持クラス
 *
 * @tparam T 保持する情報の型
 */
template <typename T>
struct value_holder_non_atomic {
	using value_type       = typename std::decay<T>::type;
	using pointer_to_value = value_type*;
	using ticket_type      = value_type*;

	value_holder_non_atomic( void )
	  : ap_val_( nullptr )
	{
	}

	value_holder_non_atomic( const T& cont_arg )
	  : ap_val_( nullptr )
	{
		pointer_to_value p_val = allocate_and_construct_data_area( cont_arg );
		ap_val_.store( p_val, std::memory_order_release );
	}

	value_holder_non_atomic( T&& cont_arg )
	  : ap_val_( nullptr )
	{
		pointer_to_value p_val = allocate_and_construct_data_area( std::move( cont_arg ) );
		ap_val_.store( p_val, std::memory_order_release );
	}

	~value_holder_non_atomic()
	{
		holder_teardown_by_recycle();
	}

	const value_type& get_value( void ) const
	{
		pointer_to_value p_my = ap_val_.load( std::memory_order_acquire );
		if ( p_my == nullptr ) {
			throw std::runtime_error( "This instance has no valid data." );
		}
		return *p_my;
	}

	ticket_type get_ticket( void ) const
	{
		return ap_val_.load( std::memory_order_acquire );
	}

	value_type exchange_ticket_to_value( ticket_type tkt ) const
	{
		value_type ans = std::move( *tkt );
		delete tkt;
		return ans;
	}

	void set_value( const value_type& value_arg )
	{
		pointer_to_value p_desire = allocate_and_construct_data_area( value_arg );
		pointer_to_value p_my     = ap_val_.load( std::memory_order_acquire );
		while ( !ap_val_.compare_exchange_strong( p_my, p_desire ) ) {}
		if ( p_my != nullptr ) {
			delete p_my;
		}
	}

	void set_value( value_type&& value_arg )
	{
		pointer_to_value p_desire = allocate_and_construct_data_area( std::move( value_arg ) );
		pointer_to_value p_my     = ap_val_.load( std::memory_order_acquire );
		while ( !ap_val_.compare_exchange_strong( p_my, p_desire ) ) {}
		if ( p_my != nullptr ) {
			delete p_my;
		}
	}

protected:
	void holder_setup_by_alloc( void )
	{
		ap_val_.store( nullptr, std::memory_order_release );
	}

	void holder_lost_ownership( void )
	{
		ap_val_.store( nullptr, std::memory_order_release );
	}

	void holder_teardown_by_recycle( void )
	{
		pointer_to_value p_my = ap_val_.load( std::memory_order_acquire );
		if ( p_my == nullptr ) {
			return;
		}

		if ( ap_val_.compare_exchange_strong( p_my, nullptr ) ) {
			delete p_my;
		}
	}

private:
	static pointer_to_value allocate_and_construct_data_area( void )
	{
		return new value_type();
	}

	static pointer_to_value allocate_and_construct_data_area( const value_type& value_arg )
	{
		return new value_type( value_arg );
	}

	static pointer_to_value allocate_and_construct_data_area( value_type&& value_arg )
	{
		return new value_type( std::move( value_arg ) );
	}

	std::atomic<ticket_type> ap_val_;
};

template <
	typename T,
	bool HAS_OWNERSHIP       = true,
	typename HANDLING_POLICY = typename std::conditional<
		is_atomic_lockfree<T>::value,
		typename std::conditional<
			HAS_OWNERSHIP && std::is_pointer<T>::value,
			value_holder_available_lf_atomic_pointer_ownership<T>,
			value_holder_available_lf_atomic<T>>::type,
		value_holder_non_atomic<T>>::type>
struct one_way_list_node : public node_of_list, public HANDLING_POLICY {
	using value_type = typename std::decay<T>::type;

	one_way_list_node( void )
	  : next_( nullptr )
	{
		static_assert( std::is_copy_assignable<value_type>::value, "T need to be copy assignable." );
	}

	one_way_list_node( const value_type& cont_arg )
	  : next_( nullptr )
	{
	}

	one_way_list_node( value_type&& cont_arg )
	  : next_( nullptr )
	{
	}

	virtual ~one_way_list_node()
	{
		next_.store( nullptr, std::memory_order_release );

		return;
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

	void setup_by_alloc( void ) override
	{
		HANDLING_POLICY::holder_setup_by_alloc();
	}
	void lost_ownership( void )
	{
		HANDLING_POLICY::holder_lost_ownership();
	}
	void teardown_by_recycle( void )
	{
		HANDLING_POLICY::holder_teardown_by_recycle();
	}

private:
	std::atomic<one_way_list_node*> next_;
};

template <typename T>
struct one_way_list_node_markable : public node_of_list {
	using value_type = typename std::decay<T>::type;

	one_way_list_node_markable( void )
	  : next_( 0 )
	  , guard_val_( false )
	  , target_()
	{
		static_assert( std::is_copy_assignable<value_type>::value, "T need to be copy assignable." );

		//		assert( next_.is_lock_free() );   // std::atomic_uintptr_t is not lock-free.
	}

	one_way_list_node_markable( const value_type& cont_arg )
	  : next_( 0 )
	  , guard_val_( false )
	  , target_( cont_arg )
	{
	}

	virtual ~one_way_list_node_markable()
	{
		next_.store( 0, std::memory_order_release );
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
		guard_val_.store( true, std::memory_order_release );
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
	std::atomic_bool      guard_val_;
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
