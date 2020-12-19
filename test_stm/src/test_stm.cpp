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

alpha::concurrent::stm<uintptr_t> stm_counter( 0 );

pthread_barrier_t barrier;

/**
 * 各スレッドのメインルーチン。
 * stmへのカウントアップを繰り返す。
 */
void* func( void* data )
{
	pthread_barrier_wait( &barrier );

	uintptr_t loop_count = reinterpret_cast<uintptr_t>( data );

	while ( loop_count-- != 0 ) {
		stm_counter.read_modify_write( []( uintptr_t a ) { return a + 1; } );
	}

	return reinterpret_cast<void*>( 1 );
}

int main( int argc, char* argv[] )
{
	cout << "!!!Hello World!!!" << endl;   // prints !!!Hello World!!!

	int       num_thread = 256;
	uintptr_t num_loop   = 100000;

	pthread_barrier_init( &barrier, NULL, num_thread + 1 );
	pthread_t* threads = new pthread_t[num_thread];

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func, reinterpret_cast<void*>( num_loop ) );
	}

	pthread_barrier_wait( &barrier );
	cout << "!!!GO!!!" << endl;   // prints !!!Hello World!!!

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
	std::cout << "Expect: " << num_thread * num_loop << std::endl;
	std::cout << "Sum: " << sum << std::endl;
	if ( sum == num_thread * num_loop ) {
		std::cout << "OK!" << std::endl;
	} else {
		std::cout << "NGGGGGGgggggg!" << std::endl;
	}

	delete[] threads;

	cout << "!!!Good-by World!!!" << endl;   // prints !!!Hello World!!!
	return 0;
}
