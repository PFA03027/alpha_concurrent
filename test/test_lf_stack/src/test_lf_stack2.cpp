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

#include "alconcurrent/lf_mem_alloc_type.hpp"
#include "alconcurrent/lf_stack.hpp"

TEST( LFSTACK_2, CallDefaultConstruct )
{
	// Arrange

	// Act
	alpha::concurrent::stack_list<int> sut;

	// Assert
}

TEST( LFSTACK_2, CallPopFromEmpty )
{
	// Arrange
	alpha::concurrent::stack_list<int> sut;

	// Act
	auto ret = sut.pop();

	// Assert
	EXPECT_FALSE( ret.has_value() );
}

TEST( LFSTACK_2, CallPushPopOne )
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

TEST( LFSTACK_2, CallPushPopTwo )
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

///////////////////////////////////////////////////////////////////////////////

constexpr unsigned int   num_thread = 10;   // Tested until 128.
constexpr std::uintptr_t loop_num   = 10000;

pthread_barrier_t barrier2;

/**
 * 各スレッドのメインルーチン。
 * カウントアップを繰り返す。
 */
long func_test_stack_list( alpha::concurrent::stack_list<long>* p_test_obj )
{

	pthread_barrier_wait( &barrier2 );

	typename alpha::concurrent::stack_list<long>::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		p_test_obj->push( v );
		auto ret = p_test_obj->pop();
		if ( !ret.has_value() ) {
			printf( "Bugggggggyyyy  func_test_fifo()!!!  %s\n", std::to_string( v ).c_str() );
			exit( 1 );
		}
		v = ret.value() + 1;
	}

	return v;
}

TEST( LFSTACK_2_HighLoad, TC3 )
{
	// Arrange
	pthread_barrier_init( &barrier2, NULL, num_thread + 1 );

	alpha::concurrent::stack_list<long> sut;
	std::vector<std::future<long>>      results( num_thread );
	std::vector<std::thread>            threads( num_thread );

	// Act
	for ( unsigned int i = 0; i < num_thread; i++ ) {
		std::packaged_task<long( alpha::concurrent::stack_list<long>* )> task( func_test_stack_list );
		results[i] = task.get_future();
		threads[i] = std::thread( std::move( task ), &sut );
	}
	pthread_barrier_wait( &barrier2 );

	// Assert
	long sum = 0;
	for ( unsigned int i = 0; i < num_thread; i++ ) {
		if ( threads[i].joinable() ) {
			threads[i].join();
		}
		sum += results[i].get();
	}

	// 各スレッドが最後にdequeueした値の合計は num_thread * num_loop
	// に等しくなるはず。
	EXPECT_EQ( num_thread * loop_num, sum );
}
