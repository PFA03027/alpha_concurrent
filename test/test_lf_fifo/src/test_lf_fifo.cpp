//============================================================================
// Name        : test_lf_fifo.cpp
// Author      :
// Version     :
// Copyright   : Copyright (C) 2020 by Teruaki Ata <PFA03027@nifty.com>
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <pthread.h>

#include <cstdint>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <random>

#include "gtest/gtest.h"

#include "alconcurrent/lf_fifo.hpp"
#include "alconcurrent/lf_mem_alloc.hpp"

constexpr int            num_thread = 1;   // Tested until 128.
constexpr std::uintptr_t loop_num   = 10000;

using test_fifo_type = alpha::concurrent::fifo_list<std::uintptr_t>;

pthread_barrier_t global_shared_barrier;

class lffifoTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		alpha::concurrent::GetErrorWarningLogCountAndReset( nullptr, nullptr );
	}

	void TearDown() override
	{
		int cw, ce;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
		EXPECT_EQ( ce, 0 );
		EXPECT_EQ( cw, 0 );
	}
};

TEST_F( lffifoTest, Pointer1 )
{
	// Arrange
	using test_fifo_type3 = alpha::concurrent::fifo_list<int*>;
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

TEST_F( lffifoTest, Pointer2 )
{
	// Arrange
	using test_fifo_type3 = alpha::concurrent::fifo_list<int*>;
	test_fifo_type3* p_test_obj;

	std::cout << "Pointer test#2" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	// Act
	p_test_obj->push( new int() );
	auto ret = p_test_obj->pop();

	// Assert
	ASSERT_TRUE( ret.has_value() );

	delete ret.value();
	delete p_test_obj;
}

TEST_F( lffifoTest, CanCal_With_Unique_ptr )
{
	// Arrange
	using test_fifo_type3 = alpha::concurrent::fifo_list<std::unique_ptr<int>>;
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

	int x;

private:
};

TEST_F( lffifoTest, Array1 )
{
	// Arrange
	using test_fifo_type3 = alpha::concurrent::fifo_list<array_test[]>;
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

TEST_F( lffifoTest, Array2 )
{
	using test_fifo_type3 = alpha::concurrent::fifo_list<array_test[]>;
	test_fifo_type3* p_test_obj;

	std::cout << "Array array_test[] test#2" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push( new array_test[2] );
	auto ret = p_test_obj->pop();

	ASSERT_TRUE( ret.has_value() );

	delete[] ret.value();
	delete p_test_obj;
}

// 以下はtrivially_copyableではないクラスでfifoを使用する場合のテストケース。
TEST_F( lffifoTest, FixedArray1 )
{
	using test_fifo_type3 = alpha::concurrent::fifo_list<array_test[2]>;
	test_fifo_type3* p_test_obj;

	std::cout << "Array array_test[2] test#1" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	array_test tmp_data[2];
	tmp_data[0].x = 2;
	tmp_data[1].x = 3;
	p_test_obj->push( tmp_data );

	delete p_test_obj;
}

TEST_F( lffifoTest, FixedArray2 )
{
	using test_fifo_type3 = alpha::concurrent::fifo_list<array_test[2]>;
	test_fifo_type3* p_test_obj;

	array_test tmp_data[2];
	tmp_data[0].x = 2;
	tmp_data[1].x = 3;

	std::cout << "Array array_test[2] test#2" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push( tmp_data );

	array_test vt[2];
	auto       ret = p_test_obj->pop( vt );

	ASSERT_TRUE( ret );

	EXPECT_EQ( 2, vt[0].x );
	EXPECT_EQ( 3, vt[1].x );

	delete p_test_obj;

	std::cout << "End Array array_test[2] test" << std::endl;
}
