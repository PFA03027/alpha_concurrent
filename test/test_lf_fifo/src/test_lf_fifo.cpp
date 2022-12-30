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
#include <iostream>
#include <list>
#include <mutex>
#include <random>

#include "gtest/gtest.h"

#include "alconcurrent/lf_fifo.hpp"
#include "alconcurrent/lf_mem_alloc_type.hpp"

constexpr int            num_thread = 8;   // Tested until 128.
constexpr std::uintptr_t loop_num   = 10000;

using test_fifo_type_part = alpha::concurrent::internal::fifo_nd_list<std::uintptr_t>;   // ownership=TRUE設定のfifoのベースクラスとして定義

using test_fifo_type  = alpha::concurrent::fifo_list<std::uintptr_t>;
using test_fifo_type2 = alpha::concurrent::fifo_list<std::uintptr_t, false>;

pthread_barrier_t barrier;

// example
static alpha::concurrent::param_chunk_allocation param[] = {
	{ 32, 100000 },
	{ 64, 100000 },
	{ 128, 100000 },
};

class lffifoTest : public ::testing::Test {
protected:
	virtual void SetUp()
	{
#ifndef ALCONCURRENT_CONF_NOT_USE_LOCK_FREE_MEM_ALLOC
		set_param_to_free_nd_mem_alloc( param, 3 );
#endif
	}

	virtual void TearDown()
	{
#ifndef ALCONCURRENT_CONF_NOT_USE_LOCK_FREE_MEM_ALLOC
		std::list<alpha::concurrent::chunk_statistics> statistics = alpha::concurrent::internal::node_of_list::get_statistics();

		for ( auto& e : statistics ) {
			printf( "%s\n", e.print().c_str() );
		}
#endif

		printf( "number of keys of dynamic_tls_key_create(),     %d\n", alpha::concurrent::internal::get_num_of_tls_key() );
		printf( "max number of keys of dynamic_tls_key_create(), %d\n", alpha::concurrent::internal::get_max_num_of_tls_key() );
	}
};

std::mutex                                  mtx_pointer_strage;
std::deque<test_fifo_type_part::node_type*> pointer_strage;   // 管理ノードを一時保管するストレージ。ownership=TRUEの管理ノードを想定

/**
 * pushのテストをするスレッドのメインルーチン。
 * 生成したポインタは、呼び出し元のTEST_F( lffifoTest, TC1 )で破棄される。
 */
void* func_push( void* data )
{
	std::deque<test_fifo_type_part::node_type*> pointer_strage_local;
	test_fifo_type_part*                        p_test_obj = reinterpret_cast<test_fifo_type_part*>( data );

	pthread_barrier_wait( &barrier );

	typename test_fifo_type_part::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		auto p_node = new test_fifo_type_part::node_type( i );
		pointer_strage_local.emplace_back( p_node );
		p_test_obj->push( p_node );
		v++;
	}

	{
		std::lock_guard<std::mutex> lk( mtx_pointer_strage );
		for ( auto& e : pointer_strage_local ) {
			pointer_strage.emplace_back( e );
		}
	}

	return reinterpret_cast<void*>( v );
}

/**
 * popの各スレッドのメインルーチン。
 */
void* func_pop( void* data )
{
	test_fifo_type_part* p_test_obj = reinterpret_cast<test_fifo_type_part*>( data );

	pthread_barrier_wait( &barrier );

	uintptr_t v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
		auto [p_node, val] = p_test_obj->pop();
#else
		auto local_ret = p_test_obj->pop();
		auto p_node    = std::get<0>( local_ret );
//		auto val       = std::get<1>( local_ret );
#endif
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

TEST_F( lffifoTest, TC1 )
{
	test_fifo_type_part* p_test_obj = new test_fifo_type_part();

	pthread_barrier_init( &barrier, NULL, num_thread + 1 );
	pthread_t* threads = new pthread_t[num_thread];

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_push, reinterpret_cast<void*>( p_test_obj ) );
	}

	std::cout << "!!!Ready!!!" << std::endl;   // prints !!!Hello World!!!
	pthread_barrier_wait( &barrier );
	std::cout << "!!!GO!!!" << std::endl;   // prints !!!Hello World!!!

	std::uintptr_t sum = 0;
	for ( int i = 0; i < num_thread; i++ ) {
		uintptr_t e;
		pthread_join( threads[i], reinterpret_cast<void**>( &e ) );
		std::cout << "Thread " << i << ": last dequeued = " << e << std::endl;
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
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
	auto [p_node, val] = p_test_obj->pop();
#else
	auto local_ret = p_test_obj->pop();
	auto p_node = std::get<0>( local_ret );
//	auto val = std::get<1>( local_ret );
#endif

	EXPECT_EQ( nullptr, p_node );   // 全部読み出し完了いること(p_node == nullptr)が期待値。もし残っていたらNG。

	std::cout << "Expect: " << std::to_string( num_thread * loop_num ) << std::endl;
	std::cout << "Sum:    " << sum << std::endl;
	EXPECT_EQ( num_thread * loop_num, sum );

	{
		std::lock_guard<std::mutex> lk( mtx_pointer_strage );
		for ( auto& e : pointer_strage ) {
			delete e;
		}
		pointer_strage.clear();
	}

	delete[] threads;

	delete p_test_obj;

	return;
}

std::mutex                                  mtx_pointer_strage2;
std::deque<test_fifo_type_part::node_type*> pointer_strage2;   // 管理ノードを一時保管するストレージ。ownership=TRUEの管理ノードを想定

/**
 * 各スレッドのメインルーチン。
 * テスト目的上、メモリリークが発生するが、想定通りの振る舞い。
 */
void* func_test_fifo2( void* data )
{
	std::deque<test_fifo_type_part::node_type*> pointer_strage_local;
	test_fifo_type_part*                        p_test_obj = reinterpret_cast<test_fifo_type_part*>( data );

	pthread_barrier_wait( &barrier );

	typename test_fifo_type_part::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		auto p_node = new test_fifo_type_part::node_type( v );
		pointer_strage_local.emplace_back( p_node );
		p_test_obj->push( p_node );
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
		auto [p_poped_node, vv] = p_test_obj->pop();
#else
		auto local_ret = p_test_obj->pop();
		auto p_poped_node = std::get<0>( local_ret );
		auto vv = std::get<1>( local_ret );
#endif
		if ( p_poped_node == nullptr ) {
			printf( "Buggggggg!!!  %s\n", std::to_string( v ).c_str() );
			exit( 1 );
		}
		v = vv + 1;
	}

	{
		std::lock_guard<std::mutex> lk( mtx_pointer_strage2 );
		for ( auto& e : pointer_strage_local ) {
			pointer_strage2.emplace_back( e );
		}
	}

	return reinterpret_cast<void*>( v );
}

TEST_F( lffifoTest, TC2 )
{
	test_fifo_type_part* p_test_obj = new test_fifo_type_part();

	pthread_barrier_init( &barrier, NULL, num_thread + 1 );
	pthread_t* threads = new pthread_t[num_thread];

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_test_fifo2, reinterpret_cast<void*>( p_test_obj ) );
	}

	std::cout << "!!!Ready!!!" << std::endl;   // prints !!!Hello World!!!
	pthread_barrier_wait( &barrier );
	std::cout << "!!!GO!!!" << std::endl;   // prints !!!Hello World!!!

	std::uintptr_t sum = 0;
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
	EXPECT_EQ( num_thread * loop_num, sum );

	{
		std::lock_guard<std::mutex> lk( mtx_pointer_strage2 );
		for ( auto& e : pointer_strage2 ) {
			delete e;
		}
		pointer_strage2.clear();
	}

	delete[] threads;
	delete p_test_obj;

	return;
}

/**
 * 各スレッドのメインルーチン。
 * カウントアップを繰り返す。
 */
template <typename TEST_FIFO_TYPE>
void* func_test_fifo( void* data )
{
	TEST_FIFO_TYPE* p_test_obj = reinterpret_cast<TEST_FIFO_TYPE*>( data );

	pthread_barrier_wait( &barrier );

	typename TEST_FIFO_TYPE::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		p_test_obj->push( v );
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
		auto [pop_flag, vv] = p_test_obj->pop();
#else
		auto local_ret = p_test_obj->pop();
		auto pop_flag = std::get<0>( local_ret );
		auto vv = std::get<1>( local_ret );
#endif
		if ( !pop_flag ) {
			printf( "Bugggggggyyyy  func_test_fifo()!!!  %s\n", std::to_string( v ).c_str() );
			printf( "fifo size count: %d\n", p_test_obj->get_size() );
			exit( 1 );
		}
		v = vv + 1;
	}

	return reinterpret_cast<void*>( v );
}

template <typename TEST_FIFO_TYPE>
std::tuple<uintptr_t, uintptr_t> func_test_fifo2( TEST_FIFO_TYPE* p_test_obj[] )
{

	typename TEST_FIFO_TYPE::value_type v1 = 0;
	typename TEST_FIFO_TYPE::value_type v2 = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		p_test_obj[0]->push( v1 );
		p_test_obj[1]->push( v2 );
		{
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
			auto [pop_flag, vv] = p_test_obj[0]->pop();
#else
			auto local_ret = p_test_obj[0]->pop();
			auto pop_flag = std::get<0>( local_ret );
			auto vv = std::get<1>( local_ret );
#endif
			if ( !pop_flag ) {
				printf( "Bugggggggyyyy!!!  func_test_fifo2()  %s\n", std::to_string( v1 ).c_str() );
				printf( "fifo size count idx 0: %d\n", p_test_obj[0]->get_size() );
				exit( 1 );
			}
			v1 = vv + 1;
		}
		{
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
			auto [pop_flag, vv] = p_test_obj[1]->pop();
#else
			auto local_ret = p_test_obj[1]->pop();
			auto pop_flag = std::get<0>( local_ret );
			auto vv = std::get<1>( local_ret );
#endif
			if ( !pop_flag ) {
				printf( "Bugggggggyyyy!!!  func_test_fifo2()  %s\n", std::to_string( v2 ).c_str() );
				printf( "fifo size count idx 1: %d\n", p_test_obj[1]->get_size() );
				exit( 1 );
			}
			v2 = vv + 1;
		}
	}

	return std::tuple<uintptr_t, uintptr_t>( v1, v2 );
}

template <typename TEST_FIFO_TYPE>
void test_case3( void )
{
	//	test_fifo_type* p_test_obj = new TEST_FIFO_TYPE( num_thread );
	TEST_FIFO_TYPE* p_test_obj[2];
	p_test_obj[0] = new TEST_FIFO_TYPE( num_thread );
	p_test_obj[1] = new TEST_FIFO_TYPE( num_thread );

	pthread_barrier_init( &barrier, NULL, num_thread + 1 );
	pthread_t* threads = new pthread_t[num_thread];

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_test_fifo<TEST_FIFO_TYPE>, reinterpret_cast<void*>( p_test_obj[i % 2] ) );
	}
	std::cout << "!!!Ready!!!" << std::endl;

	std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
	std::cout << "!!!GO!!!" << std::endl;
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
	auto [a1, a2] = func_test_fifo2<TEST_FIFO_TYPE>( p_test_obj );
#else
	auto local_ret = func_test_fifo2<TEST_FIFO_TYPE>( p_test_obj );
	auto a1 = std::get<0>( local_ret );
	auto a2 = std::get<1>( local_ret );
#endif
	std::cout << "Thread X: last dequeued = " << a1 << ", " << a2;
	std::cout << std::endl;

	std::uintptr_t sum = a1 + a2;
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

	ASSERT_EQ( ( num_thread + 2 ) * loop_num, sum );

	delete[] threads;

	std::cout << "Allocated nodes #0:    " << p_test_obj[0]->get_allocated_num() << std::endl;
	std::cout << "Allocated nodes #1:    " << p_test_obj[1]->get_allocated_num() << std::endl;

	delete p_test_obj[0];
	delete p_test_obj[1];

	return;
}

TEST_F( lffifoTest, TC3 )
{
	ASSERT_NO_FATAL_FAILURE( test_case3<test_fifo_type>() );
}

/**
 * 各スレッドのメインルーチン。
 * ウントアップを繰り返す。
 */
template <typename TEST_FIFO_TYPE>
void* func_test4_fifo( void* data )
{
	std::random_device seed_gen;
	std::mt19937       engine( seed_gen() );

	// 0以上9以下の値を等確率で発生させる
	std::uniform_int_distribution<> dist( 0, 9 );

	TEST_FIFO_TYPE* p_test_obj = reinterpret_cast<TEST_FIFO_TYPE*>( data );

	pthread_barrier_wait( &barrier );

	typename TEST_FIFO_TYPE::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		bool push_ret;
		push_ret = p_test_obj->push( v );
		if ( !push_ret ) {
			while ( !push_ret ) {
				//				printf( "Sleep in short time func_test4_fifo()\n" );
				std::this_thread::sleep_for( std::chrono::milliseconds( 1 + dist( engine ) ) );   // backoff handling
				push_ret = p_test_obj->push( v );
			}
			//			printf( "Get!!! func_test4_fifo2()\n" );
		}
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
		auto [pop_flag, vv] = p_test_obj->pop();
#else
		auto local_ret = p_test_obj->pop();
		auto pop_flag = std::get<0>( local_ret );
		auto vv = std::get<1>( local_ret );
#endif
		if ( !pop_flag ) {
			printf( "Bugggggggyyyy  func_test4_fifo()!!!  %s\n", std::to_string( v ).c_str() );
			printf( "fifo size count: %d\n", p_test_obj->get_size() );
			exit( 1 );
		}
		v = vv + 1;
	}

	return reinterpret_cast<void*>( v );
}

template <typename TEST_FIFO_TYPE>
std::tuple<uintptr_t, uintptr_t> func_test4_fifo2( TEST_FIFO_TYPE* p_test_obj[] )
{
	std::random_device seed_gen;
	std::mt19937       engine( seed_gen() );

	// 0以上9以下の値を等確率で発生させる
	std::uniform_int_distribution<> dist( 0, 9 );

	typename TEST_FIFO_TYPE::value_type v1 = 0;
	typename TEST_FIFO_TYPE::value_type v2 = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		bool push_ret;
		push_ret = p_test_obj[0]->push( v1 );
		if ( !push_ret ) {
			while ( !push_ret ) {
				//				printf( "Sleep in short time func_test4_fifo2()\n" );
				std::this_thread::sleep_for( std::chrono::milliseconds( 1 + dist( engine ) ) );   // backoff handling
				push_ret = p_test_obj[0]->push( v1 );
			}
			//			printf( "Get!!! func_test4_fifo2()\n" );
		}
		push_ret = p_test_obj[1]->push( v2 );
		if ( !push_ret ) {
			while ( !push_ret ) {
				//				printf( "Sleep in short time func_test4_fifo2()\n" );
				std::this_thread::sleep_for( std::chrono::milliseconds( 1 + dist( engine ) ) );   // backoff handling
				push_ret = p_test_obj[1]->push( v2 );
			}
			//			printf( "Get!!! func_test4_fifo2()\n" );
		}

		{
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
			auto [pop_flag, vv] = p_test_obj[0]->pop();
#else
			auto local_ret = p_test_obj[0]->pop();
			auto pop_flag = std::get<0>( local_ret );
			auto vv = std::get<1>( local_ret );
#endif
			if ( !pop_flag ) {
				printf( "Bugggggggyyyy!!!  func_test4_fifo2()  %s\n", std::to_string( v1 ).c_str() );
				printf( "fifo size count idx 0: %d\n", p_test_obj[0]->get_size() );
				exit( 1 );
			}
			v1 = vv + 1;
		}
		{
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
			auto [pop_flag, vv] = p_test_obj[1]->pop();
#else
			auto local_ret = p_test_obj[1]->pop();
			auto pop_flag = std::get<0>( local_ret );
			auto vv = std::get<1>( local_ret );
#endif
			if ( !pop_flag ) {
				printf( "Bugggggggyyyy!!!  func_test4_fifo2()  %s\n", std::to_string( v2 ).c_str() );
				printf( "fifo size count idx 1: %d\n", p_test_obj[1]->get_size() );
				exit( 1 );
			}
			v2 = vv + 1;
		}
	}

	return std::tuple<uintptr_t, uintptr_t>( v1, v2 );
}

template <typename TEST_FIFO_TYPE>
void test_case4( void )
{
	//	test_fifo_type* p_test_obj = new TEST_FIFO_TYPE( num_thread );
	TEST_FIFO_TYPE* p_test_obj[2];
	p_test_obj[0] = new TEST_FIFO_TYPE( 8 );
	p_test_obj[1] = new TEST_FIFO_TYPE( 8 );

	pthread_barrier_init( &barrier, NULL, num_thread + 1 );
	pthread_t* threads = new pthread_t[num_thread];

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_test4_fifo<TEST_FIFO_TYPE>, reinterpret_cast<void*>( p_test_obj[i % 2] ) );
	}
	std::cout << "!!!Ready!!!" << std::endl;

	std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
	std::cout << "!!!GO!!!" << std::endl;
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
	auto [a1, a2] = func_test4_fifo2<TEST_FIFO_TYPE>( p_test_obj );
#else
	auto local_ret = func_test4_fifo2<TEST_FIFO_TYPE>( p_test_obj );
	auto a1 = std::get<0>( local_ret );
	auto a2 = std::get<1>( local_ret );
#endif
	std::cout << "Thread X: last dequeued = " << a1 << ", " << a2;
	std::cout << std::endl;

	std::uintptr_t sum = a1 + a2;
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
	ASSERT_EQ( ( num_thread + 2 ) * loop_num, sum );

	delete[] threads;

	std::cout << "Allocated nodes #0:    " << p_test_obj[0]->get_allocated_num() << std::endl;
	std::cout << "Allocated nodes #1:    " << p_test_obj[1]->get_allocated_num() << std::endl;

	delete p_test_obj[0];
	delete p_test_obj[1];

	return;
}
TEST_F( lffifoTest, TC4 )
{
	ASSERT_NO_FATAL_FAILURE( test_case4<test_fifo_type2>() );
}

TEST_F( lffifoTest, Pointer1 )
{
	using test_fifo_type3 = alpha::concurrent::fifo_list<int*>;
	test_fifo_type3* p_test_obj;

	std::cout << "Pointer test#1" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push( new int() );

	delete p_test_obj;
}

TEST_F( lffifoTest, Pointer2 )
{
	using test_fifo_type3 = alpha::concurrent::fifo_list<int*>;
	test_fifo_type3* p_test_obj;

	std::cout << "Pointer test#2" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push( new int() );
	auto ret = p_test_obj->pop();

	ASSERT_TRUE( std::get<0>( ret ) );

	delete std::get<1>( ret );
	delete p_test_obj;
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

	int x;

private:
};

TEST_F( lffifoTest, Array1 )
{
	using test_fifo_type3 = alpha::concurrent::fifo_list<array_test[]>;
	test_fifo_type3* p_test_obj;

	std::cout << "Array array_test[] test#1" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push( new array_test[2] );

	delete p_test_obj;
}

TEST_F( lffifoTest, Array2 )
{
	using test_fifo_type3 = alpha::concurrent::fifo_list<array_test[]>;
	test_fifo_type3* p_test_obj;

	std::cout << "Array array_test[] test#2" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push( new array_test[2] );
	auto ret = p_test_obj->pop();

	ASSERT_TRUE( std::get<0>( ret ) );

	delete[] std::get<1>( ret );
	delete p_test_obj;
}

TEST_F( lffifoTest, FixedArray1 )
{
	using test_fifo_type3 = alpha::concurrent::fifo_list<array_test[2]>;
	test_fifo_type3* p_test_obj;

	std::cout << "Array array_test[2] test#1" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	array_test tmp_data[2];
	tmp_data[0].x = 2;
	tmp_data[1].x = 3;
	p_test_obj->push( tmp_data );

	delete p_test_obj;
}

TEST_F( lffifoTest, FixedArray2 )
{
	using test_fifo_type3 = alpha::concurrent::fifo_list<array_test[2]>;
	test_fifo_type3* p_test_obj;

	array_test tmp_data[2];
	tmp_data[0].x = 2;
	tmp_data[1].x = 3;

	std::cout << "Array array_test[2] test#2" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push( tmp_data );
	auto ret = p_test_obj->pop();

	ASSERT_TRUE( std::get<0>( ret ) );

	auto vt = std::get<1>( ret );

	EXPECT_EQ( 2, vt[0].x );
	EXPECT_EQ( 3, vt[1].x );

	delete p_test_obj;

	std::cout << "End Array array_test[2] test" << std::endl;
}
