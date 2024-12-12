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
#include <future>
#include <iostream>
#include <random>
#include <thread>

#include "gtest/gtest.h"

#include "alconcurrent/lf_list.hpp"
#include "alconcurrent/lf_mem_alloc.hpp"
#include "alconcurrent/lf_mem_alloc_type.hpp"

constexpr unsigned int   num_thread = 12;   // Tested until 128.
constexpr std::uintptr_t loop_num   = 2000;

using test_list = alpha::concurrent::lockfree_list<std::uintptr_t>;

class lflistHighLoadTest : public ::testing::Test {
protected:
	virtual void SetUp()
	{
		alpha::concurrent::gmem_prune();
	}

	virtual void TearDown()
	{
		// printf( "number of keys of dynamic_tls_key_create(),     %d\n", alpha::concurrent::internal::get_num_of_tls_key() );
		// printf( "max number of keys of dynamic_tls_key_create(), %d\n", alpha::concurrent::internal::get_max_num_of_tls_key() );
	}
};

/**
 * 各スレッドの先頭から追加して、先頭から取り出すことで、カウントアップを繰り返す。
 */
typename test_list::value_type func_test_list_front2front( test_list* p_test_obj, pthread_barrier_t* p_barrier, const std::uintptr_t loop_num_arg )
{

	pthread_barrier_wait( p_barrier );

	typename test_list::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num_arg; i++ ) {
		p_test_obj->push_front( v );
		auto ret = p_test_obj->pop_front();
		if ( !ret.has_value() ) {
			printf( "Bugggggggyyyy  func_test_list_front2front()!!!  %s\n", std::to_string( v ).c_str() );
			printf( "list size count: %zu\n", p_test_obj->get_size() );
			break;
		}
		v = ret.value() + 1;
	}

	return v;
}

/**
 * 各スレッドの先頭から追加して、先頭から取り出すことで、カウントアップを繰り返す。
 */
typename test_list::value_type func_test_list_back2back( test_list* p_test_obj, pthread_barrier_t* p_barrier, const std::uintptr_t loop_num_arg )
{
	pthread_barrier_wait( p_barrier );

	typename test_list::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num_arg; i++ ) {
		p_test_obj->push_back( v );
		auto ret = p_test_obj->pop_back();
		if ( !ret.has_value() ) {
			printf( "Bugggggggyyyy  func_test_list_back2back()!!!  %s\n", std::to_string( v ).c_str() );
			printf( "list size count: %zu\n", p_test_obj->get_size() );
			break;
		}
		v = ret.value() + 1;
	}

	return v;
}

/**
 * 各スレッドの先頭から追加して、最後から取り出すことで、カウントアップを繰り返す。
 */
typename test_list::value_type func_test_list_front2back( test_list* p_test_obj, pthread_barrier_t* p_barrier, const std::uintptr_t loop_num_arg )
{

	pthread_barrier_wait( p_barrier );

	typename test_list::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num_arg; i++ ) {
		p_test_obj->push_front( v );
		auto ret = p_test_obj->pop_back();
		if ( !ret.has_value() ) {
			printf( "Bugggggggyyyy  func_test_list_front2back()!!!  %s\n", std::to_string( v ).c_str() );
			printf( "list size count: %zu\n", p_test_obj->get_size() );
			break;
		}
		v = ret.value() + 1;
	}

	return v;
}

/**
 * 各スレッドの最後から追加して、先頭から取り出すことで、カウントアップを繰り返す。
 */
typename test_list::value_type func_test_list_back2front( test_list* p_test_obj, pthread_barrier_t* p_barrier, const std::uintptr_t loop_num_arg )
{
	pthread_barrier_wait( p_barrier );

	typename test_list::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num_arg; i++ ) {
		p_test_obj->push_back( v );
		auto ret = p_test_obj->pop_front();
		if ( !ret.has_value() ) {
			printf( "Bugggggggyyyy  func_test_list_back2front() by pop_front!!!  %s\n", std::to_string( v ).c_str() );
			printf( "list size count: %zu\n", p_test_obj->get_size() );
			break;
		}

		v = ret.value() + 1;
	}

	return v;
}

/**
 * 各スレッドが、最後から取り出し、合計を計算する。
 */
typename test_list::value_type func_test_list_pop_front( test_list* p_test_obj, pthread_barrier_t* p_barrier, const std::uintptr_t loop_num_arg )
{
	pthread_barrier_wait( p_barrier );

	typename test_list::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num_arg; i++ ) {
		auto ret = p_test_obj->pop_front();
		if ( !ret.has_value() ) {
			printf( "Bugggggggyyyy  func_test_list_pop_front() by pop_back!!!  %s\n", std::to_string( v ).c_str() );
			printf( "list size count: %zu\n", p_test_obj->get_size() );
			break;
		}
		if ( ( ret.value() <= 0 ) || ( ret.value() > ( num_thread * loop_num ) ) ) {
			printf( "Bugggggggyyyy read by func_test_list_pop_front()!!!  %s\n", std::to_string( ret.value() ).c_str() );
			printf( "list size count: %zu\n", p_test_obj->get_size() );
			break;
		}

		v += ret.value();
	}

	return v;
}

/**
 * 各スレッドが、最後から取り出し、合計を計算する。
 */
typename test_list::value_type func_test_list_pop_back( test_list* p_test_obj, pthread_barrier_t* p_barrier, const std::uintptr_t loop_num_arg )
{
	pthread_barrier_wait( p_barrier );

	typename test_list::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num_arg; i++ ) {
		auto ret = p_test_obj->pop_back();
		if ( !ret.has_value() ) {
			printf( "Bugggggggyyyy  func_test_list_pop_back() by pop_back!!!  %s\n", std::to_string( v ).c_str() );
			printf( "list size count: %zu\n", p_test_obj->get_size() );
			break;
		}
		if ( ( ret.value() <= 0 ) || ( ret.value() > ( num_thread * loop_num ) ) ) {
			printf( "Bugggggggyyyy read by func_test_list_pop_back()!!!  %s\n", std::to_string( ret.value() ).c_str() );
			printf( "list size count: %zu\n", p_test_obj->get_size() );
			break;
		}

		v += ret.value();
	}

	return v;
}

/**
 * 各スレッドが、先頭に追加する。
 */
typename test_list::value_type func_test_list_push_front( test_list* p_test_obj, pthread_barrier_t* p_barrier, const std::uintptr_t loop_num_arg )
{
	pthread_barrier_wait( p_barrier );

	typename test_list::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num_arg; i++ ) {
		p_test_obj->push_front( i + 1 );
		v += i + 1;
	}

	return v;
}

/**
 * 各スレッドが、最後に追加する。
 */
typename test_list::value_type func_test_list_push_back( test_list* p_test_obj, pthread_barrier_t* p_barrier, const std::uintptr_t loop_num_arg )
{
	pthread_barrier_wait( p_barrier );

	typename test_list::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num_arg; i++ ) {
		p_test_obj->push_back( i + 1 );
		v += i + 1;
	}

	return v;
}

TEST_F( lflistHighLoadTest, TC0_1_ManyElements_DoPopBack_Then_Empty )
{
	// Arrange
	constexpr unsigned int tmp_num_thread = 12;   // Tested until 128.

	test_list                                   count_list;
	pthread_barrier_t                           barrier;
	std::future<typename test_list::value_type> results_of_threads[tmp_num_thread];

	pthread_barrier_init( &barrier, NULL, tmp_num_thread + 1 );

	for ( auto& cur_future : results_of_threads ) {
		std::packaged_task<typename test_list::value_type( test_list*, pthread_barrier_t*, const std::uintptr_t )> cur_task( func_test_list_pop_back );
		cur_future             = cur_task.get_future();
		std::thread cur_thread = std::thread( std::move( cur_task ), &count_list, &barrier, loop_num );
		cur_thread.detach();
	}

	typename test_list::value_type expect = 0;
	for ( size_t i = 0; i < tmp_num_thread * loop_num; i++ ) {
		count_list.push_front( i + 1 );
		expect += i + 1;
	}

	// Act
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

	typename test_list::value_type sum = 0;
	unsigned int                   i   = 0;
	for ( auto& cur_future : results_of_threads ) {
		uintptr_t e = cur_future.get();
		std::cout << "Thread " << i << ": last func_test_list_pop_back dequeued = " << e << std::endl;
		sum += e;

		i++;
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << tmp_num_thread << "  Exec time: " << diff.count() << " msec" << std::endl;
	std::cout << "Allocated node num:    " << count_list.get_allocated_num() << std::endl;

	// Assert
	// 各スレッドが最後にdequeueした値の合計は tmp_num_thread * num_loop
	// に等しくなるはず。
	EXPECT_EQ( expect, sum ) << "Expect: " << std::to_string( expect ) << std::endl
							 << "Sum:    " << sum << std::endl;
	EXPECT_EQ( count_list.get_size(), 0 );

	if ( count_list.get_size() > 0 ) {
		auto ret = count_list.pop_back();
		std::cout << "remained value:    " << ret.value() << std::endl;
	}

	return;
}

TEST_F( lflistHighLoadTest, TC0_2_ManyElements_DoPopFront_Then_Empty )
{
	// Arrange
	constexpr unsigned int tmp_num_thread = 12;   // Tested until 128.

	test_list                                   count_list;
	pthread_barrier_t                           barrier;
	std::future<typename test_list::value_type> results_of_threads[tmp_num_thread];

	pthread_barrier_init( &barrier, NULL, tmp_num_thread + 1 );

	for ( auto& cur_future : results_of_threads ) {
		std::packaged_task<typename test_list::value_type( test_list*, pthread_barrier_t*, const std::uintptr_t )> cur_task( func_test_list_pop_front );
		cur_future             = cur_task.get_future();
		std::thread cur_thread = std::thread( std::move( cur_task ), &count_list, &barrier, loop_num );
		cur_thread.detach();
	}

	typename test_list::value_type expect = 0;
	for ( size_t i = 0; i < tmp_num_thread * loop_num; i++ ) {
		count_list.push_front( i + 1 );
		expect += i + 1;
	}

	// Act
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

	typename test_list::value_type sum = 0;
	unsigned int                   i   = 0;
	for ( auto& cur_future : results_of_threads ) {
		uintptr_t e = cur_future.get();
		std::cout << "Thread " << i << ": last func_test_list_pop_back dequeued = " << e << std::endl;
		sum += e;

		i++;
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << tmp_num_thread << "  Exec time: " << diff.count() << " msec" << std::endl;
	std::cout << "Allocated node num:    " << count_list.get_allocated_num() << std::endl;

	// Assert
	// 各スレッドが最後にdequeueした値の合計は tmp_num_thread * num_loop
	// に等しくなるはず。
	EXPECT_EQ( expect, sum ) << "Expect: " << std::to_string( expect ) << std::endl
							 << "Sum:    " << sum << std::endl;
	EXPECT_EQ( count_list.get_size(), 0 );

	if ( count_list.get_size() > 0 ) {
		auto ret = count_list.pop_back();
		std::cout << "remained value:    " << ret.value() << std::endl;
	}

	return;
}

TEST_F( lflistHighLoadTest, TC0_3_Empty_DoPushFront_Then_ManyElements )
{
	// Arrange
	constexpr unsigned int tmp_num_thread = 12;   // Tested until 128.

	test_list                                   count_list;
	pthread_barrier_t                           barrier;
	std::future<typename test_list::value_type> results_of_threads[tmp_num_thread];

	pthread_barrier_init( &barrier, NULL, tmp_num_thread + 1 );

	for ( auto& cur_future : results_of_threads ) {
		std::packaged_task<typename test_list::value_type( test_list*, pthread_barrier_t*, const std::uintptr_t )> cur_task( func_test_list_push_front );
		cur_future             = cur_task.get_future();
		std::thread cur_thread = std::thread( std::move( cur_task ), &count_list, &barrier, loop_num );
		cur_thread.detach();
	}

	// Act
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

	typename test_list::value_type expect = 0;
	unsigned int                   idx    = 0;
	for ( auto& cur_future : results_of_threads ) {
		uintptr_t e = cur_future.get();
		std::cout << "Thread " << idx << ": last func_test_list_push_front dequeued = " << e << std::endl;
		expect += e;

		idx++;
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << tmp_num_thread << "  Exec time: " << diff.count() << " msec" << std::endl;

	start_time_point = std::chrono::steady_clock::now();

	uintptr_t sum = 0;
	for ( size_t i = 0; i < tmp_num_thread * loop_num; i++ ) {
		auto ret = count_list.pop_front();
		if ( !ret.has_value() ) {
			printf( "Bugggggggyyyy  TC0_2_Empty_DoPushFront_Then_ManyElements() by pop_back!!!  %s\n", std::to_string( i ).c_str() );
			printf( "list size count: %zu\n", count_list.get_size() );
			break;
		}
		sum += ret.value();
	}

	end_time_point = std::chrono::steady_clock::now();

	diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "Sum Exec time: " << diff.count() << " msec" << std::endl;
	std::cout << "Allocated node num:    " << count_list.get_allocated_num() << std::endl;

	// Assert
	// 各スレッドが最後にdequeueした値の合計は tmp_num_thread * num_loop
	// に等しくなるはず。

	EXPECT_EQ( expect, sum ) << "Expect: " << std::to_string( expect ) << std::endl
							 << "Sum:    " << sum << std::endl;
	EXPECT_EQ( count_list.get_size(), 0 );

	if ( count_list.get_size() > 0 ) {
		auto ret = count_list.pop_back();
		std::cout << "remained value:    " << ret.value() << std::endl;
	}

	return;
}

TEST_F( lflistHighLoadTest, TC0_4_Empty_DoPushBack_Then_ManyElements )
{
	// Arrange
	constexpr unsigned int tmp_num_thread = 12;   // Tested until 128.

	test_list                                   count_list;
	pthread_barrier_t                           barrier;
	std::future<typename test_list::value_type> results_of_threads[tmp_num_thread];

	pthread_barrier_init( &barrier, NULL, tmp_num_thread + 1 );

	for ( auto& cur_future : results_of_threads ) {
		std::packaged_task<typename test_list::value_type( test_list*, pthread_barrier_t*, const std::uintptr_t )> cur_task( func_test_list_push_back );
		cur_future             = cur_task.get_future();
		std::thread cur_thread = std::thread( std::move( cur_task ), &count_list, &barrier, loop_num );
		cur_thread.detach();
	}

	// Act
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

	typename test_list::value_type expect = 0;
	unsigned int                   idx    = 0;
	for ( auto& cur_future : results_of_threads ) {
		uintptr_t e = cur_future.get();
		std::cout << "Thread " << idx << ": last func_test_list_push_front dequeued = " << e << std::endl;
		expect += e;

		idx++;
	}

	uintptr_t sum = 0;
	for ( size_t i = 0; i < tmp_num_thread * loop_num; i++ ) {
		auto ret = count_list.pop_front();
		if ( !ret.has_value() ) {
			printf( "Bugggggggyyyy  TC0_2_Empty_DoPushFront_Then_ManyElements() by pop_back!!!  %s\n", std::to_string( i ).c_str() );
			printf( "list size count: %zu\n", count_list.get_size() );
			break;
		}
		sum += ret.value();
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	// Assert
	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << tmp_num_thread << "  Exec time: " << diff.count() << " msec" << std::endl;
	std::cout << "Allocated node num:    " << count_list.get_allocated_num() << std::endl;

	// 各スレッドが最後にdequeueした値の合計は tmp_num_thread * num_loop
	// に等しくなるはず。
	EXPECT_EQ( expect, sum ) << "Expect: " << std::to_string( expect ) << std::endl
							 << "Sum:    " << sum << std::endl;
	EXPECT_EQ( count_list.get_size(), 0 );

	if ( count_list.get_size() > 0 ) {
		auto ret = count_list.pop_back();
		std::cout << "remained value:    " << ret.value() << std::endl;
	}

	return;
}

TEST_F( lflistHighLoadTest, TC1_1_Empty_DoManyPushFrontPopFront_Then_Empty )
{
	// Arrange
	constexpr unsigned int tmp_num_thread = 24;   // Tested until 128.

	test_list                                   count_list;
	pthread_barrier_t                           barrier;
	std::future<typename test_list::value_type> results_of_threads[tmp_num_thread];

	pthread_barrier_init( &barrier, NULL, tmp_num_thread + 1 );

	for ( auto& cur_future : results_of_threads ) {
		std::packaged_task<typename test_list::value_type( test_list*, pthread_barrier_t*, const std::uintptr_t )> cur_task( func_test_list_front2front );
		cur_future             = cur_task.get_future();
		std::thread cur_thread = std::thread( std::move( cur_task ), &count_list, &barrier, loop_num );
		cur_thread.detach();
	}

	// Act
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

	typename test_list::value_type sum = 0;
	unsigned int                   i   = 0;
	for ( auto& cur_future : results_of_threads ) {
		uintptr_t e = cur_future.get();
		std::cout << "Thread " << i << ": last dequeued = " << e << std::endl;
		sum += e;

		i++;
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << tmp_num_thread << "  Exec time: " << diff.count() << " msec" << std::endl;
	std::cout << "Allocated node num:    " << count_list.get_allocated_num() << std::endl;

	// Assert
	// 各スレッドが最後にdequeueした値の合計は tmp_num_thread * num_loop
	// に等しくなるはず。
	EXPECT_EQ( tmp_num_thread * loop_num, sum ) << "Expect: " << std::to_string( tmp_num_thread * loop_num ) << std::endl
												<< "Sum:    " << sum << std::endl;
	EXPECT_EQ( count_list.get_size(), 0 );

	if ( count_list.get_size() > 0 ) {
		auto ret = count_list.pop_back();
		std::cout << "remained value:    " << ret.value() << std::endl;
	}

	return;
}

TEST_F( lflistHighLoadTest, TC1_2_Empty_DoManyPushBackPopBack_Then_Empty )
{
	// Arrange
	constexpr unsigned int tmp_num_thread = 12;   // Tested until 128.

	test_list                                   count_list;
	pthread_barrier_t                           barrier;
	std::future<typename test_list::value_type> results_of_threads[tmp_num_thread];

	pthread_barrier_init( &barrier, NULL, tmp_num_thread + 1 );

	for ( auto& cur_future : results_of_threads ) {
		std::packaged_task<typename test_list::value_type( test_list*, pthread_barrier_t*, const std::uintptr_t )> cur_task( func_test_list_back2back );
		cur_future             = cur_task.get_future();
		std::thread cur_thread = std::thread( std::move( cur_task ), &count_list, &barrier, loop_num );
		cur_thread.detach();
	}

	// Act
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

	typename test_list::value_type sum = 0;
	unsigned int                   i   = 0;
	for ( auto& cur_future : results_of_threads ) {
		uintptr_t e = cur_future.get();
		std::cout << "Thread " << i << ": last dequeued = " << e << std::endl;
		sum += e;

		i++;
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << tmp_num_thread << "  Exec time: " << diff.count() << " msec" << std::endl;
	std::cout << "Allocated node num:    " << count_list.get_allocated_num() << std::endl;

	// Assert
	// 各スレッドが最後にdequeueした値の合計は tmp_num_thread * num_loop
	// に等しくなるはず。
	EXPECT_EQ( tmp_num_thread * loop_num, sum ) << "Expect: " << std::to_string( tmp_num_thread * loop_num ) << std::endl
												<< "Sum:    " << sum << std::endl;
	EXPECT_EQ( count_list.get_size(), 0 );

	if ( count_list.get_size() > 0 ) {
		auto ret = count_list.pop_back();
		std::cout << "remained value:    " << ret.value() << std::endl;
	}

	return;
}

TEST_F( lflistHighLoadTest, TC1_3_Empty_DoManyPushFrontPopBack_Then_Empty )
{
	constexpr unsigned int tmp_num_thread = 12;   // Tested until 128.

	test_list                                   count_list;
	pthread_barrier_t                           barrier;
	std::future<typename test_list::value_type> results_of_threads[tmp_num_thread];

	pthread_barrier_init( &barrier, NULL, tmp_num_thread + 1 );

	for ( auto& cur_future : results_of_threads ) {
		std::packaged_task<typename test_list::value_type( test_list*, pthread_barrier_t*, const std::uintptr_t )> cur_task( func_test_list_front2back );
		cur_future             = cur_task.get_future();
		std::thread cur_thread = std::thread( std::move( cur_task ), &count_list, &barrier, loop_num );
		cur_thread.detach();
	}

	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

	typename test_list::value_type sum = 0;
	unsigned int                   i   = 0;
	for ( auto& cur_future : results_of_threads ) {
		uintptr_t e = cur_future.get();
		std::cout << "Thread " << i << ": last dequeued = " << e << std::endl;
		sum += e;

		i++;
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << tmp_num_thread << "  Exec time: " << diff.count() << " msec" << std::endl;
	std::cout << "Allocated node num:    " << count_list.get_allocated_num() << std::endl;

	// 各スレッドが最後にdequeueした値の合計は tmp_num_thread * num_loop
	// に等しくなるはず。
	std::cout << "Expect: " << std::to_string( tmp_num_thread * loop_num ) << std::endl;
	std::cout << "Sum:    " << sum << std::endl;

	EXPECT_EQ( tmp_num_thread * loop_num, sum ) << "Expect: " << std::to_string( tmp_num_thread * loop_num ) << std::endl
												<< "Sum:    " << sum << std::endl;
	EXPECT_EQ( count_list.get_size(), 0 );

	if ( count_list.get_size() > 0 ) {
		auto ret = count_list.pop_back();
		std::cout << "remained value:    " << ret.value() << std::endl;
	}

	return;
}

TEST_F( lflistHighLoadTest, TC1_4_Empty_DoManyPushBackPopFront_Then_Empty )
{
	constexpr unsigned int tmp_num_thread = 12;   // Tested until 128.

	test_list                                   count_list;
	pthread_barrier_t                           barrier;
	std::future<typename test_list::value_type> results_of_threads_back2front[tmp_num_thread];

	pthread_barrier_init( &barrier, NULL, tmp_num_thread + 1 );

	for ( auto& cur_future : results_of_threads_back2front ) {
		std::packaged_task<typename test_list::value_type( test_list*, pthread_barrier_t*, const std::uintptr_t )> cur_task( func_test_list_back2front );
		cur_future             = cur_task.get_future();
		std::thread cur_thread = std::thread( std::move( cur_task ), &count_list, &barrier, loop_num );
		cur_thread.detach();
	}

	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

	typename test_list::value_type sum = 0;
	unsigned int                   i   = 0;
	for ( auto& cur_future : results_of_threads_back2front ) {
		uintptr_t e = cur_future.get();
		std::cout << "Thread " << i << ": last back2front dequeued = " << e << std::endl;
		sum += e;

		i++;
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << tmp_num_thread << "  Exec time: " << diff.count() << " msec" << std::endl;
	std::cout << "Allocated node num:    " << count_list.get_allocated_num() << std::endl;

	// 各スレッドが最後にdequeueした値の合計は tmp_num_thread * num_loop
	// に等しくなるはず。
	EXPECT_EQ( tmp_num_thread * loop_num, sum ) << "Expect: " << std::to_string( tmp_num_thread * loop_num ) << std::endl
												<< "Sum:    " << sum << std::endl;
	EXPECT_EQ( count_list.get_size(), 0 );

	return;
}

TEST_F( lflistHighLoadTest, TC1_5_Empty_DoManyCrossPushPopFrontPushPopBack_Then_Empty )
{
	constexpr unsigned int tmp_num_thread = 12;   // Tested until 128.

	test_list                                   count_list;
	pthread_barrier_t                           barrier;
	std::future<typename test_list::value_type> results_of_threads_front2back[tmp_num_thread];
	std::future<typename test_list::value_type> results_of_threads_back2front[tmp_num_thread];

	pthread_barrier_init( &barrier, NULL, tmp_num_thread * 2 + 1 );

	for ( auto& cur_future : results_of_threads_front2back ) {
		std::packaged_task<typename test_list::value_type( test_list*, pthread_barrier_t*, const std::uintptr_t )> cur_task( func_test_list_front2front );
		cur_future             = cur_task.get_future();
		std::thread cur_thread = std::thread( std::move( cur_task ), &count_list, &barrier, loop_num );
		cur_thread.detach();
	}
	for ( auto& cur_future : results_of_threads_back2front ) {
		std::packaged_task<typename test_list::value_type( test_list*, pthread_barrier_t*, const std::uintptr_t )> cur_task( func_test_list_back2back );
		cur_future             = cur_task.get_future();
		std::thread cur_thread = std::thread( std::move( cur_task ), &count_list, &barrier, loop_num );
		cur_thread.detach();
	}

	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

	typename test_list::value_type sum = 0;
	unsigned int                   i   = 0;
	for ( auto& cur_future : results_of_threads_front2back ) {
		uintptr_t e = cur_future.get();
		std::cout << "Thread " << i << ": last front2back dequeued = " << e << std::endl;
		sum += e;

		i++;
	}
	i = 0;
	for ( auto& cur_future : results_of_threads_back2front ) {
		uintptr_t e = cur_future.get();
		std::cout << "Thread " << i << ": last back2front dequeued = " << e << std::endl;
		sum += e;

		i++;
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << tmp_num_thread << "  Exec time: " << diff.count() << " msec" << std::endl;
	std::cout << "Allocated node num:    " << count_list.get_allocated_num() << std::endl;

	// 各スレッドが最後にdequeueした値の合計は tmp_num_thread * num_loop
	// に等しくなるはず。
	EXPECT_EQ( tmp_num_thread * 2 * loop_num, sum ) << "Expect: " << std::to_string( tmp_num_thread * 2 * loop_num ) << std::endl
													<< "Sum:    " << sum << std::endl;
	EXPECT_EQ( count_list.get_size(), 0 );

	if ( count_list.get_size() > 0 ) {
		auto ret = count_list.pop_back();
		std::cout << "remained value:    " << ret.value() << std::endl;
	}

	return;
}

TEST_F( lflistHighLoadTest, TC1_6_Empty_DoManyCrossPushFrontBackPopFrontBack_Then_Empty )
{
	constexpr unsigned int tmp_num_thread = 12;   // Tested until 128.

	test_list                                   count_list;
	pthread_barrier_t                           barrier;
	std::future<typename test_list::value_type> results_of_threads_front2back[tmp_num_thread];
	std::future<typename test_list::value_type> results_of_threads_back2front[tmp_num_thread];

	pthread_barrier_init( &barrier, NULL, tmp_num_thread * 2 + 1 );

	for ( auto& cur_future : results_of_threads_front2back ) {
		std::packaged_task<typename test_list::value_type( test_list*, pthread_barrier_t*, const std::uintptr_t )> cur_task( func_test_list_front2back );
		cur_future             = cur_task.get_future();
		std::thread cur_thread = std::thread( std::move( cur_task ), &count_list, &barrier, loop_num );
		cur_thread.detach();
	}
	for ( auto& cur_future : results_of_threads_back2front ) {
		std::packaged_task<typename test_list::value_type( test_list*, pthread_barrier_t*, const std::uintptr_t )> cur_task( func_test_list_back2front );
		cur_future             = cur_task.get_future();
		std::thread cur_thread = std::thread( std::move( cur_task ), &count_list, &barrier, loop_num );
		cur_thread.detach();
	}

	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

	typename test_list::value_type sum = 0;
	unsigned int                   i   = 0;
	for ( auto& cur_future : results_of_threads_front2back ) {
		uintptr_t e = cur_future.get();
		std::cout << "Thread " << i << ": last front2back dequeued = " << e << std::endl;
		sum += e;

		i++;
	}
	i = 0;
	for ( auto& cur_future : results_of_threads_back2front ) {
		uintptr_t e = cur_future.get();
		std::cout << "Thread " << i << ": last back2front dequeued = " << e << std::endl;
		sum += e;

		i++;
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << tmp_num_thread << "  Exec time: " << diff.count() << " msec" << std::endl;
	std::cout << "Allocated node num:    " << count_list.get_allocated_num() << std::endl;

	// 各スレッドが最後にdequeueした値の合計は tmp_num_thread * num_loop
	// に等しくなるはず。
	EXPECT_EQ( tmp_num_thread * 2 * loop_num, sum ) << "Expect: " << std::to_string( tmp_num_thread * 2 * loop_num ) << std::endl
													<< "Sum:    " << sum << std::endl;
	EXPECT_EQ( count_list.get_size(), 0 );

	if ( count_list.get_size() > 0 ) {
		auto ret = count_list.pop_back();
		std::cout << "remained value:    " << ret.value() << std::endl;
	}

	return;
}

constexpr typename test_list::value_type target_value = 1;
constexpr typename test_list::value_type target_min   = target_value - 1;
constexpr typename test_list::value_type target_max   = target_value + 1;

/**
 * リストのどこかに追加して、どこからか削除する操作を繰り返す。
 */
std::pair<typename test_list::value_type, typename test_list::value_type> func_test_list_insert_remove( test_list* p_test_obj, std::uint_fast32_t seed, pthread_barrier_t* p_barrier, const std::uintptr_t loop_num_arg )
{
	constexpr int num_of_values = 10;
	std::mt19937  engine( seed );

	pthread_barrier_wait( p_barrier );

	typename test_list::value_type expect = 0;
	typename test_list::value_type v      = 0;
	for ( std::uintptr_t i = 0; i < loop_num_arg; i++ ) {
		std::array<typename test_list::value_type, num_of_values> input_data;
		std::array<typename test_list::value_type, num_of_values> insert_place_values;
		std::array<typename test_list::value_type, num_of_values> remove_place_values;
		std::iota( input_data.begin(), input_data.end(), 1 );
		std::iota( insert_place_values.begin(), insert_place_values.end(), 1 );
		std::iota( remove_place_values.begin(), remove_place_values.end(), 1 );
		std::shuffle( input_data.begin(), input_data.end(), engine );
		std::shuffle( insert_place_values.begin(), insert_place_values.end(), engine );
		std::shuffle( remove_place_values.begin(), remove_place_values.end(), engine );

		for ( std::uintptr_t j = 0; j < num_of_values; j++ ) {
			expect += input_data[j];
			typename test_list::value_type target_val = insert_place_values[j];
			p_test_obj->insert(
				[target_val]( std::add_const<typename test_list::value_type>::type& chk ) -> bool {
					return target_val == chk;
				},
				input_data[j] );
		}
		bool is_success = true;
		for ( std::uintptr_t j = 0; j < num_of_values; j++ ) {
			typename test_list::value_type target_val = remove_place_values[j];
			bool                           pop_flag;
			typename test_list::value_type vv;

			// 挿入と削除場所の前後関係次第では、1passスキャンのremove_one_ifでは、拾い損ねる可能性があるため、１０回までリトライする。
			int k = 0;
			do {
				auto local_ret = p_test_obj->remove_one_if(
					[target_val]( std::add_const<typename test_list::value_type>::type& chk ) -> bool {
						return target_val == chk;
					} );
				pop_flag = local_ret.has_value();
				if ( pop_flag ) {
					vv = local_ret.value();
				}
				k++;
			} while ( !pop_flag && ( k < 10 ) );

			if ( !pop_flag ) {
				printf( "Bugggggggyyyy  func_test_list_insert_remove()!!! fail to remove %s\n", std::to_string( v ).c_str() );
				printf( "list size count: %zu\n", p_test_obj->get_size() );
				is_success = false;
				break;
			}
			if ( ( vv <= 0 ) || ( vv > num_of_values ) ) {
				printf( "Bugggggggyyyy  func_test_list_insert_remove() value is out-of-range!!!  %s\n", std::to_string( vv ).c_str() );
				printf( "list size count: %zu\n", p_test_obj->get_size() );
				is_success = false;
				break;
			}
			v += vv;
		}
		if ( !is_success ) {
			break;
		}
	}

	return std::pair<typename test_list::value_type, typename test_list::value_type> { expect, v };
}

TEST_F( lflistHighLoadTest, TC2_Empty_DoManyInsertRemove_Then_Empty )
{
	// Arrange
	constexpr unsigned int                            tmp_num_thread = 12;   // Tested until 128.
	std::random_device                                seed_gen;
	std::mt19937                                      engine( seed_gen() );
	std::uniform_int_distribution<std::uint_fast32_t> dist;
	pthread_barrier_t                                 barrier;

	test_list count_list;

	std::future<std::pair<typename test_list::value_type, typename test_list::value_type>> results_of_threads[tmp_num_thread];

	pthread_barrier_init( &barrier, NULL, tmp_num_thread + 1 );

	for ( auto& cur_future : results_of_threads ) {
		std::packaged_task<
			std::pair<typename test_list::value_type, typename test_list::value_type>(
				test_list*,
				std::uint_fast32_t,
				pthread_barrier_t*,
				const std::uintptr_t )>
			cur_task( func_test_list_insert_remove );
		cur_future             = cur_task.get_future();
		std::thread cur_thread = std::thread( std::move( cur_task ), &count_list, dist( engine ), &barrier, loop_num );
		cur_thread.detach();
	}

	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

	typename test_list::value_type expect = 0;
	typename test_list::value_type sum    = 0;
	for ( auto& cur_future : results_of_threads ) {
		std::pair<typename test_list::value_type, typename test_list::value_type> ret = cur_future.get();
		expect += ret.first;
		sum += ret.second;
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << tmp_num_thread << "  Exec time: " << diff.count() << " msec" << std::endl;
	std::cout << "Allocated nodes:    " << count_list.get_allocated_num() << std::endl;

	// 各スレッドが最後にdequeueした値の合計は num_thread * num_loop
	// に等しくなるはず。
	EXPECT_EQ( expect, sum ) << "Expect: " << std::to_string( expect ) << std::endl
							 << "Sum:    " << sum << std::endl;
	EXPECT_EQ( count_list.get_size(), 0 ) << "nodes:              " << count_list.get_size() << std::endl;

	return;
}

/**
 * 指定データを、loop_num回数、追加する。
 */
typename test_list::value_type func_test_list_push( test_list* p_test_obj, pthread_barrier_t* p_barrier, typename test_list::value_type tc_data, const std::uintptr_t loop_num_arg )
{
	// printf( "func_test_list_push()!!! -> %s\n", std::to_string( tc_data ).c_str() );

	pthread_barrier_wait( p_barrier );

	typename test_list::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num_arg; i++ ) {
		p_test_obj->push_front( tc_data );
		v++;
		if ( ( i % 100 ) == 0 ) {
			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );   // backoff handling
		}
	}

	return v;
}

/**
 * 該当するデータを持つノードをすべて削除する
 */
typename test_list::value_type func_test_list_remove_all( test_list* p_test_obj, pthread_barrier_t* p_barrier, typename test_list::value_type tc_data, const std::uintptr_t loop_num_arg )
{
	test_list::predicate_t search_remove_data = [tc_data]( const test_list::value_type& a ) -> bool {
		return ( tc_data == a );
	};

	// printf( "func_test_list_remove_all()!!! -> %s\n", std::to_string( tc_data ).c_str() );

	pthread_barrier_wait( p_barrier );

	typename test_list::value_type v = 0;
	while ( v < loop_num_arg ) {
		test_list::value_type v_tmp = p_test_obj->remove_all_if( search_remove_data );
		v += v_tmp;
		if ( v_tmp == 0 ) {
			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );   // backoff handling
		}
	}

	return v;
}

TEST_F( lflistHighLoadTest, TC3_Empty_DoPushRemoveAllIf_Then_Empty )
{
	// Arrange
	constexpr unsigned int tmp_num_thread = 12;   // Tested until 128.
	pthread_barrier_t      barrier;

	test_list count_list;

	std::future<typename test_list::value_type> results_of_push_threads[tmp_num_thread];
	std::future<typename test_list::value_type> results_of_remove_all_threads[tmp_num_thread];

	pthread_barrier_init( &barrier, NULL, tmp_num_thread * 2 + 1 );

	typename test_list::value_type tc_data = 0;
	for ( auto& cur_future : results_of_push_threads ) {
		std::packaged_task<
			typename test_list::value_type( test_list*, pthread_barrier_t*, typename test_list::value_type, const std::uintptr_t )>
			cur_task( func_test_list_push );
		cur_future             = cur_task.get_future();
		std::thread cur_thread = std::thread( std::move( cur_task ), &count_list, &barrier, tc_data, loop_num );
		cur_thread.detach();

		tc_data++;
	}
	tc_data = 0;
	for ( auto& cur_future : results_of_remove_all_threads ) {
		std::packaged_task<
			typename test_list::value_type( test_list*, pthread_barrier_t*, typename test_list::value_type, const std::uintptr_t )>
			cur_task( func_test_list_remove_all );
		cur_future             = cur_task.get_future();
		std::thread cur_thread = std::thread( std::move( cur_task ), &count_list, &barrier, tc_data, loop_num );
		cur_thread.detach();

		tc_data++;
	}

	// Act
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &barrier );

	typename test_list::value_type sum = 0;
	int                            idx = 0;
	for ( auto& cur_future : results_of_push_threads ) {
		typename test_list::value_type e = cur_future.get();
		std::cout << "Thread " << idx << ": last func_test_list_push() dequeued = " << e << std::endl;
		sum += e;

		idx++;
	}
	idx = 0;
	for ( auto& cur_future : results_of_remove_all_threads ) {
		typename test_list::value_type e = cur_future.get();
		std::cout << "Thread " << idx << ": last func_test_list_remove_all() dequeued = " << e << std::endl;
		sum += e;

		idx++;
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << tmp_num_thread << "  Exec time: " << diff.count() << " msec" << std::endl;
	std::cout << "Allocated nodes:    " << count_list.get_allocated_num() << std::endl;

	// Assert
	// 各スレッドが最後にdequeueした値の合計は num_thread * num_loop
	// に等しくなるはず。
	EXPECT_EQ( tmp_num_thread * 2 * loop_num, sum ) << "Expect: " << std::to_string( tmp_num_thread * 2 * loop_num ) << std::endl
													<< "Sum:    " << sum << std::endl;
	EXPECT_EQ( count_list.get_size(), 0 ) << "nodes:              " << count_list.get_size() << std::endl;

	return;
}
