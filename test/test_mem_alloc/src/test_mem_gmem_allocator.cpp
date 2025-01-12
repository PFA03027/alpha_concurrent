/**
 * @file test_mem_gmem_allocator.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-22
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "gtest/gtest.h"

#include "alconcurrent/lf_mem_alloc.hpp"

TEST( Test_GMemAllocator, CanAllocate )
{
	// Arrange

	// Act
	auto p_ret = alpha::concurrent::gmem_allocate( 1024 * 4 );

	// Assert
	EXPECT_NE( p_ret, nullptr );

	// Cleanup
	alpha::concurrent::gmem_deallocate( p_ret );
}

TEST( Test_GMemAllocator, CanDeallocate_Then_ReturnTrue )
{
	// Arrange
	auto p_ret = alpha::concurrent::gmem_allocate( 1024 * 4 );
	EXPECT_NE( p_ret, nullptr );

	// Act
	bool ret = alpha::concurrent::gmem_deallocate( p_ret );

	// Assert
	EXPECT_TRUE( ret );

	// Cleanup
}

TEST( Test_GMemAllocator, DoDeallocateWithNullPtr_Then_ReturnFalse )
{
	// Arrange

	// Act
	bool ret = alpha::concurrent::gmem_deallocate( nullptr );

	// Assert
	EXPECT_FALSE( ret );

	// Cleanup
}

TEST( Test_GMemAllocator, Deallocated_DoDeallocateWithSamePtr_Then_ReturnFalse )
{
	// Arrange
	auto p_ret = alpha::concurrent::gmem_allocate( 1024 * 4 );
	EXPECT_NE( p_ret, nullptr );
	bool ret = alpha::concurrent::gmem_deallocate( p_ret );
	EXPECT_TRUE( ret );

	// Act
	ret = alpha::concurrent::gmem_deallocate( p_ret );

	// Assert
	EXPECT_FALSE( ret );

	// Cleanup
}

TEST( Test_GMemAllocator, CanAllocateWithBigSize )
{
	// Arrange

	// Act
	auto p_ret = alpha::concurrent::gmem_allocate( 1024 * 1024 );

	// Assert
	EXPECT_NE( p_ret, nullptr );
	bool ret = alpha::concurrent::gmem_deallocate( p_ret );
	EXPECT_TRUE( ret );

	// Cleanup
}

TEST( Test_GMemAllocator, CanAllocateWithOverBigSize )
{
	// Arrange

	// Act
	auto p_ret = alpha::concurrent::gmem_allocate( 1024 * 1024 * 5 );

	// Assert
	EXPECT_NE( p_ret, nullptr );
	bool ret = alpha::concurrent::gmem_deallocate( p_ret );
	EXPECT_TRUE( ret );

	// Cleanup
}
