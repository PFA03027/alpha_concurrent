/**
 * @file test_lf_stack_type.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-14
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include <type_traits>

#include "gtest/gtest.h"

#include "alconcurrent/lf_stack.hpp"
#include "test_type_variation.hpp"

using tut_1 = alpha::concurrent::stack_list<fully_userdefined_5_special_op>;
using tut_2 = alpha::concurrent::stack_list<partly_userdefined_5_special_op_no_default_constructor>;
using tut_3 = alpha::concurrent::stack_list<partly_userdefined_5_special_op_no_copy_constructor_assignment>;
using tut_4 = alpha::concurrent::stack_list<partly_userdefined_5_special_op_no_move_constructor_assignment>;
using tut_5 = alpha::concurrent::stack_list<partly_userdefined_5_special_op_no_copy_move_constructor_assignment>;
static_assert( std::is_default_constructible<tut_1>::value );
static_assert( std::is_constructible<tut_2>::value );
static_assert( std::is_constructible<tut_3>::value );
static_assert( std::is_constructible<tut_4>::value );
// static_assert( std::is_default_constructible<tut_5>::value );

struct is_callable_impl {
	template <typename T, typename... UArgs>
	static auto check_push( T*, UArgs&&... ) -> decltype( std::declval<T>().push( std::forward<UArgs>( std::declval<typename std::remove_reference<UArgs>::type>() )... ), std::true_type() );
	template <typename T>
	static auto check_push( ... ) -> std::false_type;

	template <typename T, typename... UArgs>
	static auto check_pop( T*, UArgs&&... ) -> decltype( std::declval<T>().pop( std::forward<UArgs>( std::declval<typename std::remove_reference<UArgs>::type>() )... ), std::true_type() );
	template <typename T>
	static auto check_pop( ... ) -> std::false_type;
};

template <typename T, typename U>
struct is_callable_push : decltype( is_callable_impl::check_push<T>( nullptr, std::declval<U>() ) ) {};
template <typename T, typename U>
struct is_callable_pop : decltype( is_callable_impl::check_pop<T>( nullptr, std::declval<U>() ) ) {};

static_assert( is_callable_push<tut_1, const fully_userdefined_5_special_op&>::value );
static_assert( is_callable_push<tut_1, fully_userdefined_5_special_op&&>::value );

static_assert( is_callable_push<tut_2, const partly_userdefined_5_special_op_no_default_constructor&>::value );
static_assert( is_callable_push<tut_2, partly_userdefined_5_special_op_no_default_constructor&&>::value );

static_assert( !is_callable_push<tut_3, const partly_userdefined_5_special_op_no_copy_constructor_assignment&>::value );
static_assert( is_callable_push<tut_3, partly_userdefined_5_special_op_no_copy_constructor_assignment&&>::value );

static_assert( is_callable_push<tut_4, const partly_userdefined_5_special_op_no_move_constructor_assignment&>::value );
static_assert( is_callable_push<tut_4, partly_userdefined_5_special_op_no_move_constructor_assignment&&>::value );

// static_assert( !is_callable_push<tut_5, const partly_userdefined_5_special_op_no_copy_move_constructor_assignment&>::value );
// static_assert( !is_callable_push<tut_5, partly_userdefined_5_special_op_no_copy_move_constructor_assignment&&>::value );
