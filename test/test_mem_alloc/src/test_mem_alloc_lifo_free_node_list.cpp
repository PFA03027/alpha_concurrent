/**
 * @file test_mem_alloc_lifo_free_node_list.cpp
 * @author Teruaki Ata <PFA03027@nifty.com>
 * @brief
 * @version 0.1
 * @date 2023-08-11
 *
 * @copyright Copyright (c) 2023 Teruaki Ata <PFA03027@nifty.com>
 *
 */

#include "gtest/gtest.h"

#include "../src/lifo_free_node_stack.hpp"

#include "../src/lf_mem_alloc_slot.hpp"

static_assert( alpha::concurrent::internal::is_callable_lifo_free_node_if_get_next<alpha::concurrent::internal::slot_header_of_array>::value, "T should have T::get_next() I/F" );
static_assert( alpha::concurrent::internal::is_callable_lifo_free_node_if_set_next<alpha::concurrent::internal::slot_header_of_array>::value, "T should have T::set_next() I/F" );
static_assert( alpha::concurrent::internal::is_callable_lifo_free_node_if_next_CAS<alpha::concurrent::internal::slot_header_of_array>::value, "T should have T::next_CAS() I/F" );
static_assert( alpha::concurrent::internal::is_callable_lifo_free_node_if<alpha::concurrent::internal::slot_header_of_array>::value, "T should have I/Fs" );

using test_free_node       = alpha::concurrent::internal::slot_header_of_array;
using test_free_node_stack = alpha::concurrent::internal::free_node_stack<test_free_node>;

TEST( FreeNodeStack, CanCall_DefaultConstruct )
{
	// Arrange
	alpha::concurrent::internal::alloc_only_chamber aoc( true, 4 * 1024 );

	// Act
	test_free_node_stack sut( &aoc );

	// Assert
}

TEST( FreeNodeStack, CanCall_InitPushPop_tofrom_free_node_stack )
{
	// Arrange
	alpha::concurrent::internal::alloc_only_chamber aoc( true, 4 * 1024 );
	int                                             test_int;
	test_free_node                                  sut_node( &test_int, nullptr );
	test_free_node_stack                            sut( &aoc );
	sut.unchk_push_stack_list_to_head( &sut_node );

	// Act
	test_free_node* p_ret = sut.pop_from_free_node_stack();

	// Assert
	EXPECT_EQ( p_ret, &sut_node );
}

TEST( FreeNodeStack, CanCall_PushPop_tofrom_free_node_stack )
{
	// Arrange
	alpha::concurrent::internal::alloc_only_chamber aoc( true, 4 * 1024 );
	int                                             test_int;
	test_free_node                                  sut_node( &test_int, nullptr );
	test_free_node_stack                            sut( &aoc );
	sut.push_to_free_node_stack_wo_hzd_chk( &sut_node );

	// Act
	test_free_node* p_ret = sut.pop_from_free_node_stack();

	// Assert
	EXPECT_EQ( p_ret, &sut_node );
}

TEST( FreeNodeStack, CanCall_PushPop_tofrom_tls_stack )
{
	// Arrange
	alpha::concurrent::internal::alloc_only_chamber aoc( true, 4 * 1024 );
	int                                             test_int;
	test_free_node                                  sut_node( &test_int, nullptr );
	test_free_node_stack                            sut( &aoc );
	test_free_node_stack::scoped_np_accessor_type   sut_acs = sut.tls_p_hazard_slot_stack_head_.get_tls_accessor();
	sut.push_to_tls_stack( &sut_node, sut_acs );

	// Act
	test_free_node* p_ret = sut.pop_from_tls_stack( sut_acs );

	// Assert
	EXPECT_EQ( p_ret, &sut_node );
}

TEST( FreeNodeStack, CanCall_PushPop_tofrom_consignment_stack )
{
	// Arrange
	alpha::concurrent::internal::alloc_only_chamber aoc( true, 4 * 1024 );
	int                                             test_int;
	test_free_node                                  sut_node( &test_int, nullptr );
	test_free_node_stack                            sut( &aoc );
	sut.nonlockchk_push_to_consignment_stack( &sut_node );

	// Act
	test_free_node* p_ret = sut.nonlockchk_pop_from_consignment_stack();

	// Assert
	EXPECT_EQ( p_ret, &sut_node );
}

TEST( FreeNodeStack, CanCall_PushPop )
{
	// Arrange
	alpha::concurrent::internal::alloc_only_chamber aoc( true, 4 * 1024 );
	int                                             test_int;
	test_free_node                                  sut_node( &test_int, nullptr );
	test_free_node_stack                            sut( &aoc );
	sut.push( &sut_node );

	// Act
	test_free_node* p_ret = sut.pop();

	// Assert
	EXPECT_EQ( p_ret, &sut_node );
}
