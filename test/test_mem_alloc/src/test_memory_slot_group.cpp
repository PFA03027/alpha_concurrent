/**
 * @file test_memory_slot_group.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-19
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "gtest/gtest.h"

#include "mem_small_memory_slot.hpp"

using tut = alpha::concurrent::internal::memory_slot_group;

class Test_MemorySlotGroupBuffParam : public testing::TestWithParam<size_t> {
};

TEST_P( Test_MemorySlotGroupBuffParam, CanConstruct )
{
	// Arrange
	auto          cur_param = GetParam();
	const size_t  buff_size = sizeof( tut ) * 1000 + cur_param;
	unsigned char buff[buff_size];

	// Act
	tut* p_ret = tut::emplace_on_mem( buff, nullptr, buff_size, 15 );

	// Assert
	auto mn = tut::magic_number_value_;
	EXPECT_EQ( p_ret->magic_number_, mn );
	EXPECT_EQ( p_ret->p_list_mgr_, nullptr );
	EXPECT_EQ( sizeof( alpha::concurrent::internal::slot_link_info ) + 16, p_ret->one_slot_bytes_ );
	EXPECT_LE( 1, p_ret->num_slots_ );
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	EXPECT_LE( p_ret->data_, reinterpret_cast<unsigned char*>( &( p_ret->get_btinfo( 0 ) ) ) );
	EXPECT_LE( reinterpret_cast<unsigned char*>( &( p_ret->get_btinfo( static_cast<ssize_t>( p_ret->num_slots_ ) ) ) ), p_ret->p_slot_begin_ );
#endif
	EXPECT_LE( p_ret->data_, p_ret->p_slot_begin_ );
	EXPECT_LE( p_ret->p_slot_begin_, p_ret->p_slot_end_ );
	EXPECT_LE( p_ret->p_slot_end_, &( buff[sizeof( buff )] ) );
	EXPECT_EQ( reinterpret_cast<uintptr_t>( p_ret->p_slot_begin_ ) % alpha::concurrent::internal::allocated_mem_top::min_alignment_size_, 0 );
	EXPECT_EQ( reinterpret_cast<uintptr_t>( p_ret->p_slot_end_ ) % alpha::concurrent::internal::allocated_mem_top::min_alignment_size_, 0 );
	EXPECT_EQ( p_ret->ap_next_group_, nullptr );
	EXPECT_EQ( p_ret->ap_unassigned_slot_, p_ret->p_slot_begin_ );
}

INSTANTIATE_TEST_SUITE_P(
	buffsize_variation,
	Test_MemorySlotGroupBuffParam,
	testing::Values( 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ) );

//////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST( Test_MemorySlotGroup, IndexZero_DoGetSlotPointer_Then_same_to_begin )
{
	// Arrange
	unsigned char buff[sizeof( tut ) * 1000];
	tut*          p_sut = tut::emplace_on_mem( buff, nullptr, sizeof( tut ) * 1000, 15 );

	// Act
	auto p_ret = p_sut->get_slot_pointer( 0 );

	// Assert
	EXPECT_EQ( p_ret, reinterpret_cast<decltype( p_ret )>( p_sut->p_slot_begin_ ) );
}

#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
#else
TEST( Test_MemorySlotGroup, IndexMax_DoGetSlotPointer_Then_same_to_end )
{
	// Arrange
	unsigned char buff[sizeof( tut ) * 1000];
	tut*          p_sut = tut::emplace_on_mem( buff, nullptr, sizeof( tut ) * 1000, 15 );

	// Act
	auto p_ret = p_sut->get_slot_pointer( p_sut->num_slots_ );

	// Assert
	EXPECT_EQ( p_ret, reinterpret_cast<decltype( p_ret )>( p_sut->p_slot_end_ ) );
}
#endif

TEST( Test_MemorySlotGroup, NotYetAssign_DoAssignNewSlot_Then_same_to_begin )
{
	// Arrange
	unsigned char buff[sizeof( tut ) * 1000];
	tut*          p_sut = tut::emplace_on_mem( buff, nullptr, sizeof( tut ) * 1000, 15 );

	// Act
	auto p_ret = p_sut->assign_new_slot();

	// Assert
	EXPECT_EQ( reinterpret_cast<unsigned char*>( p_ret ), p_sut->p_slot_begin_ );
}

TEST( Test_MemorySlotGroup, AllAssigned_DoAssignNewSlot_Then_Nullptr )
{
	// Arrange
	unsigned char buff[sizeof( tut ) * 1000];
	tut*          p_sut = tut::emplace_on_mem( buff, nullptr, sizeof( tut ) * 1000, 15 );
	for ( size_t i = 0; i < ( p_sut->num_slots_ - 1 ); i++ ) {
		p_sut->assign_new_slot();
	}
	EXPECT_FALSE( p_sut->is_assigned_all_slots() );
	auto p_ret = p_sut->assign_new_slot();
	EXPECT_NE( p_ret, nullptr );
	EXPECT_LE( p_sut->p_slot_begin_, reinterpret_cast<unsigned char*>( p_ret ) );
	EXPECT_LT( reinterpret_cast<unsigned char*>( p_ret ), p_sut->p_slot_end_ );
	EXPECT_TRUE( p_sut->is_assigned_all_slots() );

	// Act
	p_ret = p_sut->assign_new_slot();

	// Assert
	EXPECT_EQ( p_ret, nullptr );
	EXPECT_TRUE( p_sut->is_assigned_all_slots() );
}
