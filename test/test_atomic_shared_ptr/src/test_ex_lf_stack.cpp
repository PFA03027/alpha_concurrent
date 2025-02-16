/**
 * @file test_ex_lf_stack.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2025-02-16
 *
 * @copyright Copyright (c) 2025, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "alconcurrent/experiment/ex_lf_stack.hpp"

#include "gtest/gtest.h"

#include "test_derived_class.hpp"

TEST( ex_lf_stack_Class, CanConstruct )
{
	// Arrange

	// Act
	alpha::concurrent::ex_lf_stack<int> stack;

	// Assert
}

TEST( ex_lf_stack_Class, CanPush )
{
	// Arrange
	alpha::concurrent::ex_lf_stack<int> sut;

	// Act
	sut.push( 1 );

	// Assert
}

TEST( ex_lf_stack_Class, CanPop )
{
	// Arrange
	alpha::concurrent::ex_lf_stack<int> sut;
	sut.push( 1 );

	// Act
	auto opt = sut.pop();

	// Assert
	ASSERT_TRUE( opt.has_value() );
	EXPECT_EQ( 1, *opt );
}

TEST( ex_lf_stack_Class, Empty_CanPop )
{
	// Arrange
	alpha::concurrent::ex_lf_stack<int> sut;

	// Act
	auto opt = sut.pop();

	// Assert
	ASSERT_FALSE( opt.has_value() );
}

TEST( ex_lf_stack_Class, CanPushPop_AsFILO )
{
	// Arrange
	alpha::concurrent::ex_lf_stack<int> sut;
	sut.push( 1 );
	sut.push( 2 );

	// Act
	auto opt1 = sut.pop();
	auto opt2 = sut.pop();

	// Assert
	ASSERT_TRUE( opt1.has_value() );
	ASSERT_TRUE( opt2.has_value() );
	EXPECT_EQ( 2, *opt1 );
	EXPECT_EQ( 1, *opt2 );
}
