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
#include <memory>
#include <mutex>
#include <random>

#include "gtest/gtest.h"

#include "alconcurrent/lf_fifo.hpp"
#include "alconcurrent/lf_mem_alloc_type.hpp"

constexpr int            num_thread = 1;   // Tested until 128.
constexpr std::uintptr_t loop_num   = 10000;

using test_fifo_type = alpha::concurrent::fifo_list<std::uintptr_t>;

pthread_barrier_t barrier;

class lffifoTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		alpha::concurrent::gmem_prune();
	}

	void TearDown() override
	{
		auto statistics = alpha::concurrent::internal::node_of_list::get_statistics();
		printf( "%s\n", statistics.print().c_str() );

		printf( "number of keys of dynamic_tls_key_create(),     %d\n", alpha::concurrent::internal::get_num_of_tls_key() );
		printf( "max number of keys of dynamic_tls_key_create(), %d\n", alpha::concurrent::internal::get_max_num_of_tls_key() );
	}
};

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
		auto pop_flag  = std::get<0>( local_ret );
		auto vv        = std::get<1>( local_ret );
#endif
		if ( !pop_flag ) {
			printf( "Bugggggggyyyy  func_test_fifo()!!!  %s\n", std::to_string( i ).c_str() );
#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
			printf( "fifo size count: %d\n", p_test_obj->get_size() );
#endif
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
			auto pop_flag  = std::get<0>( local_ret );
			auto vv        = std::get<1>( local_ret );
#endif
			if ( !pop_flag ) {
				printf( "Bugggggggyyyy!!!  func_test_fifo2()  %s\n", std::to_string( i ).c_str() );
#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
				printf( "fifo size count idx 0: %d\n", p_test_obj[0]->get_size() );
#endif
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
#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
				printf( "fifo size count idx 1: %d\n", p_test_obj[1]->get_size() );
#endif
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
	auto a1        = std::get<0>( local_ret );
	auto a2        = std::get<1>( local_ret );
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

#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
	std::cout << "Allocated nodes #0:    " << p_test_obj[0]->get_allocated_num() << std::endl;
	std::cout << "Allocated nodes #1:    " << p_test_obj[1]->get_allocated_num() << std::endl;
#endif

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
		auto pop_flag  = std::get<0>( local_ret );
		auto vv        = std::get<1>( local_ret );
#endif
		if ( !pop_flag ) {
			printf( "Bugggggggyyyy  func_test4_fifo()!!!  %s\n", std::to_string( v ).c_str() );
#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
			printf( "fifo size count: %d\n", p_test_obj->get_size() );
#endif
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
			auto pop_flag  = std::get<0>( local_ret );
			auto vv        = std::get<1>( local_ret );
#endif
			if ( !pop_flag ) {
				printf( "Bugggggggyyyy!!!  func_test4_fifo2()  %s\n", std::to_string( v1 ).c_str() );
#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
				printf( "fifo size count idx 0: %d\n", p_test_obj[0]->get_size() );
#endif
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
				printf( "Bugggggggyyyy!!!  func_test4_fifo2()  %s\n", std::to_string( v2 ).c_str() );
#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
				printf( "fifo size count idx 1: %d\n", p_test_obj[1]->get_size() );
#endif
				exit( 1 );
			}
			v2 = vv + 1;
		}
	}

#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
	printf( "final count of p_test_obj[0] is %d", p_test_obj[0]->get_size() );
#endif

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
	auto a1        = std::get<0>( local_ret );
	auto a2        = std::get<1>( local_ret );
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

#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
	std::cout << "Allocated nodes #0:    " << p_test_obj[0]->get_allocated_num() << std::endl;
	std::cout << "Allocated nodes #1:    " << p_test_obj[1]->get_allocated_num() << std::endl;
#endif

	delete p_test_obj[0];
	delete p_test_obj[1];

	return;
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

TEST_F( lffifoTest, CanCal_With_Unique_ptr )
{
	// Arrange
	using test_fifo_type3 = alpha::concurrent::fifo_list<std::unique_ptr<int>>;
	test_fifo_type3 test_obj;

	std::unique_ptr<int> up_tv( new int );
	*up_tv = 12;

	// Act
	test_obj.push( std::move( up_tv ) );
	auto ret = test_obj.pop();

	// Assert
	ASSERT_TRUE( std::get<0>( ret ) );
	ASSERT_NE( std::get<1>( ret ), nullptr );
	EXPECT_EQ( *( std::get<1>( ret ) ), 12 );
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

// 以下はtrivially_copyableではないクラスでfifoを使用する場合のテストケース。
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
