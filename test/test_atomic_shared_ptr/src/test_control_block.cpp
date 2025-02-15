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

#include "alconcurrent/experiment/internal/atomic_shared_ptr.hpp"

#include "gtest/gtest.h"

#include "test_derived_class.hpp"

TEST( ControlBlock_Class, CanConstruct )
{
	// Arrange
	int* p = new int;

	// Act
	alpha::concurrent::internal::control_block<int> cb( p );

	// Assert
}

TEST( ControlBlock_Class, CanConstructWithDeleter )
{
	// Arrange
	int* p       = new int;
	auto deleter = []( int* p_arg ) { delete p_arg; };

	// Act
	alpha::concurrent::internal::control_block<int, decltype( deleter )> cb( p, deleter );

	// Assert
}

TEST( ControlBlock_Class, CanGetResourcePtr )
{
	// Arrange
	int*                                            p = new int;
	alpha::concurrent::internal::control_block<int> cb( p );

	// Act
	void* ptr = cb.get_resource_ptr();

	// Assert
	EXPECT_EQ( p, ptr );
}

TEST( ControlBlock_Class, CanReleaseResource )
{
	// Arrange
	int*                                            p = new int;
	alpha::concurrent::internal::control_block<int> cb( p );

	// Act
	cb.release_resource();

	// Assert
	EXPECT_EQ( nullptr, cb.get_resource_ptr() );
}
