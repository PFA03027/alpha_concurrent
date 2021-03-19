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

#include "lf_one_side_deque.hpp"

constexpr int            num_thread = 64;   // Tested until 128.
constexpr std::uintptr_t loop_num   = 100000;

using test_list = alpha::concurrent::one_side_deque<std::uintptr_t>;

pthread_barrier_t barrier;

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
		auto [pop_flag, vv] = p_test_obj->pop_front();
		if ( !pop_flag ) {
			printf( "Bugggggggyyyy  func_test_one_side_deque_front2back()!!!  %llu\n", v );
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
		auto [pop_flag, vv] = p_test_obj->pop_front();
		if ( !pop_flag ) {
			printf( "Bugggggggyyyy  func_test_one_side_deque_back2front()!!!  %llu\n", v );
			printf( "list size count: %d\n", p_test_obj->get_size() );
			exit( 1 );
		}
		v = vv + 1;
	}

	return reinterpret_cast<void*>( v );
}

int test1( void )
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

	int sum = 0;
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
	if ( sum == ( num_thread * 2 * loop_num ) ) {
		std::cout << "OK!" << std::endl;
	} else {
		std::cout << "NGGGGGGgggggg!" << std::endl;
		return EXIT_FAILURE;
	}

	delete[] threads;

	std::cout << "Allocated nodes:    " << count_list.get_allocated_num() << std::endl;

	return EXIT_SUCCESS;
}

int main( void )
{
	std::cout << "!!!Start Test World!!!" << std::endl;   // prints !!!Hello World!!!

	test1();

	std::cout << "!!!End Test World!!!" << std::endl;   // prints !!!Hello World!!!
	return EXIT_SUCCESS;
}
