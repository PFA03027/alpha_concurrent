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
#include <mutex>

#include "gtest/gtest.h"

#include "alconcurrent/lf_mem_alloc_type.hpp"
#include "alconcurrent/lf_stack.hpp"

constexpr int            num_thread = 10;   // Tested until 128.
constexpr std::uintptr_t loop_num   = 10000;

using test_lifo_type_part = alpha::concurrent::internal::lifo_nd_list<std::uintptr_t>;

pthread_barrier_t barrier;

// example
static alpha::concurrent::param_chunk_allocation param[] = {
	{ 32, 100 },
	{ 64, 100 },
	{ 128, 100 },
};

class lfStackTest : public ::testing::Test {
protected:
	virtual void SetUp()
	{
#ifndef NOT_USE_LOCK_FREE_MEM_ALLOC
		set_param_to_free_nd_mem_alloc( param, 3 );
#endif
	}

	virtual void TearDown()
	{
#ifndef NOT_USE_LOCK_FREE_MEM_ALLOC
		std::list<alpha::concurrent::chunk_statistics> statistics = alpha::concurrent::internal::node_of_list::get_statistics();

		for ( auto& e : statistics ) {
			printf( "%s\n", e.print().c_str() );
		}
#endif

		printf( "number of keys of pthread_key_create(),     %d\n", alpha::concurrent::internal::get_num_of_tls_key() );
		printf( "max number of keys of pthread_key_create(), %d\n", alpha::concurrent::internal::get_max_num_of_tls_key() );
	}
};

std::mutex                                  mtx_pointer_strage;
std::deque<test_lifo_type_part::node_type*> pointer_strage;   // 管理ノードを一時保管するストレージ。ownership=TRUEの管理ノードを想定

/**
 * 各スレッドのメインルーチン。
 */
void* func_push( void* data )
{
	std::deque<test_lifo_type_part::node_type*> pointer_strage_local;
	test_lifo_type_part*                        p_test_obj = reinterpret_cast<test_lifo_type_part*>( data );

	pthread_barrier_wait( &barrier );

	typename test_lifo_type_part::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		auto p_node = new test_lifo_type_part::node_type( i );
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
 * 各スレッドのメインルーチン。
 */
void* func_pop( void* data )
{
	test_lifo_type_part* p_test_obj = reinterpret_cast<test_lifo_type_part*>( data );

	pthread_barrier_wait( &barrier );

	uintptr_t v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		auto p_node = p_test_obj->pop();
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

TEST_F( lfStackTest, TC1 )
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

	uintptr_t sum = 0;
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
	auto p_node = p_test_obj->pop();

	EXPECT_EQ( nullptr, p_node );   // 全部読み出し完了いること＝p_nodeがnullptrとなっていること

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

	{
		int err_cnt, warn_cnt;
		alpha::concurrent::GetErrorWarningLogCount( &err_cnt, &warn_cnt );
		EXPECT_EQ( err_cnt, 0 );
		EXPECT_EQ( warn_cnt, 0 );
		alpha::concurrent::GetErrorWarningLogCountAndReset( &err_cnt, &warn_cnt );
		EXPECT_EQ( err_cnt, 0 );
		EXPECT_EQ( warn_cnt, 0 );
	}

	return;
}

std::mutex                                  mtx_pointer_strage2;
std::deque<test_lifo_type_part::node_type*> pointer_strage2;   // 管理ノードを一時保管するストレージ。ownership=TRUEの管理ノードを想定

/**
 * 各スレッドのメインルーチン。
 * テスト目的上、メモリリークが発生するが、想定通りの振る舞い。
 */
void* func_test_fifo2( void* data )
{
	std::deque<test_lifo_type_part::node_type*> pointer_strage_local;
	test_lifo_type_part*                        p_test_obj = reinterpret_cast<test_lifo_type_part*>( data );

	pthread_barrier_wait( &barrier );

	typename test_lifo_type_part::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		auto p_node = new test_lifo_type_part::node_type( v );
		pointer_strage_local.emplace_back( p_node );
		p_test_obj->push( p_node );
		auto p_pop_node = p_test_obj->pop();
		if ( p_pop_node == nullptr ) {
			printf( "Buggggggg!!!  %s\n", std::to_string( v ).c_str() );
			exit( 1 );
		}
		auto vv = p_pop_node->get_value();
		v       = vv + 1;
	}

	{
		std::lock_guard<std::mutex> lk( mtx_pointer_strage2 );
		for ( auto& e : pointer_strage_local ) {
			pointer_strage2.emplace_back( e );
		}
	}

	return reinterpret_cast<void*>( v );
}

TEST_F( lfStackTest, TC2 )
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

	uintptr_t sum = 0;
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

using test_lifo_type = alpha::concurrent::stack_list<std::uintptr_t>;

/**
 * 各スレッドのメインルーチン。
 * カウントアップを繰り返す。
 */
void* func_test_fifo( void* data )
{
	test_lifo_type* p_test_obj = reinterpret_cast<test_lifo_type*>( data );

	pthread_barrier_wait( &barrier );

	typename test_lifo_type::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		p_test_obj->push( v );
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
		auto [pop_flag, vv] = p_test_obj->pop();
#else
		auto local_ret = p_test_obj->pop();
		auto pop_flag  = std::get<0>( local_ret );
		auto vv        = std::get<1>( local_ret );
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

std::tuple<uintptr_t, uintptr_t> func_test_fifo2( test_lifo_type* p_test_obj[] )
{

	typename test_lifo_type::value_type v1 = 0;
	typename test_lifo_type::value_type v2 = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		p_test_obj[0]->push( v1 );
		p_test_obj[1]->push( v2 );
		{
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
			auto [pop_flag, vv] = p_test_obj[0]->pop();
#else
			auto local_ret = p_test_obj[0]->pop();
			auto pop_flag  = std::get<0>( local_ret );
			auto vv        = std::get<1>( local_ret );
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
			auto pop_flag  = std::get<0>( local_ret );
			auto vv        = std::get<1>( local_ret );
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

TEST_F( lfStackTest, TC3 )
{
	//	test_lifo_type* p_test_obj = new test_lifo_type( num_thread );
	test_lifo_type* p_test_obj[2];
	p_test_obj[0] = new test_lifo_type( num_thread );
	p_test_obj[1] = new test_lifo_type( num_thread );

	pthread_barrier_init( &barrier, NULL, num_thread + 1 );
	pthread_t* threads = new pthread_t[num_thread];

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_test_fifo, reinterpret_cast<void*>( p_test_obj[i % 2] ) );
	}
	std::cout << "!!!Ready!!!" << std::endl;

	std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
	std::cout << "!!!GO!!!" << std::endl;
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
	auto [a1, a2] = func_test_fifo2( p_test_obj );
#else
	auto local_ret = func_test_fifo2( p_test_obj );
	auto a1 = std::get<0>( local_ret );
	auto a2 = std::get<1>( local_ret );
#endif
	std::cout << "Thread X: last dequeued = " << a1 << ", " << a2 << std::endl;

	uintptr_t sum = a1 + a2;
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

	EXPECT_EQ( ( num_thread + 2 ) * loop_num, sum );

	delete[] threads;

	std::cout << "Allocated nodes #0:    " << p_test_obj[0]->get_allocated_num() << std::endl;
	std::cout << "Allocated nodes #1:    " << p_test_obj[1]->get_allocated_num() << std::endl;

	delete p_test_obj[0];
	delete p_test_obj[1];

	return;
}

using test_lifo_type2 = alpha::concurrent::stack_list<std::uintptr_t, false>;

/**
 * 各スレッドのメインルーチン。
 * カウントアップを繰り返す。
 */
void* func_test4_fifo( void* data )
{
	test_lifo_type2* p_test_obj = reinterpret_cast<test_lifo_type2*>( data );

	pthread_barrier_wait( &barrier );

	typename test_lifo_type2::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		bool push_ret;
		push_ret = p_test_obj->push( v );
		if ( !push_ret ) {
			while ( !push_ret ) {
				//				printf( "Sleep in short time func_test4_fifo()\n" );
				std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
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
			printf( "Bugggggggyyyy  func_test_fifo()!!!  %s\n", std::to_string( v ).c_str() );
			printf( "fifo size count: %d\n", p_test_obj->get_size() );
			exit( 1 );
		}
		v = vv + 1;
	}

	return reinterpret_cast<void*>( v );
}

std::tuple<uintptr_t, uintptr_t> func_test4_fifo2( test_lifo_type2* p_test_obj[] )
{

	typename test_lifo_type2::value_type v1 = 0;
	typename test_lifo_type2::value_type v2 = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		bool push_ret;
		push_ret = p_test_obj[0]->push( v1 );
		if ( !push_ret ) {
			while ( !push_ret ) {
				//				printf( "Sleep in short time func_test4_fifo2()\n" );
				std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
				push_ret = p_test_obj[0]->push( v1 );
			}
			//			printf( "Get!!! func_test4_fifo2()\n" );
		}
		push_ret = p_test_obj[1]->push( v2 );
		if ( !push_ret ) {
			while ( !push_ret ) {
				//				printf( "Sleep in short time func_test4_fifo2()\n" );
				std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
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

TEST_F( lfStackTest, TC4 )
{
	//	test_lifo_type2* p_test_obj = new test_lifo_type2( num_thread );
	test_lifo_type2* p_test_obj[2];
	p_test_obj[0] = new test_lifo_type2( 8 );
	p_test_obj[1] = new test_lifo_type2( 8 );

	pthread_barrier_init( &barrier, NULL, num_thread + 1 );
	pthread_t* threads = new pthread_t[num_thread];

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_test4_fifo, reinterpret_cast<void*>( p_test_obj[i % 2] ) );
	}
	std::cout << "!!!Ready!!!" << std::endl;

	std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
	std::cout << "!!!GO!!!" << std::endl;
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
	auto [a1, a2] = func_test4_fifo2( p_test_obj );
#else
	auto local_ret = func_test4_fifo2( p_test_obj );
	auto a1 = std::get<0>( local_ret );
	auto a2 = std::get<1>( local_ret );
#endif
	std::cout << "Thread X: last dequeued = " << a1 << ", " << a2 << std::endl;

	uintptr_t sum = a1 + a2;
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

	EXPECT_EQ( ( num_thread + 2 ) * loop_num, sum );

	delete[] threads;

	std::cout << "Allocated nodes #0:    " << p_test_obj[0]->get_allocated_num() << std::endl;
	std::cout << "Allocated nodes #1:    " << p_test_obj[1]->get_allocated_num() << std::endl;

	delete p_test_obj[0];
	delete p_test_obj[1];

	return;
}

TEST_F( lfStackTest, Pointer1 )
{
	using test_fifo_type3 = alpha::concurrent::stack_list<int*>;
	test_fifo_type3* p_test_obj;

	std::cout << "Pointer test#1" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push( new int() );

	delete p_test_obj;
}

TEST_F( lfStackTest, Pointer2 )
{
	using test_fifo_type3 = alpha::concurrent::stack_list<int*>;
	test_fifo_type3* p_test_obj;

	std::cout << "Pointer test#2" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push( new int() );
	auto ret = p_test_obj->pop();

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

TEST_F( lfStackTest, Array1 )
{
	using test_fifo_type3 = alpha::concurrent::stack_list<array_test[]>;
	test_fifo_type3* p_test_obj;

	std::cout << "Array array_test[] test#1" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push( new array_test[2] );

	delete p_test_obj;
}

TEST_F( lfStackTest, Array2 )
{
	using test_fifo_type3 = alpha::concurrent::stack_list<array_test[]>;
	test_fifo_type3* p_test_obj;

	std::cout << "Array array_test[] test#2" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push( new array_test[2] );
	auto ret = p_test_obj->pop();

	ASSERT_TRUE( std::get<0>( ret ) );

	delete[] std::get<1>( ret );
	delete p_test_obj;

	std::cout << "Array array_test[] test" << std::endl;
}

TEST_F( lfStackTest, FixedArray1 )
{
	using test_fifo_type3 = alpha::concurrent::stack_list<array_test[2]>;
	test_fifo_type3* p_test_obj;

	std::cout << "Array array_test[2] test#1" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	array_test tmp_data[2];
	p_test_obj->push( tmp_data );

	delete p_test_obj;
}

TEST_F( lfStackTest, FixedArray2 )
{
	using test_fifo_type3 = alpha::concurrent::stack_list<array_test[2]>;
	test_fifo_type3* p_test_obj;

	std::cout << "Array array_test[2] test#2" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	array_test tmp_data[2];

	p_test_obj->push( tmp_data );
	auto ret = p_test_obj->pop();

	ASSERT_TRUE( std::get<0>( ret ) );

	delete p_test_obj;

	std::cout << "Array array_test[2] test" << std::endl;
}
