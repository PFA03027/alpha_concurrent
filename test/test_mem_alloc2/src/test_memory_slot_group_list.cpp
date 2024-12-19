/**
 * @file test_memory_slot_group_list.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-20
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "gtest/gtest.h"

#include "mem_slotgroup.hpp"

using tut = alpha::concurrent::internal::memory_slot_group_list;

TEST( Test_MemorySlotGroupList, CanConstruct )
{
	// Arrange
	constexpr size_t max_buffer_size  = 1024 * 4 * 100;
	constexpr size_t init_buffer_size = 1024 * 4;

	// Act
	tut sut( 15, max_buffer_size, init_buffer_size );

	// Assert
	EXPECT_EQ( sut.allocatable_bytes_, 15 );
	EXPECT_EQ( sut.max_buffer_bytes_of_memory_slot_group_, max_buffer_size );
	EXPECT_EQ( sut.next_allocating_buffer_bytes_, init_buffer_size );
	EXPECT_EQ( sut.ap_head_memory_slot_group_, nullptr );
	EXPECT_EQ( sut.ap_cur_assigning_memory_slot_group_, nullptr );
	EXPECT_EQ( sut.ap_head_unused_memory_slot_stack_, nullptr );
}

TEST( Test_MemorySlotGroupList, CanConstructWithTooSmallInitBuffSize )
{
	// Arrange
	constexpr size_t max_buffer_size  = 1024 * 4 * 100;
	constexpr size_t init_buffer_size = 1;

	// Act
	tut sut( 15, max_buffer_size, init_buffer_size );

	// Assert
	EXPECT_EQ( sut.allocatable_bytes_, 15 );
	EXPECT_EQ( sut.max_buffer_bytes_of_memory_slot_group_, max_buffer_size );
	EXPECT_LT( init_buffer_size, sut.next_allocating_buffer_bytes_ );
	EXPECT_EQ( sut.ap_head_memory_slot_group_, nullptr );
	EXPECT_EQ( sut.ap_cur_assigning_memory_slot_group_, nullptr );
	EXPECT_EQ( sut.ap_head_unused_memory_slot_stack_, nullptr );
}
