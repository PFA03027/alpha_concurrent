/**
 * @file test_lf_stack2.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-03
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include <pthread.h>

#include <cstdint>
#include <deque>
#include <future>
#include <iostream>
#include <mutex>
#include <vector>

#include "gtest/gtest.h"

#include "alconcurrent/lf_stack.hpp"

#include "test_type_variation.hpp"

class lfStackTest : public ::testing::Test {
protected:
	virtual void SetUp()
	{
	}

	virtual void TearDown()
	{
	}
};

TEST_F( lfStackTest, CallDefaultConstruct )
{
	// Arrange

	// Act
	alpha::concurrent::stack_list<int> sut;

	// Assert
}

TEST_F( lfStackTest, CallPopFromEmpty )
{
	// Arrange
	alpha::concurrent::stack_list<int> sut;

	// Act
	auto ret = sut.pop();

	// Assert
	EXPECT_FALSE( ret.has_value() );
}

TEST_F( lfStackTest, CallPushPopOne )
{
	// Arrange
	alpha::concurrent::stack_list<int> sut;

	// Act
	sut.push( 1 );
	auto ret = sut.pop();

	// Assert
	ASSERT_TRUE( ret.has_value() );
	EXPECT_EQ( ret.value(), 1 );
}

TEST_F( lfStackTest, CallPushPopTwo )
{
	// Arrange
	alpha::concurrent::stack_list<int> sut;

	// Act
	sut.push( 1 );
	sut.push( 2 );
	auto ret1 = sut.pop();
	auto ret2 = sut.pop();

	// Assert
	ASSERT_TRUE( ret1.has_value() );
	EXPECT_EQ( ret1.value(), 2 );
	ASSERT_TRUE( ret2.has_value() );
	EXPECT_EQ( ret2.value(), 1 );
}

TEST_F( lfStackTest, DoEmplace )
{
	// Arrange
	alpha::concurrent::stack_list<partly_userdefined_5_special_op_no_default_constructor> sut;

	// Act
	sut.emplace( 2, 3.0 );

	// Arrange
	auto poped_data = sut.pop();
	ASSERT_TRUE( poped_data.has_value() );
	EXPECT_EQ( poped_data.value().x_, 2 );
	EXPECT_EQ( poped_data.value().y_, 3.0 );
}

TEST_F( lfStackTest, Pointer1 )
{
	// Arrange
	using test_fifo_type3 = alpha::concurrent::stack_list<int*>;
	test_fifo_type3* p_test_obj;

	std::cout << "Pointer test#1" << std::endl;
	p_test_obj  = new test_fifo_type3( 8 );
	int* p_data = new int();

	// Act
	p_test_obj->push( p_data );

	// Assert
	delete p_test_obj;
	delete p_data;
}

TEST_F( lfStackTest, Pointer2 )
{
	using test_fifo_type3 = alpha::concurrent::stack_list<int*>;
	test_fifo_type3* p_test_obj;

	std::cout << "Pointer test#2" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push( new int() );
	auto ret = p_test_obj->pop();

	ASSERT_TRUE( ret.has_value() );

	delete ret.value();
	delete p_test_obj;

	std::cout << "End Pointer test" << std::endl;
}

TEST_F( lfStackTest, CanCal_With_Unique_ptr )
{
	// Arrange
	using test_fifo_type3 = alpha::concurrent::stack_list<std::unique_ptr<int>>;
	test_fifo_type3 test_obj;

	std::unique_ptr<int> up_tv( new int );
	*up_tv = 12;

	// Act
	test_obj.push( std::move( up_tv ) );
	auto ret = test_obj.pop();

	// Assert
	ASSERT_TRUE( ret.has_value() );
	ASSERT_NE( ret.value(), nullptr );
	EXPECT_EQ( *( ret.value() ), 12 );
}

class array_test {
public:
	array_test( void )
	{
		x = 1;
		return;
	}

	~array_test()
	{
		printf( "called destructor of array_test\n" );
		return;
	}

private:
	int x;
};

TEST_F( lfStackTest, Array1 )
{
	// Arrange
	using test_fifo_type3 = alpha::concurrent::stack_list<array_test[]>;
	test_fifo_type3* p_test_obj;

	std::cout << "Array array_test[] test#1" << std::endl;
	p_test_obj         = new test_fifo_type3( 8 );
	array_test* p_data = new array_test[2];

	// Act
	p_test_obj->push( p_data );

	// Assert
	delete p_test_obj;
	delete[] p_data;
}

TEST_F( lfStackTest, Array2 )
{
	using test_fifo_type3 = alpha::concurrent::stack_list<array_test[]>;
	test_fifo_type3* p_test_obj;

	std::cout << "Array array_test[] test#2" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push( new array_test[2] );
	auto ret = p_test_obj->pop();

	ASSERT_TRUE( ret.has_value() );

	delete[] ret.value();
	delete p_test_obj;

	std::cout << "Array array_test[] test" << std::endl;
}

TEST_F( lfStackTest, FixedArray1 )
{
	using test_fifo_type3 = alpha::concurrent::stack_list<array_test[2]>;
	test_fifo_type3* p_test_obj;

	std::cout << "Array array_test[2] test#1" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	array_test tmp_data[2];
	p_test_obj->push( tmp_data );

	delete p_test_obj;
}

TEST_F( lfStackTest, FixedArray2 )
{
	using test_fifo_type3 = alpha::concurrent::stack_list<array_test[2]>;
	test_fifo_type3* p_test_obj;

	std::cout << "Array array_test[2] test#2" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	array_test tmp_data[2];

	p_test_obj->push( tmp_data );
	auto ret = p_test_obj->pop( tmp_data );

	ASSERT_TRUE( ret );

	delete p_test_obj;

	std::cout << "Array array_test[2] test" << std::endl;
}
