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

#include "mem_small_memory_slot.hpp"

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
	EXPECT_EQ( sut.limit_bytes_for_one_memory_slot_group_, max_buffer_size );
	EXPECT_EQ( sut.next_allocating_buffer_bytes_, init_buffer_size );
	EXPECT_EQ( sut.ap_head_memory_slot_group_, nullptr );
	EXPECT_EQ( sut.ap_cur_assigning_memory_slot_group_, nullptr );
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
	EXPECT_EQ( sut.limit_bytes_for_one_memory_slot_group_, max_buffer_size );
	EXPECT_LT( init_buffer_size, sut.next_allocating_buffer_bytes_ );
	EXPECT_EQ( sut.ap_head_memory_slot_group_, nullptr );
	EXPECT_EQ( sut.ap_cur_assigning_memory_slot_group_, nullptr );
}

TEST( Test_MemorySlotGroupList, Empty_DoRequestAllocateMemorySlotGroup_Then_AddedOne )
{
	// Arrange
	constexpr size_t max_buffer_size  = 1024 * 4 * 100;
	constexpr size_t init_buffer_size = 1024 * 4;
	tut              sut( 15, max_buffer_size, init_buffer_size );

	// Act
	sut.request_allocate_memory_slot_group();

	// Assert
	EXPECT_NE( sut.ap_head_memory_slot_group_, nullptr );
	EXPECT_EQ( sut.ap_cur_assigning_memory_slot_group_, sut.ap_head_memory_slot_group_ );

	// Cleanup
	sut.clear_for_test();
}

TEST( Test_MemorySlotGroupList, OneElement_DoRequestAllocateMemorySlotGroup_Then_NextSizeIsSameToLimitSize )
{
	// Arrange
	constexpr size_t max_buffer_size  = 1024 * 4;
	constexpr size_t init_buffer_size = 1024 * 4;
	tut              sut( 15, max_buffer_size, init_buffer_size );

	sut.request_allocate_memory_slot_group();

	// Act
	sut.request_allocate_memory_slot_group();

	// Assert
	EXPECT_NE( sut.ap_head_memory_slot_group_, nullptr );
	EXPECT_NE( sut.ap_cur_assigning_memory_slot_group_, sut.ap_head_memory_slot_group_ );
	EXPECT_EQ( sut.next_allocating_buffer_bytes_, sut.limit_bytes_for_one_memory_slot_group_ );

	// Cleanup
	sut.clear_for_test();
}

TEST( Test_MemorySlotGroupList, Empty_DoAllocate_Then_ReturnNullptr )
{
	// Arrange
	constexpr size_t max_buffer_size  = 1024 * 4;
	constexpr size_t init_buffer_size = 1024 * 4;
	tut              sut( 15, max_buffer_size, init_buffer_size );

	// Act
	auto p = sut.allocate();

	// Assert
	EXPECT_EQ( p, nullptr );

	// Cleanup
	sut.clear_for_test();
}

TEST( Test_MemorySlotGroupList, DeallocateOneSlot_DoAllocate_Then_ReturnElement )
{
	// Arrange
	constexpr size_t max_buffer_size  = 1024 * 4;
	constexpr size_t init_buffer_size = 1024 * 4;
	tut              sut( 15, max_buffer_size, init_buffer_size );

	unsigned char buff_memory_slot_group[1024 * 4];
	auto          p_ret = alpha::concurrent::internal::memory_slot_group::emplace_on_mem( buff_memory_slot_group, nullptr, 1024 * 4, 15 );
	EXPECT_NE( p_ret->num_slots_, 0 );
	alpha::concurrent::internal::slot_link_info* p_sli1 = p_ret->assign_new_slot();
	EXPECT_NE( p_sli1, nullptr );

	// Act
	sut.deallocate( p_sli1 );
	auto p = sut.allocate();

	// Assert
	EXPECT_EQ( p, p_sli1 );

	// Cleanup
	sut.clear_for_test();
}

TEST( Test_MemorySlotGroupList, OneElement_DoAllocate_Then_ReturnElement )
{
	// Arrange
	constexpr size_t max_buffer_size  = 1024 * 4;
	constexpr size_t init_buffer_size = 1024 * 4;
	tut              sut( 15, max_buffer_size, init_buffer_size );

	sut.request_allocate_memory_slot_group();

	// Act
	auto p = sut.allocate();

	// Assert
	EXPECT_NE( p, nullptr );

	// Cleanup
	sut.clear_for_test();
}

TEST( Test_MemorySlotGroupList, FullyAssignedOneMore_DoAllocate_Then_ReturnValidSlot )
{
	// Arrange
	constexpr size_t max_buffer_size  = 1024 * 4;
	constexpr size_t init_buffer_size = 1024 * 4;
	tut              sut( 15, max_buffer_size, init_buffer_size );

	sut.request_allocate_memory_slot_group();
	while ( sut.allocate() != nullptr ) {
	}

	// Act
	sut.request_allocate_memory_slot_group();

	// Assert
	auto p = sut.allocate();
	EXPECT_NE( p, nullptr );

	// Cleanup
	sut.clear_for_test();
}

TEST( Test_MemorySlotGroupList, FullyAssignedOneMore_DoAllocate_Then_ReturnNullptr )
{
	// Arrange
	constexpr size_t max_buffer_size  = 1024 * 4;
	constexpr size_t init_buffer_size = 1024 * 4;
	tut              sut( 15, max_buffer_size, init_buffer_size );

	sut.request_allocate_memory_slot_group();
	size_t num_slots = 0;
	while ( sut.allocate() != nullptr ) {
		++num_slots;
	}

	// Act
	sut.request_allocate_memory_slot_group();
	sut.request_allocate_memory_slot_group();

	// Assert
	size_t additional_num_slots = 0;
	while ( sut.allocate() != nullptr ) {
		++additional_num_slots;
	}
	EXPECT_EQ( additional_num_slots, num_slots * 2 );

	// Cleanup
	sut.clear_for_test();
}