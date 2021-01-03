//============================================================================
// Name        : test_stm.cpp
// Author      :
// Version     :
// Copyright   : Copyright (C) 2020 by Teruaki Ata <PFA03027@nifty.com>
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <pthread.h>
#include <stdint.h>
#include <thread>

#include "stm.hpp"

using namespace std;

constexpr uintptr_t g_loop_num = 10000 * 256;
//constexpr uintptr_t g_loop_num = 10 * 256;

pthread_barrier_t barrier;

/**
 * 各スレッドのメインルーチン。
 * stmへのカウントアップを繰り返す。
 */
void func( alpha::concurrent::stm<uintptr_t>* p_target, int loop_num, int* p_ans )
{

	pthread_barrier_wait( &barrier );

	uintptr_t loop_count = loop_num;

	while ( loop_count-- != 0 ) {
		p_target->read_modify_write( []( uintptr_t a ) -> uintptr_t { return a + 1; } );
	}

	*p_ans = 1;
	return;
}

void test_case1( int num_thread, int loop_num )
{
	cout << "!!!Ready!!!" << endl;   // prints !!!Hello World!!!

	alpha::concurrent::stm<uintptr_t> stm_counter( 0 );
	alpha::concurrent::stm<intptr_t>  tmp_stm_counter( 0 );

	//	cout << (stm_counter.is_lock_free() ? "lock free" : "NOT lock free") << endl;

	pthread_barrier_init( &barrier, NULL, num_thread + 1 );
	std::thread* threads     = new std::thread[num_thread];
	int*         threads_ans = new int[num_thread];

	for ( int i = 0; i < num_thread; i++ ) {
		threads[i] = std::move(
			std::thread( func, &stm_counter, loop_num, &( threads_ans[i] ) ) );
	}

	std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
	cout << "!!!GO!!!" << endl;   // prints !!!Hello World!!!

	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();

	pthread_barrier_wait( &barrier );

	int sum = 0;
	for ( int i = 0; i < num_thread; i++ ) {
		threads[i].join();
		//		std::cout << "Thread " << i << ": last dequeued = " << threads_ans[i] << std::endl;
		sum += threads_ans[i];
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << num_thread << "  Exec time: " << diff.count() << std::endl;

	// 各スレッドが最後にdequeueした値の合計は num_thread * num_loop
	// に等しくなるはず。
	sum = *( stm_counter.read_value() );
	std::cout << "Expect: " << num_thread * loop_num << std::endl;
	std::cout << "Sum: " << sum << std::endl;
	if ( sum == num_thread * loop_num ) {
		std::cout << "OK!" << std::endl;
	} else {
		std::cout << "NGGGGGGgggggg!" << std::endl;
	}

	auto [hzrd_size, del_size] = alpha::concurrent::stm<uintptr_t>::debug_get_glist_size();
	printf( "glist_size: hazard ptr=%d, del ptr=%d\n", hzrd_size, del_size );

	delete[] threads;
	delete[] threads_ans;

	std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );

	return;
}

int main( int argc, char* argv[] )
{
	cout << "!!!Hello World!!!" << endl;   // prints !!!Hello World!!!

	for ( int i = 1; i <= 64; i = i * 2 ) {
		test_case1( i, g_loop_num / i );
	}

	cout << "!!!Good-by World!!!" << endl;   // prints !!!Hello World!!!
	return 0;
}
