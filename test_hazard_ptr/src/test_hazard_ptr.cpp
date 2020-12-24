//============================================================================
// Name        : test_hazard_ptr.cpp
// Author      : alpha
// Version     :
// Copyright   : PFA03027@nifty.com
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <pthread.h>

#include "hazard_ptr.hpp"

class delete_test {
public:
	delete_test( void )
	{
		std::cout << "!!!Hello World!!!" << std::endl;   // prints !!!Hello World!!!
	}

	~delete_test()
	{
		std::cout << "!!!Goodbye World!!!" << std::endl;   // prints !!!Hello World!!!
	}

private:
	char dummy[1024 * 1024];
};

constexpr int       num_thread = 256;
constexpr uintptr_t loop_num   = 100000;

pthread_barrier_t barrier;

/**
 * 各スレッドのメインルーチン。
 * stmへのカウントアップを繰り返す。
 */
void* func_refarencing( void* data )
{
	alpha::concurrent::hazard_ptr<delete_test> hazard_ptr_to;
	delete_test*                               p_target = reinterpret_cast<delete_test*>( data );

	alpha::concurrent::hazard_ptr_scoped_ref<delete_test> hzrd_ref( hazard_ptr_to );
	//	printf( "p_target: %p\n", p_target );

	hazard_ptr_to.regist_ptr_as_hazard_ptr( p_target );

	pthread_barrier_wait( &barrier );

	uintptr_t ans = 0;

	std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );

	//	hazard_ptr_to.clear_hazard_ptr();
	ans = 0;

	return reinterpret_cast<void*>( ans );
}

/**
 * 各スレッドのメインルーチン。
 * stmへのカウントアップを繰り返す。
 */
void* func_delete_owner( void* data )
{
	alpha::concurrent::hazard_ptr<delete_test> hazard_ptr_to;
	delete_test*                               p_target = reinterpret_cast<delete_test*>( data );

	{
		alpha::concurrent::hazard_ptr_scoped_ref<delete_test> hzrd_ref( hazard_ptr_to );

		printf( "p_target: %p\n", p_target );

		hazard_ptr_to.regist_ptr_as_hazard_ptr( p_target );

		pthread_barrier_wait( &barrier );

		std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );

		hazard_ptr_to.try_delete_instance();
	}

	uintptr_t ans = 0;

	ans = 1;

	return reinterpret_cast<void*>( ans );
}

int test_case1( void )
{
	delete_test* p_test_obj = new delete_test;

	pthread_barrier_init( &barrier, NULL, num_thread + 2 );
	pthread_t* threads = new pthread_t[num_thread + 1];

	pthread_create( &threads[0], NULL, func_delete_owner, reinterpret_cast<void*>( p_test_obj ) );
	for ( int i = 1; i <= num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_refarencing, reinterpret_cast<void*>( p_test_obj ) );
	}

	std::cout << "!!!Ready!!!" << std::endl;   // prints !!!Hello World!!!
	pthread_barrier_wait( &barrier );
	std::cout << "!!!GO!!!" << std::endl;   // prints !!!Hello World!!!

	int sum = 0;
	for ( int i = 0; i <= num_thread; i++ ) {
		uintptr_t e;
		pthread_join( threads[i], reinterpret_cast<void**>( &e ) );
		//		std::cout << "Thread " << i << ": last dequeued = " << e << std::endl;
		sum += e;
	}

	// 各スレッドが最後にdequeueした値の合計は num_thread * num_loop
	// に等しくなるはず。
	std::cout << "Expect: 1" << std::endl;
	std::cout << "Sum: " << sum << std::endl;
	if ( sum == 1 ) {
		std::cout << "OK!" << std::endl;
	} else {
		std::cout << "NGGGGGGgggggg!" << std::endl;
	}

	delete[] threads;

	printf( "glist_size: %d\n",
	        alpha::concurrent::hazard_ptr<delete_test>::debug_get_glist_size() );

	return 0;
}

int main( void )
{
	for ( int i = 0; i < num_thread; i++ ) {
		test_case1();
	}
	return 0;
}
