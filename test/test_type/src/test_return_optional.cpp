/*
 * test_type.cpp
 *
 *  Created on: 2021/10/31
 *      Author: alpha
 */

#include <cstdio>
#include <cstdlib>

#include <memory>
#include <type_traits>

#include "gtest/gtest.h"

#include "alconcurrent/internal/return_optional.hpp"

TEST( TestType_ReturnOptional_WithTrivialType, DoDefaultConstruct_Then_NotHasValue )
{
	// Arrange
	// Act
	alpha::concurrent::return_optional<int> sut;

	// Assert
	EXPECT_FALSE( sut.has_value() );
	EXPECT_THROW( sut.value(), alpha::concurrent::bad_return_optional_access );
}

TEST( TestType_ReturnOptional_WithTrivialType, DoConstructWithNullOpt_Then_NotHasValue )
{
	// Arrange
	// Act
	alpha::concurrent::return_optional<int> sut( alpha::concurrent::return_nullopt );

	// Assert
	EXPECT_FALSE( sut.has_value() );
	EXPECT_THROW( sut.value(), alpha::concurrent::bad_return_optional_access );
}

TEST( TestType_ReturnOptional_WithTrivialType, DoConstructWithIntValue_Then_HasValue )
{
	// Arrange
	// Act
	alpha::concurrent::return_optional<int> sut( 1 );

	// Assert
	EXPECT_TRUE( sut.has_value() );
	ASSERT_NO_THROW( sut.value() );
	EXPECT_EQ( sut.value(), 1 );
}

TEST( TestType_ReturnOptional_WithTrivialType, DoCopyConstruct_Then_HasValue )
{
	// Arrange
	alpha::concurrent::return_optional<int> data( 1 );

	// Act
	alpha::concurrent::return_optional<int> sut( data );

	// Assert
	EXPECT_TRUE( data.has_value() );
	ASSERT_NO_THROW( data.value() );
	EXPECT_EQ( data.value(), 1 );

	EXPECT_TRUE( sut.has_value() );
	ASSERT_NO_THROW( sut.value() );
	EXPECT_EQ( sut.value(), 1 );
}

TEST( TestType_ReturnOptional_WithTrivialType, DoMoveConstruct_Then_HasValue )
{
	// Arrange
	alpha::concurrent::return_optional<int> data( 1 );

	// Act
	alpha::concurrent::return_optional<int> sut( std::move( data ) );

	// Assert
	EXPECT_TRUE( data.has_value() );
	ASSERT_NO_THROW( data.value() );

	EXPECT_TRUE( sut.has_value() );
	ASSERT_NO_THROW( sut.value() );
	EXPECT_EQ( sut.value(), 1 );
}

TEST( TestType_ReturnOptional_WithTrivialType, NotHaveValue_DoCopyAssingment_Then_HasValue )
{
	// Arrange
	int                                     data = 1;
	alpha::concurrent::return_optional<int> sut;
	EXPECT_FALSE( sut.has_value() );

	// Act
	sut = data;

	// Assert
	EXPECT_TRUE( sut.has_value() );
	ASSERT_NO_THROW( sut.value() );
	EXPECT_EQ( sut.value(), 1 );
}

TEST( TestType_ReturnOptional_WithTrivialType, HaveValue_DoCopyAssingmentNotHaveValue_Then_NotHasValue )
{
	// Arrange
	alpha::concurrent::return_optional<int> data;
	alpha::concurrent::return_optional<int> sut( 1 );
	EXPECT_TRUE( sut.has_value() );

	// Act
	sut = data;

	// Assert
	EXPECT_FALSE( data.has_value() );
	ASSERT_THROW( data.value(), alpha::concurrent::bad_return_optional_access );

	EXPECT_FALSE( sut.has_value() );
	ASSERT_THROW( sut.value(), alpha::concurrent::bad_return_optional_access );
}

TEST( TestType_ReturnOptional_WithTrivialType, NotHaveValue_DoCopyAssingmentHaveValue_Then_HasValue )
{
	// Arrange
	alpha::concurrent::return_optional<int> data( 1 );
	alpha::concurrent::return_optional<int> sut;

	// Act
	sut = data;

	// Assert
	EXPECT_TRUE( data.has_value() );
	ASSERT_NO_THROW( data.value() );
	EXPECT_EQ( data.value(), 1 );

	EXPECT_TRUE( sut.has_value() );
	ASSERT_NO_THROW( sut.value() );
	EXPECT_EQ( sut.value(), 1 );
}

TEST( TestType_ReturnOptional_WithTrivialType, HaveValue_DoCopyAssingmentHaveValue_Then_HasValue )
{
	// Arrange
	alpha::concurrent::return_optional<int> data( 1 );
	alpha::concurrent::return_optional<int> sut( 2 );
	EXPECT_EQ( sut.value(), 2 );

	// Act
	sut = data;

	// Assert
	EXPECT_TRUE( data.has_value() );
	ASSERT_NO_THROW( data.value() );
	EXPECT_EQ( data.value(), 1 );

	EXPECT_TRUE( sut.has_value() );
	ASSERT_NO_THROW( sut.value() );
	EXPECT_EQ( sut.value(), 1 );
}

TEST( TestType_ReturnOptional_WithTrivialType, NotHaveValue_DoCopyAssingmentNotHaveValue_Then_NotHasValue )
{
	// Arrange
	alpha::concurrent::return_optional<int> data;
	alpha::concurrent::return_optional<int> sut;

	// Act
	sut = data;

	// Assert
	EXPECT_FALSE( data.has_value() );
	ASSERT_THROW( data.value(), alpha::concurrent::bad_return_optional_access );

	EXPECT_FALSE( sut.has_value() );
	ASSERT_THROW( sut.value(), alpha::concurrent::bad_return_optional_access );
}

TEST( TestType_ReturnOptional_WithTrivialType, NotHaveValue_DoMoveAssingment_Then_HasValue )
{
	// Arrange
	int                                     data = 1;
	alpha::concurrent::return_optional<int> sut;
	EXPECT_FALSE( sut.has_value() );

	// Act
	sut = std::move( data );

	// Assert
	EXPECT_TRUE( sut.has_value() );
	ASSERT_NO_THROW( sut.value() );
	EXPECT_EQ( sut.value(), 1 );
}

TEST( TestType_ReturnOptional_WithTrivialType, HaveValue_DoMoveAssingmentNotHaveValue_Then_NotHasValue )
{
	// Arrange
	alpha::concurrent::return_optional<int> data;
	alpha::concurrent::return_optional<int> sut( 1 );
	EXPECT_TRUE( sut.has_value() );

	// Act
	sut = std::move( data );

	// Assert
	EXPECT_FALSE( data.has_value() );
	ASSERT_THROW( data.value(), alpha::concurrent::bad_return_optional_access );

	EXPECT_FALSE( sut.has_value() );
	ASSERT_THROW( sut.value(), alpha::concurrent::bad_return_optional_access );
}

TEST( TestType_ReturnOptional_WithTrivialType, NotHaveValue_DoMoveAssingmentHaveValue_Then_HasValue )
{
	// Arrange
	alpha::concurrent::return_optional<int> data( 1 );
	alpha::concurrent::return_optional<int> sut;

	// Act
	sut = std::move( data );

	// Assert
	EXPECT_TRUE( data.has_value() );
	ASSERT_NO_THROW( data.value() );
	EXPECT_EQ( data.value(), 1 );

	EXPECT_TRUE( sut.has_value() );
	ASSERT_NO_THROW( sut.value() );
	EXPECT_EQ( sut.value(), 1 );
}

TEST( TestType_ReturnOptional_WithTrivialType, HaveValue_DoMoveAssingmentHaveValue_Then_HasValue )
{
	// Arrange
	alpha::concurrent::return_optional<int> data( 1 );
	alpha::concurrent::return_optional<int> sut( 2 );
	EXPECT_EQ( sut.value(), 2 );

	// Act
	sut = std::move( data );

	// Assert
	EXPECT_TRUE( data.has_value() );
	ASSERT_NO_THROW( data.value() );
	EXPECT_EQ( data.value(), 1 );

	EXPECT_TRUE( sut.has_value() );
	ASSERT_NO_THROW( sut.value() );
	EXPECT_EQ( sut.value(), 1 );
}

TEST( TestType_ReturnOptional_WithTrivialType, NotHaveValue_DoMoveAssingmentNotHaveValue_Then_NotHasValue )
{
	// Arrange
	alpha::concurrent::return_optional<int> data;
	alpha::concurrent::return_optional<int> sut;

	// Act
	sut = std::move( data );

	// Assert
	EXPECT_FALSE( data.has_value() );
	ASSERT_THROW( data.value(), alpha::concurrent::bad_return_optional_access );

	EXPECT_FALSE( sut.has_value() );
	ASSERT_THROW( sut.value(), alpha::concurrent::bad_return_optional_access );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

TEST( TestType_ReturnOptional_WithMoveOnlyType, DoDefaultConstruct_Then_HasValue )
{
	// Arrange
	// Act
	alpha::concurrent::return_optional<std::unique_ptr<int>> sut;

	// Assert
	EXPECT_FALSE( sut.has_value() );
	EXPECT_THROW( sut.value(), alpha::concurrent::bad_return_optional_access );
}

TEST( TestType_ReturnOptional_WithMoveOnlyType, DoConstructWithValueByMove_Then_HasValue )
{
	// Arrange
	int* p_data = new int( 1 );

	// Act
	alpha::concurrent::return_optional<std::unique_ptr<int>> sut { std::unique_ptr<int>( p_data ) };

	// Assert
	EXPECT_TRUE( sut.has_value() );
	ASSERT_NO_THROW( sut.value() );
	EXPECT_EQ( sut.value().get(), p_data );
}

TEST( TestType_ReturnOptional_WithMoveOnlyType, DoMoveConstruct_Then_HasValue )
{
	// Arrange
	int*                                                     p_data = new int( 1 );
	alpha::concurrent::return_optional<std::unique_ptr<int>> data { std::unique_ptr<int>( p_data ) };

	// Act
	alpha::concurrent::return_optional<std::unique_ptr<int>> sut { std::move( data ) };

	// Assert
	EXPECT_TRUE( data.has_value() );
	ASSERT_NO_THROW( data.value() );
	EXPECT_EQ( data.value().get(), nullptr );

	EXPECT_TRUE( sut.has_value() );
	ASSERT_NO_THROW( sut.value() );
	EXPECT_EQ( sut.value().get(), p_data );
}

TEST( TestType_ReturnOptional_WithMoveOnlyType, NotHaveValue_DoMoveAssignment_Then_HasValue )
{
	// Arrange
	int*                                                     p_data = new int( 1 );
	std::unique_ptr<int>                                     up_data( p_data );
	alpha::concurrent::return_optional<std::unique_ptr<int>> sut;

	// Act
	sut = std::move( up_data );

	// Assert
	EXPECT_EQ( up_data.get(), nullptr );

	EXPECT_TRUE( sut.has_value() );
	ASSERT_NO_THROW( sut.value() );
	EXPECT_EQ( sut.value().get(), p_data );
}
