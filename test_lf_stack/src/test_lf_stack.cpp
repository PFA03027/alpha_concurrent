//============================================================================
// Name        : test_lf_fifo.cpp
// Author      :
// Version     :
// Copyright   : Copyright (C) 2020 by Teruaki Ata <PFA03027@nifty.com>
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <pthread.h>

#include <cstdint>
#include <iostream>

#include "lf_stack.hpp"

constexpr int            num_thread = 128;   // Tested until 128.
constexpr std::uintptr_t loop_num   = 100000;

using test_lifo_type_part = alpha::concurrent::internal::lifo_nd_list<std::uintptr_t>;

pthread_barrier_t barrier;

/**
 * 各スレッドのメインルーチン。
 */
void* func_push( void* data )
{
	test_lifo_type_part* p_test_obj = reinterpret_cast<test_lifo_type_part*>( data );

	pthread_barrier_wait( &barrier );

	typename test_lifo_type_part::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		auto p_node = new test_lifo_type_part::node_type( i );
		p_test_obj->push( p_node );
		v++;
	}

	return reinterpret_cast<void*>( v );
}

/**
 * 各スレッドのメインルーチン。
 */
void* func_pop( void* data )
{
	test_lifo_type_part* p_test_obj = reinterpret_cast<test_lifo_type_part*>( data );

	pthread_barrier_wait( &barrier );

	uintptr_t v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		auto [p_node, val] = p_test_obj->pop();
		if ( p_node == nullptr ) {
			printf( "Gyaaaa!!!\n" );
			return reinterpret_cast<void*>( v );
			exit( 1 );
		}
		v++;
		//		delete p_node;
	}

	return reinterpret_cast<void*>( v );
}

int test_case1( void )
{
	test_lifo_type_part* p_test_obj = new test_lifo_type_part();

	pthread_barrier_init( &barrier, NULL, num_thread + 1 );
	pthread_t* threads = new pthread_t[num_thread];

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_push, reinterpret_cast<void*>( p_test_obj ) );
	}

	std::cout << "!!!Ready!!!" << std::endl;   // prints !!!Hello World!!!
	pthread_barrier_wait( &barrier );
	std::cout << "!!!GO!!!" << std::endl;   // prints !!!Hello World!!!

	int sum = 0;
	for ( int i = 0; i < num_thread; i++ ) {
		uintptr_t e;
		pthread_join( threads[i], reinterpret_cast<void**>( &e ) );
		//		std::cout << "Thread " << i << ": last dequeued = " << e << std::endl;
		sum += e;
	}

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_pop, reinterpret_cast<void*>( p_test_obj ) );
	}

	pthread_barrier_wait( &barrier );
	std::cout << "!!!GO 2nd!!!" << std::endl;   // prints !!!Hello World!!!
	sum = 0;
	for ( int i = 0; i < num_thread; i++ ) {
		uintptr_t e;
		pthread_join( threads[i], reinterpret_cast<void**>( &e ) );
		//		std::cout << "Thread " << i << ": last dequeued = " << e << std::endl;
		sum += e;
	}

	std::cout << "!!!Check!!!" << std::endl;   // prints !!!Hello World!!!
	auto [p_node, val] = p_test_obj->pop();
	if ( p_node != nullptr ) {
		// 全部読み出し完了いることが必要だが、残っていた。
		printf( "Gyaaaa!!!\n" );
		return 1;
	}

	std::cout << "Expect: " << std::to_string( num_thread * loop_num ) << std::endl;
	std::cout << "Sum:    " << sum << std::endl;
	if ( sum == num_thread * loop_num ) {
		std::cout << "OK! test_case1()" << std::endl;
	} else {
		std::cout << "NGGGGGGgggggg!" << std::endl;
		exit( 1 );
	}

	delete[] threads;

	delete p_test_obj;

	return 0;
}

/**
 * 各スレッドのメインルーチン。
 * テスト目的上、メモリリークが発生するが、想定通りの振る舞い。
 */
void* func_test_fifo2( void* data )
{
	test_lifo_type_part* p_test_obj = reinterpret_cast<test_lifo_type_part*>( data );

	pthread_barrier_wait( &barrier );

	typename test_lifo_type_part::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		auto p_node = new test_lifo_type_part::node_type( v );
		p_test_obj->push( p_node );
		auto [pop_flag, vv] = p_test_obj->pop();
		if ( !pop_flag ) {
			printf( "Buggggggg!!!  %llu\n", v );
			exit( 1 );
		}
		v = vv + 1;
	}

	return reinterpret_cast<void*>( v );
}

int test_case2( void )
{
	test_lifo_type_part* p_test_obj = new test_lifo_type_part();

	pthread_barrier_init( &barrier, NULL, num_thread + 1 );
	pthread_t* threads = new pthread_t[num_thread];

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_test_fifo2, reinterpret_cast<void*>( p_test_obj ) );
	}

	std::cout << "!!!Ready!!!" << std::endl;   // prints !!!Hello World!!!
	pthread_barrier_wait( &barrier );
	std::cout << "!!!GO!!!" << std::endl;   // prints !!!Hello World!!!

	int sum = 0;
	for ( int i = 0; i < num_thread; i++ ) {
		uintptr_t e;
		pthread_join( threads[i], reinterpret_cast<void**>( &e ) );
		std::cout << "Thread " << i << ": last dequeued = " << e << std::endl;
		sum += e;
	}

	//	std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );

	// 各スレッドが最後にdequeueした値の合計は num_thread * num_loop
	// に等しくなるはず。
	std::cout << "Expect: " << std::to_string( num_thread * loop_num ) << std::endl;
	std::cout << "Sum:    " << sum << std::endl;
	if ( sum == num_thread * loop_num ) {
		std::cout << "OK! test_case2()" << std::endl;
	} else {
		std::cout << "NGGGGGGgggggg!" << std::endl;
		exit( 1 );
	}

	delete[] threads;

	return 0;
}

using test_lifo_type = alpha::concurrent::stack_list<std::uintptr_t>;

/**
 * 各スレッドのメインルーチン。
 * stmへのカウントアップを繰り返す。
 */
void* func_test_fifo( void* data )
{
	test_lifo_type* p_test_obj = reinterpret_cast<test_lifo_type*>( data );

	pthread_barrier_wait( &barrier );

	typename test_lifo_type::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		p_test_obj->push( v );
		auto [pop_flag, vv] = p_test_obj->pop();
		if ( !pop_flag ) {
			printf( "Bugggggggyyyy  func_test_fifo()!!!  %llu\n", v );
			printf( "fifo size count: %d\n", p_test_obj->get_size() );
			exit( 1 );
		}
		v = vv + 1;
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
			auto [pop_flag, vv] = p_test_obj[0]->pop();
			if ( !pop_flag ) {
				printf( "Bugggggggyyyy!!!  func_test_fifo2()  %llu\n", v1 );
				printf( "fifo size count idx 0: %d\n", p_test_obj[0]->get_size() );
				exit( 1 );
			}
			v1 = vv + 1;
		}
		{
			auto [pop_flag, vv] = p_test_obj[1]->pop();
			if ( !pop_flag ) {
				printf( "Bugggggggyyyy!!!  func_test_fifo2()  %llu\n", v2 );
				printf( "fifo size count idx 1: %d\n", p_test_obj[1]->get_size() );
				exit( 1 );
			}
			v2 = vv + 1;
		}
	}

	return std::tuple<uintptr_t, uintptr_t>( v1, v2 );
}

int test_case3( void )
{
	//	test_lifo_type* p_test_obj = new test_lifo_type( num_thread );
	test_lifo_type* p_test_obj[2];
	p_test_obj[0] = new test_lifo_type();
	p_test_obj[1] = new test_lifo_type();

	pthread_barrier_init( &barrier, NULL, num_thread + 1 );
	pthread_t* threads = new pthread_t[num_thread];

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_test_fifo, reinterpret_cast<void*>( p_test_obj[i % 2] ) );
	}

	std::cout << "!!!Ready!!!" << std::endl;   // prints !!!Hello World!!!
	pthread_barrier_wait( &barrier );
	std::cout << "!!!GO!!!" << std::endl;   // prints !!!Hello World!!!

	auto [a1, a2] = func_test_fifo2( p_test_obj );
	std::cout << "Thread X: last dequeued = " << a1 << ", " << a2 << std::endl;

	int sum = a1 + a2;
	for ( int i = 0; i < num_thread; i++ ) {
		uintptr_t e;
		pthread_join( threads[i], reinterpret_cast<void**>( &e ) );
		std::cout << "Thread " << i << ": last dequeued = " << e << std::endl;
		sum += e;
	}

	//	std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );

	// 各スレッドが最後にdequeueした値の合計は num_thread * num_loop
	// に等しくなるはず。
	std::cout << "Expect: " << std::to_string( ( num_thread + 2 ) * loop_num ) << std::endl;
	std::cout << "Sum:    " << sum << std::endl;
	if ( sum == ( num_thread + 2 ) * loop_num ) {
		std::cout << "OK! test_case3()" << std::endl;
	} else {
		std::cout << "NGGGGGGgggggg!" << std::endl;
		exit( 1 );
	}

	delete[] threads;

	delete p_test_obj[0];
	delete p_test_obj[1];

	return 0;
}

int main( void )
{
	std::cout << "!!!Start World!!!" << std::endl;   // prints !!!Hello World!!!

	for ( int i = 0; i < 4; i++ ) {
		std::cout << "!!! " << i << " World!!!" << std::endl;   // prints !!!Hello World!!!
																//		test_case1();
																//		test_case2();
		test_case3();
	}

	std::cout << "!!!End World!!!" << std::endl;   // prints !!!Hello World!!!
	return 0;
}