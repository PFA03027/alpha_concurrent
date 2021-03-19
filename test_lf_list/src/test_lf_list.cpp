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

#include "lf_list.hpp"

constexpr int            num_thread = 64;   // Tested until 128.
constexpr std::uintptr_t loop_num   = 100000;

using test_list = alpha::concurrent::lockfree_list<std::uintptr_t>;

pthread_barrier_t barrier;

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
			printf( "Bugggggggyyyy  func_test_list_front2back()!!!  %llu\n", v );
			printf( "list size count: %d\n", p_test_obj->get_size() );
			exit( 1 );
		}
		auto [pop_flag, vv] = p_test_obj->pop_back();
		if ( !pop_flag ) {
			printf( "Bugggggggyyyy  func_test_list_front2back()!!!  %llu\n", v );
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
			printf( "Bugggggggyyyy  func_test_list_back2front()!!!  %llu\n", v );
			printf( "list size count: %d\n", p_test_obj->get_size() );
			exit( 1 );
		}
		auto [pop_flag, vv] = p_test_obj->pop_front();
		if ( !pop_flag ) {
			printf( "Bugggggggyyyy  func_test_list_back2front()!!!  %llu\n", v );
			printf( "list size count: %d\n", p_test_obj->get_size() );
			exit( 1 );
		}
		v = vv + 1;
	}

	return reinterpret_cast<void*>( v );
}

int test1( void )
{
	test_list count_list;

	pthread_barrier_init( &barrier, NULL, num_thread * 2 + 1 );
	pthread_t* threads = new pthread_t[num_thread * 2];

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[i], NULL, func_test_list_front2back, reinterpret_cast<void*>( &count_list ) );
	}

	for ( int i = 0; i < num_thread; i++ ) {
		pthread_create( &threads[num_thread + i], NULL, func_test_list_back2front, reinterpret_cast<void*>( &count_list ) );
	}

	std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

	int sum = 0;
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
	if ( sum == ( num_thread * 2 * loop_num ) ) {
		std::cout << "OK!" << std::endl;
	} else {
		std::cout << "NGGGGGGgggggg!" << std::endl;
		return EXIT_FAILURE;
	}

	delete[] threads;

	std::cout << "Allocated nodes:    " << count_list.get_allocated_num() << std::endl;

	return EXIT_SUCCESS;
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
			auto [ins_chk_ret, ins_allc_ret] = p_test_obj->insert( target_value, search_insert_pos );
			if ( ins_chk_ret ) break;
			if ( ins_allc_ret ) {
				// フリーノードストレージからの管理ノードアロケーションに成功しながらも、挿入位置を見つけられなかったことを示す。
				// テスト条件として、これは起きてはならないため、エラー終了する。
				printf( "Bugggggggyyyy  func_test_list_insert_remove()!!!  %llu\n", i );
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

int test2( void )
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

	int sum = 0;
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
	if ( sum == ( num_thread * loop_num ) ) {
		std::cout << "OK!" << std::endl;
	} else {
		std::cout << "NGGGGGGgggggg!" << std::endl;
		return EXIT_FAILURE;
	}

	delete[] threads;

	std::cout << "nodes:              " << count_list.get_size() << std::endl;
	std::cout << "Allocated nodes:    " << count_list.get_allocated_num() << std::endl;

	return EXIT_SUCCESS;
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

	printf( "func_test_list_push()!!! -> %llu\n", p_tc->tc_data );

	pthread_barrier_wait( &barrier );

	typename test_list::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		if ( !p_test_obj->push_front( p_tc->tc_data ) ) {
			printf( "Bugggggggyyyy  func_test_list_push()!!!  %llu\n", v );
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

	printf( "func_test_list_remove_all()!!! -> %llu\n", p_tc->tc_data );

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

int test3( void )
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

	std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

	int sum = 0;
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
	if ( sum == ( num_thread * 2 * loop_num ) ) {
		std::cout << "OK!" << std::endl;
	} else {
		std::cout << "NGGGGGGgggggg!" << std::endl;
		return EXIT_FAILURE;
	}

	delete[] threads;

	std::cout << "nodes:              " << count_list.get_size() << std::endl;
	std::cout << "Allocated nodes:    " << count_list.get_allocated_num() << std::endl;

	return EXIT_SUCCESS;
}

int test4( void )
{
	test_list count_list;

	for ( std::uintptr_t i = 0; i <= loop_num; i++ ) {
		if ( !count_list.push_front( i ) ) {
			printf( "Bugggggggyyyy  func_test_list_push()!!!  %llu\n", i );
			printf( "list size count: %d\n", count_list.get_size() );
			return EXIT_FAILURE;
		}
	}

	std::uintptr_t sum = 0;
	count_list.for_each( [&sum]( auto& ref_value ) {
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
	if ( sum == expect ) {
		std::cout << "OK!" << std::endl;
	} else {
		std::cout << "NGGGGGGgggggg!" << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int main( void )
{
	std::cout << "!!!Start World!!!" << std::endl;   // prints !!!Hello World!!!

	test1();
	test2();
	test3();
	test4();

	std::cout << "!!!End World!!!" << std::endl;   // prints !!!Hello World!!!
	return EXIT_SUCCESS;
}
