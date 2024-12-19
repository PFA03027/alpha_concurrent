/**
 * @file test_allocation_mem_top.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-18
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "gtest/gtest.h"

#include "mem_slotgroup.hpp"

using tut = alpha::concurrent::internal::allocated_mem_top;

TEST( Test_AllocatedMemTop, CanConstruct )
{
	// Arrange
	unsigned char buff[sizeof( tut ) * 10];

	// Act
	tut* p_ret = tut::emplace_on_mem<alpha::concurrent::internal::memory_slot_group>( buff, nullptr, alpha::concurrent::internal::mem_type::OVER_BIG_MEM, true );

	// Assert
	EXPECT_EQ( reinterpret_cast<uintptr_t>( p_ret ), reinterpret_cast<uintptr_t>( buff ) );
}

TEST( Test_AllocatedMemTop, DoGetStructureAddr )
{
	// Arrange
	unsigned char buff[sizeof( tut ) * 10];
	tut*          p_sut = tut::emplace_on_mem<alpha::concurrent::internal::memory_slot_group>( buff, nullptr, alpha::concurrent::internal::mem_type::OVER_BIG_MEM, true );

	// Act
	tut* p_ret = tut::get_structure_addr( reinterpret_cast<void*>( p_sut->data_ ) );

	// Assert
	EXPECT_EQ( reinterpret_cast<uintptr_t>( p_ret ), reinterpret_cast<uintptr_t>( buff ) );
}

TEST( Test_AllocatedMemTop, DoLoadAllocationInfo )
{
	// Arrange
	unsigned char buff[sizeof( tut ) * 10];
	tut*          p_sut = tut::emplace_on_mem<alpha::concurrent::internal::memory_slot_group>( buff, nullptr, alpha::concurrent::internal::mem_type::OVER_BIG_MEM, true );

	// Act
	auto ret = p_sut->load_allocation_info<alpha::concurrent::internal::memory_slot_group>();

	// Assert
	EXPECT_EQ( ret.p_mgr_, nullptr );
	EXPECT_EQ( ret.mt_, alpha::concurrent::internal::mem_type::OVER_BIG_MEM );
	EXPECT_TRUE( ret.is_used_ );
}
