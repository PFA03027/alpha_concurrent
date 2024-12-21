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
}
