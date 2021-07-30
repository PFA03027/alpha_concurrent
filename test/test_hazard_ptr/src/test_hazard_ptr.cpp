//============================================================================
// Name        : test_hazard_ptr.cpp
// Author      :
// Version     :
// Copyright   : Copyright (C) 2020 by Teruaki Ata <PFA03027@nifty.com>
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <pthread.h>

#include "gtest/gtest.h"

#include "alconcurrent/hazard_ptr.hpp"

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

constexpr int       num_thread = 128;
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

	alpha::concurrent::hazard_ptr_scoped_ref<
		alpha::concurrent::hazard_ptr<delete_test, 1>::hzrd_type,
		alpha::concurrent::hazard_ptr<delete_test, 1>::hzrd_max_slot>
		hzrd_ref( hazard_ptr_to, 0 );
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
		alpha::concurrent::hazard_ptr_scoped_ref<
			alpha::concurrent::hazard_ptr<delete_test, 1>::hzrd_type,
			alpha::concurrent::hazard_ptr<delete_test, 1>::hzrd_max_slot>
			hzrd_ref( hazard_ptr_to, 0 );

		printf( "p_target: %p\n", p_test_obj );

		hazard_ptr_to.regist_ptr_as_hazard_ptr( p_test_obj, 0 );

		std::cout << "!!!Ready!!!" << std::endl;   // prints !!!Hello World!!!
		pthread_barrier_wait( &barrier );
		printf( "func_delete_owner GO now!!!\n" );

		//		std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );

		//		hazard_ptr_to.move_hazard_ptr_to_del_list();
	}

	while ( hazard_ptr_to.check_ptr_in_hazard_list( p_test_obj ) ) {
		std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );
	}

	p_target->store( nullptr );
	delete p_test_obj;

	uintptr_t ans = 0;

	ans = 1;

	printf( "Exit func_delete_owner!!!\n" );
	return reinterpret_cast<void*>( ans );
}

void test_case1( void )
{
	std::atomic<delete_test*> atm_p_test_obj( new delete_test );

	pthread_barrier_init( &barrier, NULL, num_thread + 2 );
	pthread_t* threads = new pthread_t[num_thread + 1];

	pthread_create( &threads[0], NULL, func_delete_owner, reinterpret_cast<void*>( &atm_p_test_obj ) );
	for ( int i = 1; i <= num_thread; i++ ) {
		//		std::cout << "Thread " << i << " is created." << std::endl;
		pthread_create( &threads[i], NULL, func_refarencing, reinterpret_cast<void*>( &atm_p_test_obj ) );
	}
	std::cout << "!!!Ready!!!" << std::endl;
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );
	std::cout << "!!!GO!!!" << std::endl;

	int sum = 0;
	for ( int i = 0; i <= num_thread; i++ ) {
		uintptr_t e;
		pthread_join( threads[i], reinterpret_cast<void**>( &e ) );
		//		std::cout << "Thread " << i << ": last dequeued = " << e << std::endl;
		sum += e;
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << num_thread << "  Exec time: " << diff.count() << " msec" << std::endl;

	// 各スレッドが最後にdequeueした値の合計は num_thread * num_loop
	// に等しくなるはず。
	std::cout << "Expect: 1" << std::endl;
	std::cout << "Sum: " << sum << std::endl;
	ASSERT_EQ( 1, sum );

	delete[] threads;

	//	auto [hzrd_size, del_size] = alpha::concurrent::hazard_ptr<delete_test>::debug_get_glist_size();
	//	printf( "glist_size: hazard ptr=%d, del ptr=%d\n", hzrd_size, del_size );

	return;
}

TEST( HazardPtr, TC1 )
{
	std::cout << "!!!Start World!!!" << std::endl;   // prints !!!Hello World!!!

	for ( int i = 0; i < num_thread; i++ ) {
		//	for ( int i = 0; i < 1; i++ ) {
		std::cout << "\t!!!Start " << i << std::endl;   // prints !!!Hello World!!!
		ASSERT_NO_FATAL_FAILURE( test_case1() );
	}

	printf( "number of keys of pthread_key_create(),     %d\n", alpha::concurrent::internal::get_num_of_tls_key() );
	printf( "max number of keys of pthread_key_create(), %d\n", alpha::concurrent::internal::get_max_num_of_tls_key() );
	std::cout << "!!!End World!!!" << std::endl;   // prints !!!Hello World!!!
	return;
}
