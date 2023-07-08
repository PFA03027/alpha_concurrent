/**
 * @file test_conf.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief Test for conf_logger.hpp
 * @version 0.1
 * @date 2022-12-30
 *
 * @copyright Copyright (c) 2022, Teruaki Ata <PFA03027@nifty.com>
 *
 */

#include "alloc_only_allocator.hpp"
#include "mmap_allocator.hpp"

#include "gtest/gtest.h"

constexpr size_t REQ_ALLOC_SIZE = 1024;

TEST( Alloc_only_class, Call_push )
{
	// Arrange
	auto mmap_alloc_ret = alpha::concurrent::internal::allocate_by_mmap( REQ_ALLOC_SIZE, alpha::concurrent::internal::default_align_size );

	// Act
	alpha::concurrent::internal::alloc_chamber_head::get_inst().push_alloc_mem( mmap_alloc_ret.p_allocated_addr_, mmap_alloc_ret.allocated_size_ );

	// Assert
	alpha::concurrent::internal::alloc_chamber_head::get_inst().dump_to_log( alpha::concurrent::log_type::TEST, 't', 1 );
}

TEST( Alloc_only_class, Call_dump )
{
	// Arrange
	alpha::concurrent::internal::allocating_only( REQ_ALLOC_SIZE, alpha::concurrent::internal::default_align_size );

	// Act
	alpha::concurrent::internal::alloc_chamber_head::get_inst().dump_to_log( alpha::concurrent::log_type::TEST, 't', 2 );

	// Assert
}

TEST( Alloc_only_class, Call_allocating_only )
{
	// Arrange

	// Act
	alpha::concurrent::internal::allocating_only( REQ_ALLOC_SIZE, alpha::concurrent::internal::default_align_size );

	// Assert
	alpha::concurrent::internal::alloc_chamber_head::get_inst().dump_to_log( alpha::concurrent::log_type::TEST, 't', 3 );
}

TEST( Alloc_only_class, Do_append_allocation )
{
	// Arrange
	alpha::concurrent::internal::allocating_only( alpha::concurrent::internal::conf_pre_mmap_size / 2, alpha::concurrent::internal::default_align_size );

	// Act
	alpha::concurrent::internal::allocating_only( alpha::concurrent::internal::conf_pre_mmap_size / 2, alpha::concurrent::internal::default_align_size );

	// Assert
	alpha::concurrent::internal::alloc_chamber_head::get_inst().dump_to_log( alpha::concurrent::log_type::TEST, 't', 4 );
}

TEST( Alloc_only_class, Do_allocation_over_pre_mmap_size )
{
	// Arrange

	// Act
	alpha::concurrent::internal::allocating_only( alpha::concurrent::internal::conf_pre_mmap_size * 2, alpha::concurrent::internal::default_align_size );

	// Assert
	alpha::concurrent::internal::alloc_chamber_head::get_inst().dump_to_log( alpha::concurrent::log_type::TEST, 't', 5 );
}
