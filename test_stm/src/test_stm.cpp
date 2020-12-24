//============================================================================
// Name        : test_stm.cpp
// Author      :
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <pthread.h>
#include <stdint.h>

#include <iostream>

#include "stm.hpp"

using namespace std;

constexpr int       num_thread = 256;
constexpr uintptr_t loop_num   = 100000;

pthread_barrier_t barrier;

/**
 * 各スレッドのメインルーチン。
 * stmへのカウントアップを繰り返す。
 */
void* func_refarencing( void* data )
{
	alpha::concurrent::stm<uintptr_t>* p_target = reinterpret_cast<alpha::concurrent::stm<uintptr_t>*>( data );

	pthread_barrier_wait( &barrier );

	uintptr_t loop_count = loop_num;

	while ( loop_count-- != 0 ) {
		p_target->read_modify_write( []( uintptr_t a ) -> uintptr_t { return a + 1; } );
	}

	return reinterpret_cast<void*>( 1 );
}

int main( int argc, char* argv[] )
{
	cout << "!!!Hello World!!!" << endl;   // prints !!!Hello World!!!

	alpha::concurrent::stm<uintptr_t> stm_counter( 0 );

	//	cout << (stm_counter.is_lock_free() ? "lock free" : "NOT lock free") << endl;

	pthread_barrier_init( &barrier, NULL, num_thread + 1 );
	pthread_t* threads = new pthread_t[num_thread];

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_refarencing, reinterpret_cast<void*>( &stm_counter ) );
	}

	cout << "!!!GO!!!" << endl;   // prints !!!Hello World!!!
	pthread_barrier_wait( &barrier );

	uintptr_t sum = 0;
	for ( int i = 0; i < num_thread; i++ ) {
		uintptr_t e;
		pthread_join( threads[i], reinterpret_cast<void**>( &e ) );
		std::cout << "Thread " << i << ": last dequeued = " << e << std::endl;
		sum += e;
	}

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

	delete[] threads;

	cout << "!!!Good-by World!!!" << endl;   // prints !!!Hello World!!!
	return 0;
}
