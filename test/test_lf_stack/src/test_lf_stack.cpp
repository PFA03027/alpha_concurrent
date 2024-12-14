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
#include <future>
#include <iostream>
#include <mutex>
#include <vector>

#include "gtest/gtest.h"

#include "alconcurrent/lf_mem_alloc.hpp"
#include "alconcurrent/lf_mem_alloc_type.hpp"
#include "alconcurrent/lf_stack.hpp"

constexpr int            num_thread = 10;   // Tested until 128.
constexpr std::uintptr_t loop_num   = 10000;

pthread_barrier_t global_shared_barrier;

class lfStackTest_Highload : public ::testing::Test {
protected:
	virtual void SetUp()
	{
		alpha::concurrent::gmem_prune();
	}

	virtual void TearDown()
	{
		printf( "number of keys of dynamic_tls_key_create(),     %d\n", alpha::concurrent::internal::get_num_of_tls_key() );
		printf( "max number of keys of dynamic_tls_key_create(), %d\n", alpha::concurrent::internal::get_max_num_of_tls_key() );
	}
};

using test_lifo_type = alpha::concurrent::stack_list<std::uintptr_t>;

/**
 * 各スレッドのメインルーチン。
 * カウントアップを繰り返す。
 */
void* func_test_fifo( void* data )
{
	test_lifo_type* p_test_obj = reinterpret_cast<test_lifo_type*>( data );

	pthread_barrier_wait( &global_shared_barrier );

	typename test_lifo_type::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		p_test_obj->push( v );
		auto ret = p_test_obj->pop();
		if ( !ret.has_value() ) {
			printf( "Bugggggggyyyy  func_test_fifo()!!!  %s\n", std::to_string( v ).c_str() );
#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
			printf( "fifo size count: %d\n", p_test_obj->get_size() );
#endif
			exit( 1 );
		}
		v = ret.value() + 1;
	}

	return reinterpret_cast<void*>( v );
}

std::tuple<uintptr_t, uintptr_t> func_test_fifo2( test_lifo_type* p_test_obj[] )
{

	typename test_lifo_type::value_type v1 = 0;
	typename test_lifo_type::value_type v2 = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		p_test_obj[0]->push( v1 );
		p_test_obj[1]->push( v2 );
		{
			auto ret = p_test_obj[0]->pop();
			if ( !ret.has_value() ) {
				printf( "Bugggggggyyyy!!!  func_test_fifo2()  %s\n", std::to_string( v1 ).c_str() );
#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
				printf( "fifo size count idx 0: %d\n", p_test_obj[0]->get_size() );
#endif
				exit( 1 );
			}
			v1 = ret.value() + 1;
		}
		{
			auto ret = p_test_obj[1]->pop();
			if ( !ret.has_value() ) {
				printf( "Bugggggggyyyy!!!  func_test_fifo2()  %s\n", std::to_string( v2 ).c_str() );
#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
				printf( "fifo size count idx 1: %d\n", p_test_obj[1]->get_size() );
#endif
				exit( 1 );
			}
			v2 = ret.value() + 1;
		}
	}

	return std::tuple<uintptr_t, uintptr_t>( v1, v2 );
}

TEST_F( lfStackTest_Highload, TC1 )
{
	test_lifo_type  sut[2];
	test_lifo_type* p_test_obj[2];
	p_test_obj[0] = &( sut[0] );
	p_test_obj[1] = &( sut[1] );

	pthread_barrier_init( &global_shared_barrier, NULL, num_thread + 1 );
	pthread_t* threads = new pthread_t[num_thread];

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_test_fifo, reinterpret_cast<void*>( p_test_obj[i % 2] ) );
	}
	std::cout << "!!!Ready!!!" << std::endl;

	std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
	std::cout << "!!!GO!!!" << std::endl;
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &global_shared_barrier );

#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
	auto [a1, a2] = func_test_fifo2( p_test_obj );
#else
	auto local_ret = func_test_fifo2( p_test_obj );
	auto a1        = std::get<0>( local_ret );
	auto a2        = std::get<1>( local_ret );
#endif
	std::cout << "Thread X: last dequeued = " << a1 << ", " << a2 << std::endl;

	uintptr_t sum = a1 + a2;
	for ( int i = 0; i < num_thread; i++ ) {
		uintptr_t e;
		pthread_join( threads[i], reinterpret_cast<void**>( &e ) );
		std::cout << "Thread " << i << ": last dequeued = " << e << std::endl;
		sum += e;
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << num_thread << "  Exec time: " << diff.count() << " msec" << std::endl;

	// 各スレッドが最後にdequeueした値の合計は num_thread * num_loop
	// に等しくなるはず。
	std::cout << "Expect: " << std::to_string( ( num_thread + 2 ) * loop_num ) << std::endl;
	std::cout << "Sum:    " << sum << std::endl;

	EXPECT_EQ( ( num_thread + 2 ) * loop_num, sum );

	delete[] threads;

#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
	std::cout << "Allocated nodes #0:    " << p_test_obj[0]->get_allocated_num() << std::endl;
	std::cout << "Allocated nodes #1:    " << p_test_obj[1]->get_allocated_num() << std::endl;
#endif

	return;
}

using test_lifo_type2 = alpha::concurrent::stack_list<std::uintptr_t>;

/**
 * 各スレッドのメインルーチン。
 * カウントアップを繰り返す。
 */
void* func_test4_fifo( void* data )
{
	test_lifo_type2* p_test_obj = reinterpret_cast<test_lifo_type2*>( data );

	pthread_barrier_wait( &global_shared_barrier );

	typename test_lifo_type2::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		p_test_obj->push( v );
		auto ret = p_test_obj->pop();
		if ( !ret.has_value() ) {
			printf( "Bugggggggyyyy  func_test_fifo()!!!  %s\n", std::to_string( v ).c_str() );
#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
			printf( "fifo size count: %d\n", p_test_obj->get_size() );
#endif
			exit( 1 );
		}
		v = ret.value() + 1;
	}

	return reinterpret_cast<void*>( v );
}

std::tuple<uintptr_t, uintptr_t> func_test4_fifo2( test_lifo_type2* p_test_obj[] )
{

	typename test_lifo_type2::value_type v1 = 0;
	typename test_lifo_type2::value_type v2 = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		p_test_obj[0]->push( v1 );
		p_test_obj[1]->push( v2 );

		{
			auto ret = p_test_obj[0]->pop();
			if ( !ret.has_value() ) {
				printf( "Bugggggggyyyy!!!  func_test_fifo2()  %s\n", std::to_string( v1 ).c_str() );
#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
				printf( "fifo size count idx 0: %d\n", p_test_obj[0]->get_size() );
#endif
				exit( 1 );
			}
			v1 = ret.value() + 1;
		}
		{
			auto ret = p_test_obj[1]->pop();
			if ( !ret.has_value() ) {
				printf( "Bugggggggyyyy!!!  func_test_fifo2()  %s\n", std::to_string( v2 ).c_str() );
#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
				printf( "fifo size count idx 1: %d\n", p_test_obj[1]->get_size() );
#endif
				exit( 1 );
			}
			v2 = ret.value() + 1;
		}
	}

#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
	printf( "final count of p_test_obj[0] is %d", p_test_obj[0]->get_size() );
#endif

	return std::tuple<uintptr_t, uintptr_t>( v1, v2 );
}

TEST_F( lfStackTest_Highload, TC2 )
{
	//	test_lifo_type2* p_test_obj = new test_lifo_type2( num_thread );
	test_lifo_type2* p_test_obj[2];
	p_test_obj[0] = new test_lifo_type2( 8 );
	p_test_obj[1] = new test_lifo_type2( 8 );

	pthread_barrier_init( &global_shared_barrier, NULL, num_thread + 1 );
	pthread_t* threads = new pthread_t[num_thread];

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_test4_fifo, reinterpret_cast<void*>( p_test_obj[i % 2] ) );
	}
	std::cout << "!!!Ready!!!" << std::endl;

	std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
	std::cout << "!!!GO!!!" << std::endl;
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &global_shared_barrier );

#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
	auto [a1, a2] = func_test4_fifo2( p_test_obj );
#else
	auto local_ret = func_test4_fifo2( p_test_obj );
	auto a1        = std::get<0>( local_ret );
	auto a2        = std::get<1>( local_ret );
#endif
	std::cout << "Thread X: last dequeued = " << a1 << ", " << a2 << std::endl;

	uintptr_t sum = a1 + a2;
	for ( int i = 0; i < num_thread; i++ ) {
		uintptr_t e;
		pthread_join( threads[i], reinterpret_cast<void**>( &e ) );
		std::cout << "Thread " << i << ": last dequeued = " << e << std::endl;
		sum += e;
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << num_thread << "  Exec time: " << diff.count() << " msec" << std::endl;

	// 各スレッドが最後にdequeueした値の合計は num_thread * num_loop
	// に等しくなるはず。
	std::cout << "Expect: " << std::to_string( ( num_thread + 2 ) * loop_num ) << std::endl;
	std::cout << "Sum:    " << sum << std::endl;

	EXPECT_EQ( ( num_thread + 2 ) * loop_num, sum );

	delete[] threads;

#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
	std::cout << "Allocated nodes #0:    " << p_test_obj[0]->get_allocated_num() << std::endl;
	std::cout << "Allocated nodes #1:    " << p_test_obj[1]->get_allocated_num() << std::endl;
#endif

	delete p_test_obj[0];
	delete p_test_obj[1];

	return;
}

///////////////////////////////////////////////////////////////////////////////

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

TEST_F( lfStackTest_Highload, TC3 )
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
