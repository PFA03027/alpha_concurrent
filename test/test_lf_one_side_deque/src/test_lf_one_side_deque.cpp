//============================================================================
// Name        : test_lf_list.cpp
// Author      : Teruaki Ata
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C, Ansi-style
//============================================================================

#include <pthread.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <thread>

#include "gtest/gtest.h"

#include "alconcurrent/lf_mem_alloc_type.hpp"
#include "alconcurrent/lf_one_side_deque.hpp"

constexpr int            num_thread = 5;   // Tested until 128.
constexpr std::uintptr_t loop_num   = 10000;

using test_list = alpha::concurrent::one_side_deque<std::uintptr_t>;

pthread_barrier_t barrier;

class lfOneSideDeqTest : public ::testing::Test {
protected:
	virtual void SetUp()
	{
		alpha::concurrent::gmem_prune();
	}

	virtual void TearDown()
	{
		auto statistics = alpha::concurrent::internal::node_of_list::get_statistics();
		printf( "%s\n", statistics.print().c_str() );

		printf( "number of keys of dynamic_tls_key_create(),     %d\n", alpha::concurrent::internal::get_num_of_tls_key() );
		printf( "max number of keys of dynamic_tls_key_create(), %d\n", alpha::concurrent::internal::get_max_num_of_tls_key() );
	}
};

/**
 * 各スレッドの先頭から追加して、最後から取り出すことで、カウントアップを繰り返す。
 */
void* func_test_one_side_deque_front2front( void* data )
{
	test_list* p_test_obj = reinterpret_cast<test_list*>( data );

	pthread_barrier_wait( &barrier );

	typename test_list::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		p_test_obj->push_front( v );
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
		auto [pop_flag, vv] = p_test_obj->pop_front();
#else
		auto local_ret = p_test_obj->pop_front();
		auto pop_flag  = std::get<0>( local_ret );
		auto vv        = std::get<1>( local_ret );
#endif
		if ( !pop_flag ) {
			printf( "Bugggggggyyyy  func_test_one_side_deque_front2back()!!!  %s\n", std::to_string( v ).c_str() );
			printf( "list size count: %d\n", p_test_obj->get_size() );
			exit( 1 );
		}
		v = vv + 1;
	}

	return reinterpret_cast<void*>( v );
}

/**
 * 各スレッドの最後から追加して、先頭から取り出すことで、カウントアップを繰り返す。
 */
void* func_test_one_side_deque_back2front( void* data )
{
	test_list* p_test_obj = reinterpret_cast<test_list*>( data );

	pthread_barrier_wait( &barrier );

	typename test_list::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		p_test_obj->push_back( v );
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
		auto [pop_flag, vv] = p_test_obj->pop_front();
#else
		auto local_ret = p_test_obj->pop_front();
		auto pop_flag  = std::get<0>( local_ret );
		auto vv        = std::get<1>( local_ret );
#endif
		if ( !pop_flag ) {
			printf( "Bugggggggyyyy  func_test_one_side_deque_back2front()!!!  %s\n", std::to_string( v ).c_str() );
			printf( "list size count: %d\n", p_test_obj->get_size() );
			exit( 1 );
		}
		v = vv + 1;
	}

	printf( "final count of p_test_obj is %d", p_test_obj->get_size() );

	return reinterpret_cast<void*>( v );
}

TEST_F( lfOneSideDeqTest, TC1 )
{
	test_list count_list;

	pthread_barrier_init( &barrier, NULL, num_thread * 2 + 1 );
	pthread_t* threads = new pthread_t[num_thread * 2];

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_test_one_side_deque_front2front, reinterpret_cast<void*>( &count_list ) );
	}

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[num_thread + i], NULL, func_test_one_side_deque_back2front, reinterpret_cast<void*>( &count_list ) );
	}

	std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

	uintptr_t sum = 0;
	for ( int i = 0; i < num_thread * 2; i++ ) {
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
	std::cout << "Expect: " << std::to_string( num_thread * 2 * loop_num ) << std::endl;
	std::cout << "Sum:    " << sum << std::endl;

	EXPECT_EQ( num_thread * 2 * loop_num, sum );

	delete[] threads;

	std::cout << "Allocated nodes:    " << count_list.get_allocated_num() << std::endl;

	return;
}

TEST_F( lfOneSideDeqTest, Pointer )
{
	using test_fifo_type3 = alpha::concurrent::one_side_deque<int*>;
	test_fifo_type3* p_test_obj;

	std::cout << "Pointer test#1" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push_back( new int() );

	delete p_test_obj;

	std::cout << "Pointer test#2" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push_front( new int() );

	delete p_test_obj;

	std::cout << "Pointer test#3" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push_back( new int() );
	auto ret = p_test_obj->pop_front();

	ASSERT_TRUE( std::get<0>( ret ) );

	delete std::get<1>( ret );
	delete p_test_obj;

	std::cout << "Pointer test#4" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push_front( new int() );
	ret = p_test_obj->pop_front();

	ASSERT_TRUE( std::get<0>( ret ) );

	delete std::get<1>( ret );
	delete p_test_obj;

	std::cout << "End Pointer test" << std::endl;
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

TEST_F( lfOneSideDeqTest, Array )
{
	using test_fifo_type3 = alpha::concurrent::one_side_deque<array_test[]>;
	test_fifo_type3* p_test_obj;

	std::cout << "Array array_test[] test#1" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push_back( new array_test[2] );

	delete p_test_obj;

	std::cout << "Array array_test[] test#2" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push_front( new array_test[2] );

	delete p_test_obj;

	std::cout << "Array array_test[] test#3" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push_back( new array_test[2] );
	auto ret = p_test_obj->pop_front();

	ASSERT_TRUE( std::get<0>( ret ) );

	delete[] std::get<1>( ret );
	delete p_test_obj;

	std::cout << "Array array_test[] test#4" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push_front( new array_test[2] );
	ret = p_test_obj->pop_front();

	ASSERT_TRUE( std::get<0>( ret ) );

	delete[] std::get<1>( ret );
	delete p_test_obj;

	std::cout << "End Array array_test[] test" << std::endl;
}
