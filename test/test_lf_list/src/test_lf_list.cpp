//============================================================================
// Name        : test_lf_list.cpp
// Author      : Teruaki Ata
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C, Ansi-style
//============================================================================

#include <pthread.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <thread>

#include "gtest/gtest.h"

#include "alconcurrent/lf_list.hpp"
#include "alconcurrent/lf_mem_alloc_type.hpp"

constexpr int            num_thread = 12;   // Tested until 128.
constexpr std::uintptr_t loop_num   = 10000;

using test_list = alpha::concurrent::lockfree_list<std::uintptr_t>;

pthread_barrier_t barrier;

// example
static alpha::concurrent::param_chunk_allocation param[] = {
	{ 32, 10000 },
	{ 64, 10000 },
	{ 128, 10000 },
};

class lflistTest : public ::testing::Test {
protected:
	virtual void SetUp()
	{
		set_param_to_free_nd_mem_alloc( param, 3 );
		alpha::concurrent::gmem_prune();
	}

	virtual void TearDown()
	{
		std::list<alpha::concurrent::chunk_statistics> statistics = alpha::concurrent::internal::node_of_list::get_statistics();

		for ( auto& e : statistics ) {
			printf( "%s\n", e.print().c_str() );
		}

		printf( "number of keys of dynamic_tls_key_create(),     %d\n", alpha::concurrent::internal::get_num_of_tls_key() );
		printf( "max number of keys of dynamic_tls_key_create(), %d\n", alpha::concurrent::internal::get_max_num_of_tls_key() );
	}
};

/**
 * 各スレッドの先頭から追加して、最後から取り出すことで、カウントアップを繰り返す。
 */
void* func_test_list_front2back( void* data )
{
	test_list* p_test_obj = reinterpret_cast<test_list*>( data );

	pthread_barrier_wait( &barrier );

	typename test_list::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		if ( !p_test_obj->push_front( v ) ) {
			printf( "Bugggggggyyyy  func_test_list_front2back()!!!  %s\n", std::to_string( v ).c_str() );
			printf( "list size count: %d\n", p_test_obj->get_size() );
			exit( 1 );
		}
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
		auto [pop_flag, vv] = p_test_obj->pop_back();
#else
		auto local_ret = p_test_obj->pop_back();
		auto pop_flag  = std::get<0>( local_ret );
		auto vv        = std::get<1>( local_ret );
#endif
		if ( !pop_flag ) {
			printf( "Bugggggggyyyy  func_test_list_front2back()!!!  %s\n", std::to_string( v ).c_str() );
			printf( "list size count: %d\n", p_test_obj->get_size() );
			exit( 1 );
		}
		v = vv + 1;
	}

	return reinterpret_cast<void*>( v );
}

/**
 * 各スレッドの最後から追加して、先頭から取り出すことで、カウントアップを繰り返す。
 */
void* func_test_list_back2front( void* data )
{
	test_list* p_test_obj = reinterpret_cast<test_list*>( data );

	pthread_barrier_wait( &barrier );

	typename test_list::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		if ( !p_test_obj->push_back( v ) ) {
			printf( "Bugggggggyyyy  func_test_list_back2front() by push_back!!!  %s\n", std::to_string( v ).c_str() );
			printf( "list size count: %d\n", p_test_obj->get_size() );
			exit( 1 );
		}
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
		auto [pop_flag, vv] = p_test_obj->pop_front();
#else
		auto local_ret = p_test_obj->pop_front();
		auto pop_flag  = std::get<0>( local_ret );
		auto vv        = std::get<1>( local_ret );
#endif
		if ( !pop_flag ) {
			printf( "Bugggggggyyyy  func_test_list_back2front() by pop_front!!!  %s\n", std::to_string( v ).c_str() );
			printf( "list size count: %d\n", p_test_obj->get_size() );
			exit( 1 );
		}
		v = vv + 1;
	}

	return reinterpret_cast<void*>( v );
}

TEST_F( lflistTest, TC1 )
{
	test_list count_list;

	pthread_barrier_init( &barrier, NULL, num_thread * 2 + 1 );
	pthread_t* threads = new pthread_t[num_thread * 2];

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_test_list_front2back, reinterpret_cast<void*>( &count_list ) );
		// pthread_create( &threads[i], NULL, func_test_list_back2front, reinterpret_cast<void*>( &count_list ) );
	}

	for ( int i = 0; i < num_thread; i++ ) {
		// pthread_create( &threads[num_thread + i], NULL, func_test_list_front2back, reinterpret_cast<void*>( &count_list ) );
		pthread_create( &threads[num_thread + i], NULL, func_test_list_back2front, reinterpret_cast<void*>( &count_list ) );
	}

	std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

	uintptr_t sum = 0;
	for ( int i = 0; i < num_thread * 2; i++ ) {
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
	std::cout << "Expect: " << std::to_string( num_thread * 2 * loop_num ) << std::endl;
	std::cout << "Sum:    " << sum << std::endl;

	EXPECT_EQ( num_thread * 2 * loop_num, sum );

	delete[] threads;

	std::cout << "Allocated nodes:    " << count_list.get_allocated_num() << std::endl;

	return;
}

constexpr typename test_list::value_type target_value = 1;
constexpr typename test_list::value_type target_min   = target_value - 1;
constexpr typename test_list::value_type target_max   = target_value + 1;

/**
 * 各スレッドの先頭から追加して、最後から取り出すことで、カウントアップを繰り返す。
 */
void* func_test_list_insert_remove( void* data )
{
	test_list* p_test_obj = reinterpret_cast<test_list*>( data );

	test_list::predicate_t search_insert_pos = []( const test_list::value_type& a ) -> bool {
		return ( a > target_value );
	};

	test_list::predicate_t search_remove_data = []( const test_list::value_type& a ) -> bool {
		return ( a == target_value );
	};

	std::random_device seed_gen;
	std::mt19937       engine( seed_gen() );

	// 0以上9以下の値を等確率で発生させる
	std::uniform_int_distribution<> dist( 0, 9 );

	pthread_barrier_wait( &barrier );

	typename test_list::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		while ( true ) {
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
			auto [ins_chk_ret, ins_allc_ret] = p_test_obj->insert( target_value, search_insert_pos );
#else
            auto local_ret    = p_test_obj->insert( target_value, search_insert_pos );
            auto ins_chk_ret  = std::get<0>( local_ret );
            auto ins_allc_ret = std::get<1>( local_ret );
#endif
			if ( ins_chk_ret ) break;
			if ( ins_allc_ret ) {
				// フリーノードストレージからの管理ノードアロケーションに成功しながらも、挿入位置を見つけられなかったことを示す。
				// テスト条件として、これは起きてはならないため、エラー終了する。
				printf( "Bugggggggyyyy  func_test_list_insert_remove()!!!  %s\n", std::to_string( i ).c_str() );
				printf( "list size count: %d\n", p_test_obj->get_size() );
				return reinterpret_cast<void*>( 0 );
			} else {
				// バックオフ処理を行って次の機会を狙う
				std::this_thread::sleep_for( std::chrono::milliseconds( 1 + dist( engine ) ) );   // backoff handling
			}
		}

		if ( p_test_obj->remove_one_if( search_remove_data ) ) {
			v++;
		}
	}

	return reinterpret_cast<void*>( v );
}

TEST_F( lflistTest, TC2 )
{
	test_list count_list;

	count_list.push_back( target_min );
	count_list.push_back( target_max );

	pthread_barrier_init( &barrier, NULL, num_thread + 1 );
	pthread_t* threads = new pthread_t[num_thread];

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_test_list_insert_remove, reinterpret_cast<void*>( &count_list ) );
	}

	std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

	uintptr_t sum = 0;
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
	std::cout << "Expect: " << std::to_string( num_thread * loop_num ) << std::endl;
	std::cout << "Sum:    " << sum << std::endl;

	EXPECT_EQ( num_thread * loop_num, sum );

	delete[] threads;

	std::cout << "nodes:              " << count_list.get_size() << std::endl;
	std::cout << "Allocated nodes:    " << count_list.get_allocated_num() << std::endl;

	return;
}

struct data_tc {
	test_list*            p_test_obj;
	test_list::value_type tc_data;
};

/**
 * 各スレッドの先頭から追加して、最後から取り出すことで、カウントアップを繰り返す。
 */
void* func_test_list_push( void* data )
{
	data_tc*   p_tc       = reinterpret_cast<data_tc*>( data );
	test_list* p_test_obj = p_tc->p_test_obj;

	printf( "func_test_list_push()!!! -> %s\n", std::to_string( p_tc->tc_data ).c_str() );

	pthread_barrier_wait( &barrier );

	typename test_list::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		if ( !p_test_obj->push_front( p_tc->tc_data ) ) {
			printf( "Bugggggggyyyy  func_test_list_push()!!!  %s\n", std::to_string( v ).c_str() );
			printf( "list size count: %d\n", p_test_obj->get_size() );
			return reinterpret_cast<void*>( v );
		}
		v++;
		if ( ( i % 100 ) == 0 ) {
			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );   // backoff handling
		}
	}

	return reinterpret_cast<void*>( v );
}

/**
 * 各スレッドの先頭から追加して、最後から取り出すことで、カウントアップを繰り返す。
 */
void* func_test_list_remove_all( void* data )
{
	data_tc*   p_tc       = reinterpret_cast<data_tc*>( data );
	test_list* p_test_obj = p_tc->p_test_obj;

	test_list::predicate_t search_remove_data = [p_tc]( const test_list::value_type& a ) -> bool {
		return ( p_tc->tc_data == a );
	};

	printf( "func_test_list_remove_all()!!! -> %s\n", std::to_string( p_tc->tc_data ).c_str() );

	pthread_barrier_wait( &barrier );

	typename test_list::value_type v = 0;
	while ( v < loop_num ) {
		test_list::value_type v_tmp = p_test_obj->remove_all_if( search_remove_data );
		v += v_tmp;
		if ( v_tmp == 0 ) {
			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );   // backoff handling
		}
	}

	return reinterpret_cast<void*>( v );
}

TEST_F( lflistTest, TC3 )
{
	test_list count_list;

	pthread_barrier_init( &barrier, NULL, num_thread * 2 + 1 );
	pthread_t* threads       = new pthread_t[num_thread * 2];
	data_tc*   test_data_set = new data_tc[num_thread];

	for ( int i = 0; i < num_thread; i++ ) {
		test_data_set[i].p_test_obj = &count_list;
		test_data_set[i].tc_data    = i;
	}

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_test_list_push, reinterpret_cast<void*>( &( test_data_set[i] ) ) );
	}

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[num_thread + i], NULL, func_test_list_remove_all, reinterpret_cast<void*>( &( test_data_set[i] ) ) );
	}

	// std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

	uintptr_t sum = 0;
	for ( int i = 0; i < num_thread * 2; i++ ) {
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
	std::cout << "Expect: " << std::to_string( num_thread * 2 * loop_num ) << std::endl;
	std::cout << "Sum:    " << sum << std::endl;
	EXPECT_EQ( num_thread * 2 * loop_num, sum );

	delete[] threads;
	delete[] test_data_set;

	std::cout << "nodes:              " << count_list.get_size() << std::endl;
	std::cout << "Allocated nodes:    " << count_list.get_allocated_num() << std::endl;

	return;
}

TEST_F( lflistTest, TC4 )
{
	test_list count_list;

	for ( std::uintptr_t i = 0; i <= loop_num; i++ ) {
		ASSERT_TRUE( count_list.push_front( i ) );
		//			printf( "Bugggggggyyyy  func_test_list_push()!!!  %llu\n", i );
		//			printf( "list size count: %d\n", count_list.get_size() );
	}

	std::uintptr_t sum = 0;
	count_list.for_each( [&sum]( test_list::value_type& ref_value ) {
		sum += ref_value;
	} );

	std::uintptr_t expect = 0;
	if ( ( loop_num % 2 ) == 0 ) {
		// 偶数の場合
		expect = loop_num * ( loop_num / 2 ) + loop_num / 2;
	} else {
		// 奇数の場合
		expect = loop_num * ( ( loop_num + 1 ) / 2 );
	}
	std::cout << "Expect: " << expect << std::endl;
	std::cout << "Sum:    " << sum << std::endl;

	EXPECT_EQ( expect, sum );

	return;
}

TEST_F( lflistTest, Pointer )
{
	using test_fifo_type3 = alpha::concurrent::lockfree_list<int*>;
	test_fifo_type3* p_test_obj;

	std::cout << "Pointer test#1" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push_front( new int() );

	delete p_test_obj;

	std::cout << "Pointer test#2" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push_front( new int() );
	auto ret = p_test_obj->pop_front();

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

TEST_F( lflistTest, Array )
{
	using test_fifo_type3 = alpha::concurrent::lockfree_list<array_test[]>;
	test_fifo_type3* p_test_obj;

	std::cout << "Array array_test[] test#1" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push_front( new array_test[2] );

	delete p_test_obj;

	std::cout << "Array array_test[] test#2" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push_front( new array_test[2] );
	auto ret = p_test_obj->pop_front();

	ASSERT_TRUE( std::get<0>( ret ) );

	delete[] std::get<1>( ret );
	delete p_test_obj;

	std::cout << "End Array array_test[] test" << std::endl;
}
