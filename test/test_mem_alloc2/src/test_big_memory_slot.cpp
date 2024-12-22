/**
 * @file test_big_memory_slot.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-22
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "gtest/gtest.h"

#include "mem_big_memory_slot.hpp"

using tut = alpha::concurrent::internal::big_memory_slot;

TEST( Test_BigMemorySlot, CanConstruct )
{
	// Arrange
	constexpr size_t buffer_size = sizeof( tut ) + 1024 * 4;
	unsigned char    buff[buffer_size];

	// Act
	tut* p_ret = tut::emplace_on_mem( buff, alpha::concurrent::internal::mem_type::BIG_MEM, buffer_size );

	// Assert
	EXPECT_EQ( p_ret->magic_number_, tut::magic_number_value_ );
	EXPECT_EQ( p_ret->buffer_size_, buffer_size );
	EXPECT_EQ( p_ret->ap_slot_next_, nullptr );
	auto unzip_info = p_ret->link_to_big_memory_slot_.load_allocation_info<tut>();
	EXPECT_EQ( unzip_info.mt_, alpha::concurrent::internal::mem_type::BIG_MEM );
	EXPECT_TRUE( unzip_info.is_used_ );
	EXPECT_NE( p_ret->check_validity_to_ownwer_and_get(), nullptr );
}

TEST( Test_BigMemorySlotList, CanConstruct )
{
	// Arrange

	// Act
	alpha::concurrent::internal::big_memory_slot_list sut;

	// Assert

	// Cleanup
	sut.clear_for_test();
}

TEST( Test_BigMemorySlotList, Empty_DoAllocate_Then_ReturnNullptr )
{
	// Arrange
	alpha::concurrent::internal::big_memory_slot_list sut;

	// Act
	auto p_ret = sut.reuse_allocate( 1024 * 4 );

	// Assert
	EXPECT_EQ( p_ret, nullptr );

	// Cleanup
	sut.clear_for_test();
}

TEST( Test_BigMemorySlotList, Empty_DoDeallocateWithNonRelatedPtr_Then_ReturnFalse )
{
	// Arrange
	alpha::concurrent::internal::big_memory_slot_list sut;
	unsigned char                                     buff[1024 * 4];

	// Act
	auto ret = sut.deallocate( reinterpret_cast<alpha::concurrent::internal::big_memory_slot*>( buff ) );

	// Assert
	EXPECT_FALSE( ret );

	// Cleanup
	sut.clear_for_test();
}

TEST( Test_BigMemorySlotList, Empty_DoAllocatNewly_Then_ReturnValidPtr )
{
	// Arrange
	alpha::concurrent::internal::big_memory_slot_list sut;

	// Act
	alpha::concurrent::internal::big_memory_slot* p_slot = sut.allocate_newly( 1024 * 4 );

	// Assert
	ASSERT_NE( nullptr, p_slot );
	EXPECT_LE( 1024 * 4, p_slot->max_allocatable_size() );
	EXPECT_EQ( p_slot->link_to_big_memory_slot_.load_mem_type(), alpha::concurrent::internal::mem_type::BIG_MEM );

	// Cleanup
	sut.deallocate( p_slot );
	sut.clear_for_test();
}

TEST( Test_BigMemorySlotList, Empty_DoDeallocateNullptr_Then_ReturnWithWarning )
{
	// Arrange
	alpha::concurrent::internal::big_memory_slot_list sut;

	// Act
	sut.deallocate( nullptr );

	// Assert

	// Cleanup
	sut.clear_for_test();
}

TEST( Test_BigMemorySlotList, OneElement_DoReuseAllocateWithSameSize_Then_ReturnValidPtr )
{
	// Arrange
	alpha::concurrent::internal::big_memory_slot_list sut;
	alpha::concurrent::internal::big_memory_slot*     p_slot = sut.allocate_newly( 1024 * 4 );
	EXPECT_NE( nullptr, p_slot );
	sut.deallocate( p_slot );

	// Act
	auto p_ret = sut.reuse_allocate( 1024 * 4 );

	// Assert
	EXPECT_EQ( p_ret, p_slot );

	// Cleanup
	sut.deallocate( p_ret );
	sut.clear_for_test();
}

TEST( Test_BigMemorySlotList, OneElement_DoReuseAllocateWithOverSize_Then_ReturnNullPtr )
{
	// Arrange
	alpha::concurrent::internal::big_memory_slot_list sut;
	alpha::concurrent::internal::big_memory_slot*     p_slot = sut.allocate_newly( 1024 * 4 );
	EXPECT_NE( nullptr, p_slot );
	sut.deallocate( p_slot );

	// Act
	auto p_ret = sut.reuse_allocate( 1024 * 8 );

	// Assert
	EXPECT_EQ( p_ret, nullptr );

	// Cleanup
	sut.clear_for_test();
}

TEST( Test_BigMemorySlotList, TweElement_DoReuseAllocateWithOverSize_Then_ReturnValidPtr )
{
	// Arrange
	alpha::concurrent::internal::big_memory_slot_list sut;

	alpha::concurrent::internal::big_memory_slot* p_slot1 = sut.allocate_newly( 1024 * 8 );
	EXPECT_NE( nullptr, p_slot1 );
	sut.deallocate( p_slot1 );
	alpha::concurrent::internal::big_memory_slot* p_slot2 = sut.allocate_newly( 1024 * 4 );
	EXPECT_NE( nullptr, p_slot2 );
	sut.deallocate( p_slot2 );

	// Act
	auto p_ret = sut.reuse_allocate( 1024 * 8 );

	// Assert
	EXPECT_EQ( p_ret, p_slot1 );

	// Cleanup
	sut.deallocate( p_ret );
	sut.clear_for_test();
}

TEST( Test_BigMemorySlotList, Empty_DoAllocatNewlyWithTooBigSize_Then_ReturnValidPtr )
{
	// Arrange
	alpha::concurrent::internal::big_memory_slot_list sut;

	// Act
	alpha::concurrent::internal::big_memory_slot* p_slot = sut.allocate_newly( alpha::concurrent::internal::big_memory_slot_list::too_big_memory_slot_buffer_size_threshold_ + 1 );

	// Assert
	ASSERT_NE( nullptr, p_slot );
	EXPECT_EQ( p_slot->link_to_big_memory_slot_.load_mem_type(), alpha::concurrent::internal::mem_type::OVER_BIG_MEM );

	// Cleanup
	sut.deallocate( p_slot );
	sut.clear_for_test();
}

TEST( Test_BigMemorySlotList, DeallocateTooBigSize_DoReuseAllocate_Then_ReturnNullPtr )
{
	// Arrange
	alpha::concurrent::internal::big_memory_slot_list sut;

	alpha::concurrent::internal::big_memory_slot* p_slot = sut.allocate_newly( alpha::concurrent::internal::big_memory_slot_list::too_big_memory_slot_buffer_size_threshold_ + 1 );
	ASSERT_NE( nullptr, p_slot );
	EXPECT_EQ( p_slot->link_to_big_memory_slot_.load_mem_type(), alpha::concurrent::internal::mem_type::OVER_BIG_MEM );
	sut.deallocate( p_slot );

	// Act
	auto p_ret = sut.reuse_allocate( 1024 * 8 );

	// Assert
	EXPECT_EQ( p_ret, nullptr );

	// Cleanup
	sut.clear_for_test();
}

TEST( Test_BigMemorySlotList, DeallocateOverCacheSize_DoReuseAllocate_Then_ReturnNullPtr )
{
	// Arrange
	alpha::concurrent::internal::big_memory_slot_list sut;

	alpha::concurrent::internal::big_memory_slot* p_slot1 = sut.allocate_newly( alpha::concurrent::internal::big_memory_slot_list::too_big_memory_slot_buffer_size_threshold_ / 2 );
	ASSERT_NE( nullptr, p_slot1 );
	EXPECT_EQ( p_slot1->link_to_big_memory_slot_.load_mem_type(), alpha::concurrent::internal::mem_type::BIG_MEM );
	sut.deallocate( p_slot1 );
	alpha::concurrent::internal::big_memory_slot* p_slot2 = sut.allocate_newly( alpha::concurrent::internal::big_memory_slot_list::too_big_memory_slot_buffer_size_threshold_ / 2 + 1 );
	ASSERT_NE( nullptr, p_slot2 );
	EXPECT_EQ( p_slot2->link_to_big_memory_slot_.load_mem_type(), alpha::concurrent::internal::mem_type::BIG_MEM );
	sut.deallocate( p_slot2 );

	auto p_ret1 = sut.reuse_allocate( 1024 * 8 );
	EXPECT_NE( p_ret1, nullptr );

	// Act
	auto p_ret2 = sut.reuse_allocate( 1024 * 8 );

	// Assert
	EXPECT_EQ( p_ret2, nullptr );

	// Cleanup
	sut.deallocate( p_slot1 );
	sut.clear_for_test();
}
