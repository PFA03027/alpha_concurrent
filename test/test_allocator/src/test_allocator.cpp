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

#include "alconcurrent/alloc_only_allocator.hpp"
#include "mmap_allocator.hpp"

#include "gtest/gtest.h"

constexpr size_t REQ_ALLOC_SIZE          = 1024;
constexpr size_t test_conf_pre_mmap_size = 1024 * 1024;

TEST( MMAP_Alocator, DO_max_size )
{
	// Arrange

	// Act
	auto mmap_alloc_ret = alpha::concurrent::internal::allocate_by_mmap( alpha::concurrent::internal::conf_max_mmap_alloc_size, 0 );

	// Assert
	ASSERT_NE( nullptr, mmap_alloc_ret.p_allocated_addr_ );
	EXPECT_GE( alpha::concurrent::internal::conf_max_mmap_alloc_size, mmap_alloc_ret.allocated_size_ );

	// Cleanup
	auto ret_unmap = alpha::concurrent::internal::deallocate_by_munmap( mmap_alloc_ret.p_allocated_addr_, mmap_alloc_ret.allocated_size_ );
	EXPECT_EQ( 0, ret_unmap );
}

TEST( MMAP_Alocator, DO_max_size_plus_one )
{
	// Arrange

	// Act
	auto mmap_alloc_ret = alpha::concurrent::internal::allocate_by_mmap( alpha::concurrent::internal::conf_max_mmap_alloc_size + 1, 0 );

	// Assert
	EXPECT_EQ( nullptr, mmap_alloc_ret.p_allocated_addr_ );
	EXPECT_EQ( 0, mmap_alloc_ret.allocated_size_ );
}

TEST( Alloc_only_class, Call_push )
{
	auto pre_status = alpha::concurrent::internal::get_alloc_mmap_status();

	{
		// Arrange
		alpha::concurrent::internal::alloc_only_chamber sut( true, 128 );
		void*                                           p_mem = sut.allocate( 55, 8 );
		EXPECT_NE( p_mem, nullptr );
	}

	// Act
	auto post_status = alpha::concurrent::internal::get_alloc_mmap_status();

	// Assert
	EXPECT_EQ( pre_status.active_size_, post_status.active_size_ );
}

TEST( Alloc_only_class, Call_dump )
{
	auto pre_status = alpha::concurrent::internal::get_alloc_mmap_status();

	{
		// Arrange
		alpha::concurrent::internal::alloc_only_chamber sut( true, 128 );
		void*                                           p_mem = sut.allocate( REQ_ALLOC_SIZE, alpha::concurrent::internal::default_align_size );
		EXPECT_NE( p_mem, nullptr );

		// Act
		sut.dump_to_log( alpha::concurrent::log_type::TEST, 't', 2 );
	}

	// Assert
	auto post_status = alpha::concurrent::internal::get_alloc_mmap_status();
	EXPECT_EQ( pre_status.active_size_, post_status.active_size_ );
}

TEST( Alloc_only_class, Call_allocating_only )
{
	auto pre_status = alpha::concurrent::internal::get_alloc_mmap_status();

	{
		// Arrange
		alpha::concurrent::internal::alloc_only_chamber sut( true, 128 );

		// Act
		void* p_mem = sut.allocate( REQ_ALLOC_SIZE, alpha::concurrent::internal::default_align_size );

		// Assert
		EXPECT_NE( p_mem, nullptr );
	}
	auto post_status = alpha::concurrent::internal::get_alloc_mmap_status();
	EXPECT_EQ( pre_status.active_size_, post_status.active_size_ );
}

TEST( Alloc_only_class, Do_append_allocation )
{
	auto pre_status = alpha::concurrent::internal::get_alloc_mmap_status();

	{
		// Arrange
		alpha::concurrent::internal::alloc_only_chamber sut( true, test_conf_pre_mmap_size );
		void*                                           p_mem = sut.allocate( test_conf_pre_mmap_size / 2, alpha::concurrent::internal::default_align_size );
		EXPECT_NE( p_mem, nullptr );

		// Act
		p_mem = sut.allocate( test_conf_pre_mmap_size / 2, alpha::concurrent::internal::default_align_size );

		// Assert
		EXPECT_NE( p_mem, nullptr );
	}
	auto post_status = alpha::concurrent::internal::get_alloc_mmap_status();
	EXPECT_EQ( pre_status.active_size_, post_status.active_size_ );
}

TEST( Alloc_only_class, Do_allocation_over_pre_mmap_size )
{
	auto pre_status = alpha::concurrent::internal::get_alloc_mmap_status();

	{
		// Arrange
		alpha::concurrent::internal::alloc_only_chamber sut( true, test_conf_pre_mmap_size );

		// Act
		void* p_mem = sut.allocate( test_conf_pre_mmap_size * 2, alpha::concurrent::internal::default_align_size );

		// Assert
		EXPECT_NE( p_mem, nullptr );
	}
	auto post_status = alpha::concurrent::internal::get_alloc_mmap_status();
	EXPECT_EQ( pre_status.active_size_, post_status.active_size_ );
}
