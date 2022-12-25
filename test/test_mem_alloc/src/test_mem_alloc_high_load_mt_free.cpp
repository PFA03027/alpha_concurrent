/*!
 * @file	test_mem_alloc_high_load_mt_free.cpp
 * @brief
 * @author	Teruaki Ata
 * @date	Created on 2021/05/29
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#include <pthread.h>

#include <chrono>
#include <iostream>
#include <random>

#include "gtest/gtest.h"

#include "alconcurrent/lf_fifo.hpp"
#include "alconcurrent/lf_mem_alloc.hpp"

static alpha::concurrent::param_chunk_allocation param[] = {
	{ 16, 100 },
	{ 32, 200 },
	{ 64, 200 },
	{ 128, 400 },
	{ 256, 800 },
	{ 512, 1600 },
	{ 1024, 2800 },
};

static alpha::concurrent::param_chunk_allocation param2[] = {
	{ 16, 2 },
	{ 32, 2 },
	{ 64, 2 },
	{ 128, 2 },
	{ 256, 2 },
	{ 512, 2 },
	{ 1024, 2 },
};

std::atomic<bool> err_flag( false );

static pthread_barrier_t barrier;

constexpr int max_slot_size  = 1000;
constexpr int max_alloc_size = 900;
constexpr int num_loop       = 1200;
constexpr int num_thread     = 10;

using test_fifo_type = alpha::concurrent::fifo_list<void*, true, false>;

struct test_params {
	test_fifo_type*                           p_test_obj;
	alpha::concurrent::general_mem_allocator* p_tmg;
	int                                       num_loop;
};

/**
 * 各スレッドのメインルーチン。
 * カウントアップを繰り返す。
 */
void* func_test_fifo( void* p_data )
{
	test_params*                              p_test_param = reinterpret_cast<test_params*>( p_data );
	test_fifo_type*                           p_test_obj   = p_test_param->p_test_obj;
	alpha::concurrent::general_mem_allocator* p_tmg        = p_test_param->p_tmg;

	fflush( NULL );
	std::random_device seed_gen;
	std::mt19937       engine( seed_gen() );

	// 0以上9以下の値を等確率で発生させる
	std::uniform_int_distribution<> num_sleep( 0, 9 );
	std::uniform_int_distribution<> num_dist( 1, max_slot_size - 1 );
	std::uniform_int_distribution<> size_dist( 1, max_alloc_size );

	pthread_barrier_wait( &barrier );

	printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	for ( int i = 0; i < p_test_param->num_loop; i++ ) {
		int cur_alloc_num = num_dist( engine );
		for ( int j = 0; j < cur_alloc_num; j++ ) {
			void* p_tmp_alloc_to_push = p_tmg->allocate( size_dist( engine ) );

			p_test_obj->push( p_tmp_alloc_to_push );

			// std::this_thread::sleep_for( std::chrono::milliseconds( num_sleep( engine ) ) );

#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
			auto [pop_flag, p_tmp_alloc] = p_test_obj->pop();
#else
			auto local_ret   = p_test_obj->pop();
			auto pop_flag    = std::get<0>( local_ret );
			auto p_tmp_alloc = std::get<1>( local_ret );
#endif
			if ( !pop_flag ) {
				printf( "Bugggggggyyyy  func_test_fifo()!!!\n" );
				printf( "fifo size count: %d\n", p_test_obj->get_size() );
				err_flag.store( true );
				return nullptr;
			}

			p_tmg->deallocate( p_tmp_alloc );
		}
	}
	printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 20, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );

	return nullptr;
}

void load_test_lockfree_bw_mult_thread( int num_of_thd, alpha::concurrent::general_mem_allocator* p_tmg_arg )
{
	test_fifo_type fifo;

	test_params tda;
	tda.p_test_obj = &fifo;
	tda.p_tmg      = p_tmg_arg;
	tda.num_loop   = num_loop;

	pthread_barrier_init( &barrier, NULL, num_of_thd + 1 );
	pthread_t* threads = new pthread_t[num_of_thd];

	for ( int i = 0; i < num_of_thd; i++ ) {
		pthread_create( &threads[i], NULL, func_test_fifo, &tda );
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
			  << " func_test_fifo() Exec time: " << diff.count() << " msec" << std::endl;

	EXPECT_FALSE( err_flag.load() );

	std::list<alpha::concurrent::chunk_statistics> statistics = p_tmg_arg->get_statistics();

	printf( "Statistics is;\n" );
	for ( auto& e : statistics ) {
		printf( "%s\n", e.print().c_str() );
	}

	delete[] threads;
}

void load_test_lockfree_bw_mult_thread_startstop( int num_of_thd, alpha::concurrent::general_mem_allocator* p_tmg_arg )
{
	int            start_stop_reqeat = 2;
	test_fifo_type fifo;

	printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 0, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );

	test_params tda;
	tda.p_test_obj = &fifo;
	tda.p_tmg      = p_tmg_arg;
	tda.num_loop   = num_loop / start_stop_reqeat;

	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();

	for ( int j = 0; j < start_stop_reqeat; j++ ) {
		pthread_barrier_init( &barrier, NULL, num_of_thd + 1 );
		pthread_t* threads = new pthread_t[num_of_thd];

		for ( int i = 0; i < num_of_thd; i++ ) {
			pthread_create( &threads[i], NULL, func_test_fifo, &tda );
		}
		pthread_barrier_wait( &barrier );

		for ( int i = 0; i < num_of_thd; i++ ) {
			pthread_join( threads[i], nullptr );
		}
		delete[] threads;

		printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", j, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << num_of_thd
			  << " func_test_fifo() Exec time: " << diff.count() << " msec" << std::endl;

	EXPECT_FALSE( err_flag.load() );

	std::list<alpha::concurrent::chunk_statistics> statistics = p_tmg_arg->get_statistics();

	printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 1, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	printf( "Statistics is;\n" );
	for ( auto& e : statistics ) {
		printf( "%s\n", e.print().c_str() );
	}

	return;
}

void prune_thread( std::atomic_bool* p_loop, alpha::concurrent::general_mem_allocator* p_gma_for_prune )
{
	while ( p_loop->load( std::memory_order_acquire ) ) {
		std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );
		p_gma_for_prune->prune();
	}
}

TEST( lfmemAlloc_prune, TestAllocFreeBwMultThread1 )
{
	printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 90, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	{
		alpha::concurrent::general_mem_allocator test1_gma( param2, 7 );
		{
			std::atomic_bool prune_loop( true );

			std::thread prune_th( prune_thread, &prune_loop, &test1_gma );

			err_flag.store( false );
			EXPECT_NO_FATAL_FAILURE( load_test_lockfree_bw_mult_thread_startstop( num_thread, &test1_gma ) );

			prune_loop.store( false, std::memory_order_release );
			prune_th.join();
			std::this_thread::sleep_for( std::chrono::milliseconds( 200 ) );
			printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 91, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
		}
		printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 92, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	}
	printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 93, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	{
		int err_cnt, warn_cnt;
		alpha::concurrent::GetErrorWarningLogCount( &err_cnt, &warn_cnt );
		EXPECT_EQ( err_cnt, 0 );
		EXPECT_EQ( warn_cnt, 0 );
		alpha::concurrent::GetErrorWarningLogCountAndReset( &err_cnt, &warn_cnt );
		EXPECT_EQ( err_cnt, 0 );
		EXPECT_EQ( warn_cnt, 0 );
	}
}

TEST( lfmemAlloc, TestAllocFreeBwMultThread1 )
{
	alpha::concurrent::general_mem_allocator test1_gma( nullptr, 0 );

	err_flag.store( false );
	EXPECT_NO_FATAL_FAILURE( load_test_lockfree_bw_mult_thread( num_thread, &test1_gma ) );

	{
		int err_cnt, warn_cnt;
		alpha::concurrent::GetErrorWarningLogCount( &err_cnt, &warn_cnt );
		EXPECT_EQ( err_cnt, 0 );
		EXPECT_EQ( warn_cnt, 0 );
		alpha::concurrent::GetErrorWarningLogCountAndReset( &err_cnt, &warn_cnt );
		EXPECT_EQ( err_cnt, 0 );
		EXPECT_EQ( warn_cnt, 0 );
	}
}

TEST( lfmemAlloc, TestAllocFreeBwMultThread2 )
{
	alpha::concurrent::general_mem_allocator test2_gma( param, 7 );

	err_flag.store( false );
	EXPECT_NO_FATAL_FAILURE( load_test_lockfree_bw_mult_thread( num_thread, &test2_gma ) );

	{
		int err_cnt, warn_cnt;
		alpha::concurrent::GetErrorWarningLogCount( &err_cnt, &warn_cnt );
		EXPECT_EQ( err_cnt, 0 );
		EXPECT_EQ( warn_cnt, 0 );
		alpha::concurrent::GetErrorWarningLogCountAndReset( &err_cnt, &warn_cnt );
		EXPECT_EQ( err_cnt, 0 );
		EXPECT_EQ( warn_cnt, 0 );
	}
}
