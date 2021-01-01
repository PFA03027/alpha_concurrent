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

	void test_write( void )
	{
		dummy[0] = 1;
		return;
	}

private:
	char dummy[1024 * 1024];
};

constexpr int       num_thread = 256;
constexpr uintptr_t loop_num   = 100000;

alpha::concurrent::hazard_ptr<delete_test, 1> hazard_ptr_to;

pthread_barrier_t barrier;

/**
 * 各スレッドのメインルーチン。
 * stmへのカウントアップを繰り返す。
 */
void* func_refarencing( void* data )
{
	std::atomic<delete_test*>* p_target = reinterpret_cast<std::atomic<delete_test*>*>( data );

	alpha::concurrent::hazard_ptr_scoped_ref hzrd_ref( hazard_ptr_to, 0 );
	//	printf( "p_target: %p\n", p_target );

	delete_test* p_test_obj = p_target->load();
	hazard_ptr_to.regist_ptr_as_hazard_ptr( p_test_obj, 0 );

	//	std::cout << "!!!Ready!!!" << std::endl;   // prints !!!Hello World!!!
	pthread_barrier_wait( &barrier );
	//	printf( "func_refarencing GO now!!!\n" );

	if ( p_test_obj == p_target->load() ) {
		p_test_obj->test_write();
	} else {
		printf( "Gyaaaa!!!\n" );
	}

	uintptr_t ans = 0;

	//	std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );

	//	hazard_ptr_to.clear_hazard_ptr();
	ans = 0;

	//	printf( "Exit func_refarencing!!!\n" );
	return reinterpret_cast<void*>( ans );
}

/**
 * 各スレッドのメインルーチン。
 * stmへのカウントアップを繰り返す。
 */
void* func_delete_owner( void* data )
{
	std::atomic<delete_test*>* p_target = reinterpret_cast<std::atomic<delete_test*>*>( data );

	delete_test* p_test_obj = p_target->load();

	{
		alpha::concurrent::hazard_ptr_scoped_ref hzrd_ref( hazard_ptr_to, 0 );

		printf( "p_target: %p\n", p_test_obj );

		hazard_ptr_to.regist_ptr_as_hazard_ptr( p_test_obj, 0 );

		std::cout << "!!!Ready!!!" << std::endl;   // prints !!!Hello World!!!
		pthread_barrier_wait( &barrier );
		printf( "func_delete_owner GO now!!!\n" );

		//		std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );

		//		hazard_ptr_to.move_hazard_ptr_to_del_list();
	}

	while ( hazard_ptr_to.chk_ptr_in_hazard_list( p_test_obj ) ) {
		std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );
	}

	p_target->store( nullptr );
	delete p_test_obj;

	uintptr_t ans = 0;

	ans = 1;

	printf( "Exit func_delete_owner!!!\n" );
	return reinterpret_cast<void*>( ans );
}

int test_case1( void )
{
	std::atomic<delete_test*> atm_p_test_obj( new delete_test );

	pthread_barrier_init( &barrier, NULL, num_thread + 2 );
	pthread_t* threads = new pthread_t[num_thread + 1];

	pthread_create( &threads[0], NULL, func_delete_owner, reinterpret_cast<void*>( &atm_p_test_obj ) );
	for ( int i = 1; i <= num_thread; i++ ) {
//		std::cout << "Thread " << i << " is created." << std::endl;
		pthread_create( &threads[i], NULL, func_refarencing, reinterpret_cast<void*>( &atm_p_test_obj ) );
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

	//	std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );

	// 各スレッドが最後にdequeueした値の合計は num_thread * num_loop
	// に等しくなるはず。
	std::cout << "Expect: 1" << std::endl;
	std::cout << "Sum: " << sum << std::endl;
	if ( sum == 1 ) {
		std::cout << "OK!" << std::endl;
	} else {
		std::cout << "NGGGGGGgggggg!" << std::endl;
		exit( 1 );
	}

	delete[] threads;

	//	auto [hzrd_size, del_size] = alpha::concurrent::hazard_ptr<delete_test>::debug_get_glist_size();
	//	printf( "glist_size: hazard ptr=%d, del ptr=%d\n", hzrd_size, del_size );

	return 0;
}

int main( void )
{
	std::cout << "!!!Start World!!!" << std::endl;   // prints !!!Hello World!!!

		for ( int i = 0; i < num_thread; i++ ) {
//	for ( int i = 0; i < 1; i++ ) {
		test_case1();
	}

	std::cout << "!!!End World!!!" << std::endl;   // prints !!!Hello World!!!
	return 0;
}
