/*!
 * @file	test_mem_alloc_high_load.cpp
 * @brief
 * @author	Teruaki Ata
 * @date	Created on 2021/05/16
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#include <pthread.h>

#include <chrono>
#include <iostream>
#include <random>

#include "gtest/gtest.h"

#include "alconcurrent/lf_mem_alloc.hpp"
#include "alconcurrent/lf_mem_alloc_internal.hpp"

// #define TEST_WITH_SLEEP

static alpha::concurrent::param_chunk_allocation param[] = {
	{ 16, 100 },
	{ 32, 200 },
	{ 64, 200 },
	{ 128, 400 },
	{ 256, 800 },
	{ 512, 1600 },
	{ 1024, 2800 },
};

pthread_barrier_t barrier;

constexpr int max_slot_size  = 1000;
constexpr int max_alloc_size = 900;
constexpr int num_loop       = 200;

void write_task( char* p_write )
{
	*p_write = 10;
}

// chunk_header_multi_slot単体のCPU負荷測定
TEST( lfmemAllocOneChunk, TC_Load )
{
	alpha::concurrent::internal::chunk_list_statistics test_st;

	fflush( NULL );

	void* alloc_addr[max_slot_size];

	static alpha::concurrent::param_chunk_allocation param = { 256, 20 };

	alpha::concurrent::internal::chunk_header_multi_slot chms( param, 0, &test_st );

	//	pthread_barrier_wait( &barrier );

	//	for ( int i = 0; i < num_loop; i++ ) {
	for ( int i = 0; i < 1; i++ ) {
		int cur_alloc_num = 15;
		for ( int j = 0; j < cur_alloc_num; j++ ) {
			alloc_addr[j] = chms.allocate_mem_slot( 256, sizeof( uintptr_t ) );
		}

		for ( int j = 0; j < cur_alloc_num; j++ ) {
			write_task( reinterpret_cast<char*>( alloc_addr[j] ) );
		}

		for ( int j = 0; j < cur_alloc_num; j++ ) {
			ASSERT_TRUE( chms.recycle_mem_slot( alloc_addr[j] ) );
		}
	}

	alpha::concurrent::chunk_statistics e = chms.get_statistics();
	printf( "%s\n", e.print().c_str() );

	printf( "gmem Statistics is;\n" );
	std::list<alpha::concurrent::chunk_statistics> statistics = alpha::concurrent::gmem_get_statistics();
	for ( auto& e : statistics ) {
		EXPECT_EQ( 0, e.consum_cnt_ );
		printf( "%s\n", e.print().c_str() );
	}

	chms.dump();

	{
		int err_cnt, warn_cnt;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &err_cnt, &warn_cnt );
		EXPECT_EQ( err_cnt, 0 );
		EXPECT_EQ( warn_cnt, 0 );
		alpha::concurrent::GetErrorWarningLogCount( &err_cnt, &warn_cnt );
		EXPECT_EQ( err_cnt, 0 );
		EXPECT_EQ( warn_cnt, 0 );
	}

	return;
}

// lf_mem_allocを共有した場合のCPU負荷
void* one_load_lock_free_actual_behavior( void* p_data )
{
	alpha::concurrent::general_mem_allocator* p_gma = reinterpret_cast<alpha::concurrent::general_mem_allocator*>( p_data );

	fflush( NULL );
	std::random_device seed_gen;
	std::mt19937       engine( seed_gen() );

	// 0以上9以下の値を等確率で発生させる
	std::uniform_int_distribution<> num_sleep( 0, 9 );
	std::uniform_int_distribution<> num_dist( 1, 20 );
	std::uniform_int_distribution<> calc_load( 200, 10000 );
	std::uniform_int_distribution<> size_dist( 1, max_alloc_size );

	void* alloc_addr[max_slot_size];

	pthread_barrier_wait( &barrier );

	for ( int i = 0; i < num_loop * ( max_slot_size / 20 ); i++ ) {
		int cur_alloc_num = num_dist( engine );
		for ( int j = 0; j < cur_alloc_num; j++ ) {
			alloc_addr[j] = p_gma->allocate( size_dist( engine ) );
		}

#ifdef TEST_WITH_SLEEP
		std::this_thread::sleep_for( std::chrono::milliseconds( num_sleep( engine ) ) );
#endif

		int cur_calc_load_num = calc_load( engine );
		for ( int j = 0; j < cur_calc_load_num; j++ ) {
			write_task( reinterpret_cast<char*>( alloc_addr[j % cur_alloc_num] ) );
		}

		for ( int j = 0; j < cur_alloc_num; j++ ) {
			p_gma->deallocate( alloc_addr[j] );
		}
	}

	return nullptr;
}

// lf_mem_allocを共有した場合のCPU負荷
void* one_load_empty_actual_behavior( void* p_data )
{

	fflush( NULL );
	std::random_device seed_gen;
	std::mt19937       engine( seed_gen() );

	// 0以上9以下の値を等確率で発生させる
	std::uniform_int_distribution<> num_sleep( 0, 9 );
	std::uniform_int_distribution<> num_dist( 1, 20 );
	std::uniform_int_distribution<> calc_load( 200, 10000 );
	std::uniform_int_distribution<> size_dist( 1, max_alloc_size );
	char                            y;

	pthread_barrier_wait( &barrier );

	for ( int i = 0; i < num_loop * ( max_slot_size / 20 ); i++ ) {
		int cur_alloc_num = num_dist( engine );
		for ( int j = 0; j < cur_alloc_num; j++ ) {
			y = size_dist( engine );
			write_task( &y );
		}

#ifdef TEST_WITH_SLEEP
		std::this_thread::sleep_for( std::chrono::milliseconds( num_sleep( engine ) ) );
#endif

		int cur_calc_load_num = calc_load( engine );
		for ( int j = 0; j < cur_calc_load_num; j++ ) {
			y = j % cur_alloc_num;
			write_task( &y );
		}

		for ( int j = 0; j < cur_alloc_num; j++ ) {
			write_task( &y );
		}
	}

	return nullptr;
}

static alpha::concurrent::param_chunk_allocation param2[] = {
	{ 1024, max_slot_size + 100 },
};

// collisionがない場合のCPU負荷測定
void* one_load_lock_free_min2( void* p_data )
{
	alpha::concurrent::general_mem_allocator* p_gma = reinterpret_cast<alpha::concurrent::general_mem_allocator*>( p_data );

	fflush( NULL );
	std::random_device seed_gen;
	std::mt19937       engine( seed_gen() );

	// 0以上9以下の値を等確率で発生させる
	std::uniform_int_distribution<> num_sleep( 0, 9 );
	std::uniform_int_distribution<> num_dist( 0, max_slot_size - 1 );
	std::uniform_int_distribution<> size_dist( 1, max_alloc_size );

	void* alloc_addr[max_slot_size];

	pthread_barrier_wait( &barrier );

	for ( int i = 0; i < num_loop; i++ ) {
		int cur_alloc_num = num_dist( engine );
		for ( int j = 0; j < cur_alloc_num; j++ ) {
			alloc_addr[j] = p_gma->allocate( size_dist( engine ) );
		}

#ifdef TEST_WITH_SLEEP
		std::this_thread::sleep_for( std::chrono::milliseconds( num_sleep( engine ) ) );
#endif

		for ( int j = 0; j < cur_alloc_num; j++ ) {
			write_task( reinterpret_cast<char*>( alloc_addr[j] ) );
		}

		for ( int j = 0; j < cur_alloc_num; j++ ) {
			p_gma->deallocate( alloc_addr[j] );
		}
	}

	return nullptr;
}

// loop処理部のCPU負荷計測
void* one_load_empty( void* )
{
	fflush( NULL );
	std::random_device seed_gen;
	std::mt19937       engine( seed_gen() );

	// 0以上9以下の値を等確率で発生させる
	std::uniform_int_distribution<> num_sleep( 0, 9 );
	std::uniform_int_distribution<> num_dist( 0, max_slot_size - 1 );
	std::uniform_int_distribution<> size_dist( 1, max_alloc_size );
	char                            y;

	pthread_barrier_wait( &barrier );

	for ( int i = 0; i < num_loop; i++ ) {
		int cur_alloc_num = num_dist( engine );
		for ( int j = 0; j < cur_alloc_num; j++ ) {
			write_task( &y );
		}

#ifdef TEST_WITH_SLEEP
		std::this_thread::sleep_for( std::chrono::milliseconds( num_sleep( engine ) ) );
#endif

		for ( int j = 0; j < cur_alloc_num; j++ ) {
			write_task( &y );
		}

		for ( int j = 0; j < cur_alloc_num; j++ ) {
			write_task( &y );
		}
	}

	return nullptr;
}

void load_test_lockfree( int num_of_thd )
{
	alpha::concurrent::general_mem_allocator test_gma( param, 7 );

	pthread_barrier_init( &barrier, NULL, num_of_thd + 1 );
	pthread_t* threads = new pthread_t[num_of_thd];

	for ( int i = 0; i < num_of_thd; i++ ) {
		pthread_create( &threads[i], NULL, one_load_lock_free_min2, &test_gma );
	}
	std::cout << "!!!Ready!!!" << std::endl;

	pthread_barrier_wait( &barrier );
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	std::cout << "!!!GO!!!" << std::endl;
	fflush( NULL );

	for ( int i = 0; i < num_of_thd; i++ ) {
		pthread_join( threads[i], nullptr );
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << num_of_thd
			  << " one_load_lock_free() Exec time: " << diff.count() << " msec" << std::endl;

	std::list<alpha::concurrent::chunk_statistics> statistics = test_gma.get_statistics();

	for ( auto& e : statistics ) {
		printf( "%s\n", e.print().c_str() );
	}

	delete[] threads;
}

void load_test_lockfree_actual_behavior( int num_of_thd )
{
	alpha::concurrent::general_mem_allocator test_gma( param, 7 );

	pthread_barrier_init( &barrier, NULL, num_of_thd + 1 );
	pthread_t* threads = new pthread_t[num_of_thd];

	for ( int i = 0; i < num_of_thd; i++ ) {
		pthread_create( &threads[i], NULL, one_load_lock_free_actual_behavior, &test_gma );
	}
	std::cout << "!!!Ready!!!" << std::endl;

	pthread_barrier_wait( &barrier );
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	std::cout << "!!!GO!!!" << std::endl;
	fflush( NULL );

	for ( int i = 0; i < num_of_thd; i++ ) {
		pthread_join( threads[i], nullptr );
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << num_of_thd
			  << " one_load_lock_free_actual_behavior() Exec time: " << diff.count() << " msec" << std::endl;

	std::list<alpha::concurrent::chunk_statistics> statistics = test_gma.get_statistics();

	for ( auto& e : statistics ) {
		printf( "%s\n", e.print().c_str() );
	}

	delete[] threads;
}

void load_test_lockfree_min2( int num_of_thd )
{
	pthread_barrier_init( &barrier, NULL, num_of_thd + 1 );
	pthread_t* threads = new pthread_t[num_of_thd];

	std::vector<std::unique_ptr<alpha::concurrent::general_mem_allocator>> free_gma_array( num_of_thd );
	for ( int i = 0; i < num_of_thd; i++ ) {
		free_gma_array[i] = std::make_unique<alpha::concurrent::general_mem_allocator>( param2, 1 );
	}

	for ( int i = 0; i < num_of_thd; i++ ) {
		pthread_create( &threads[i], NULL, one_load_lock_free_min2, free_gma_array[i].get() );
	}
	std::cout << "!!!Ready!!!" << std::endl;

	pthread_barrier_wait( &barrier );
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	std::cout << "!!!GO!!!" << std::endl;
	fflush( NULL );

	for ( int i = 0; i < num_of_thd; i++ ) {
		pthread_join( threads[i], nullptr );
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << num_of_thd
			  << " one_load_lock_free_min2() Exec time: " << diff.count() << " msec" << std::endl;

	for ( int i = 0; i < num_of_thd; i++ ) {
		std::list<alpha::concurrent::chunk_statistics> statistics = free_gma_array[i]->get_statistics();

		for ( auto& e : statistics ) {
			printf( "%s\n", e.print().c_str() );
		}
	}

	for ( int i = 0; i < num_of_thd; i++ ) {
		free_gma_array[i].reset();
	}
	delete[] threads;
}

void load_test_lockfree_min2_actual_behavior( int num_of_thd )
{
	pthread_barrier_init( &barrier, NULL, num_of_thd + 1 );
	pthread_t* threads = new pthread_t[num_of_thd];

	alpha::concurrent::general_mem_allocator* free_gma_array[num_of_thd];
	for ( int i = 0; i < num_of_thd; i++ ) {
		free_gma_array[i] = new alpha::concurrent::general_mem_allocator( param2, 1 );
	}

	for ( int i = 0; i < num_of_thd; i++ ) {
		pthread_create( &threads[i], NULL, one_load_lock_free_actual_behavior, free_gma_array[i] );
	}
	std::cout << "!!!Ready!!!" << std::endl;

	pthread_barrier_wait( &barrier );
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	std::cout << "!!!GO!!!" << std::endl;
	fflush( NULL );

	for ( int i = 0; i < num_of_thd; i++ ) {
		pthread_join( threads[i], nullptr );
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << num_of_thd
			  << " one_load_lock_free_actual_behavior() Exec time: " << diff.count() << " msec" << std::endl;

	for ( int i = 0; i < num_of_thd; i++ ) {
		std::list<alpha::concurrent::chunk_statistics> statistics = free_gma_array[i]->get_statistics();

		for ( auto& e : statistics ) {
			printf( "%s\n", e.print().c_str() );
		}
	}

	for ( int i = 0; i < num_of_thd; i++ ) {
		delete free_gma_array[i];
	}
	delete[] threads;
}

void load_test_empty( int num_of_thd )
{
	pthread_barrier_init( &barrier, NULL, num_of_thd + 1 );
	pthread_t* threads = new pthread_t[num_of_thd];

	void* dummy = nullptr;

	for ( int i = 0; i < num_of_thd; i++ ) {
		pthread_create( &threads[i], NULL, one_load_empty, &dummy );
	}
	std::cout << "!!!Ready!!!" << std::endl;

	pthread_barrier_wait( &barrier );
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	std::cout << "!!!GO!!!" << std::endl;
	fflush( NULL );

	for ( int i = 0; i < num_of_thd; i++ ) {
		pthread_join( threads[i], nullptr );
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << num_of_thd
			  << " load_test_empty() Exec time: " << diff.count() << " msec" << std::endl;

	delete[] threads;
}

void load_test_empty_actual_behavior( int num_of_thd )
{
	pthread_barrier_init( &barrier, NULL, num_of_thd + 1 );
	pthread_t* threads = new pthread_t[num_of_thd];

	void* dummy = nullptr;

	for ( int i = 0; i < num_of_thd; i++ ) {
		pthread_create( &threads[i], NULL, one_load_empty_actual_behavior, &dummy );
	}
	std::cout << "!!!Ready!!!" << std::endl;

	pthread_barrier_wait( &barrier );
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	std::cout << "!!!GO!!!" << std::endl;
	fflush( NULL );

	for ( int i = 0; i < num_of_thd; i++ ) {
		pthread_join( threads[i], nullptr );
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << num_of_thd
			  << " one_load_empty_actual_behavior() Exec time: " << diff.count() << " msec" << std::endl;

	delete[] threads;
}

// malloc/freeの場合のCPU負荷計測
void load_test_malloc_free( int num_of_thd )
{
	alpha::concurrent::general_mem_allocator test_gma( nullptr, 0 );

	pthread_barrier_init( &barrier, NULL, num_of_thd + 1 );
	pthread_t* threads = new pthread_t[num_of_thd];

	for ( int i = 0; i < num_of_thd; i++ ) {
		pthread_create( &threads[i], NULL, one_load_lock_free_min2, &test_gma );
	}
	std::cout << "!!!Ready!!!" << std::endl;

	pthread_barrier_wait( &barrier );
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	std::cout << "!!!GO!!!" << std::endl;
	fflush( NULL );

	for ( int i = 0; i < num_of_thd; i++ ) {
		pthread_join( threads[i], nullptr );
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << num_of_thd
			  << " load_test_malloc_free() Exec time: " << diff.count() << " msec" << std::endl;

	delete[] threads;
}

// malloc/freeの場合のCPU負荷計測
void load_test_malloc_free_actual_behavior( int num_of_thd )
{
	alpha::concurrent::general_mem_allocator test_gma( nullptr, 0 );

	pthread_barrier_init( &barrier, NULL, num_of_thd + 1 );
	pthread_t* threads = new pthread_t[num_of_thd];

	for ( int i = 0; i < num_of_thd; i++ ) {
		pthread_create( &threads[i], NULL, one_load_lock_free_actual_behavior, &test_gma );
	}
	std::cout << "!!!Ready!!!" << std::endl;

	pthread_barrier_wait( &barrier );
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	std::cout << "!!!GO!!!" << std::endl;
	fflush( NULL );

	for ( int i = 0; i < num_of_thd; i++ ) {
		pthread_join( threads[i], nullptr );
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << num_of_thd
			  << " load_test_malloc_free_actual_behavior() Exec time: " << diff.count() << " msec" << std::endl;

	delete[] threads;
}

class lfmemAllocLoadTest : public testing::TestWithParam<int> {
	// You can implement all the usual fixture class members here.
	// To access the test parameter, call GetParam() from class
	// TestWithParam<T>.
public:
	lfmemAllocLoadTest( void )
	  : num_thread_( 1 )
	{
		num_thread_ = GetParam();
	}

	void SetUp() override
	{
		int err_cnt, warn_cnt;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &err_cnt, &warn_cnt );
		alpha::concurrent::gmem_prune();
	}
	void TearDown() override
	{
		int err_cnt, warn_cnt;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &err_cnt, &warn_cnt );
		EXPECT_EQ( err_cnt, 0 );
		EXPECT_EQ( warn_cnt, 0 );
		alpha::concurrent::GetErrorWarningLogCount( &err_cnt, &warn_cnt );
		EXPECT_EQ( err_cnt, 0 );
		EXPECT_EQ( warn_cnt, 0 );
	}

	int num_thread_;
};

TEST_P( lfmemAllocLoadTest, load_test_empty )
{
	load_test_empty( num_thread_ );

	return;
}

TEST_P( lfmemAllocLoadTest, load_test_malloc_free )
{
	load_test_malloc_free( num_thread_ );

	return;
}

TEST_P( lfmemAllocLoadTest, load_test_lockfree_min2 )
{
	load_test_lockfree_min2( num_thread_ );

	return;
}

TEST_P( lfmemAllocLoadTest, load_test_lockfree )
{
	load_test_lockfree( num_thread_ );

	return;
}

TEST_P( lfmemAllocLoadTest, load_test_empty_actual_behavior )
{
	load_test_empty_actual_behavior( num_thread_ );

	return;
}

TEST_P( lfmemAllocLoadTest, load_test_malloc_free_actual_behavior )
{
	load_test_malloc_free_actual_behavior( num_thread_ );

	return;
}

TEST_P( lfmemAllocLoadTest, load_test_lockfree_actual_behavior )
{
	load_test_lockfree_actual_behavior( num_thread_ );

	return;
}

INSTANTIATE_TEST_SUITE_P( various_threads,
                          lfmemAllocLoadTest,
                          testing::Values( 1, 20 ) );
