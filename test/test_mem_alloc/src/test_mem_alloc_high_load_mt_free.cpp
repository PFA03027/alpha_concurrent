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
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <random>

#include "gtest/gtest.h"

#include "alconcurrent/lf_fifo.hpp"
#include "alconcurrent/lf_mem_alloc.hpp"

#include "alconcurrent/internal/alloc_only_allocator.hpp"

#include "mmap_allocator.hpp"

#ifdef PERFORMANCE_ANALYSIS_LOG1
#include "../src/lifo_free_node_stack.hpp"
#endif

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

static pthread_barrier_t global_shared_barrier;

constexpr size_t       max_slot_size           = 1000;
constexpr size_t       max_alloc_size          = 900;
constexpr unsigned int TEST_CONDITION_num_loop = 1000;

using test_fifo_type = alpha::concurrent::fifo_list<void*>;

struct test_params {
	test_fifo_type*                           p_test_obj;
	alpha::concurrent::general_mem_allocator* p_tmg;
	unsigned int                              num_loop;
};

class lfmemAllocFreeBwMultThread : public testing::TestWithParam<unsigned int> {
	// You can implement all the usual fixture class members here.
	// To access the test parameter, call GetParam() from class
	// TestWithParam<T>.
public:
	lfmemAllocFreeBwMultThread( void )
	  : num_thread_( 1 )
	{
		num_thread_ = GetParam();
	}

	void SetUp() override
	{
		int err_cnt, warn_cnt;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &err_cnt, &warn_cnt );

		err_flag.store( false );
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

		EXPECT_FALSE( err_flag.load() );

#ifdef DEBUG_LOG
		alpha::concurrent::internal::print_of_mmap_allocator();
		alpha::concurrent::internal::dynamic_tls_status_info st = alpha::concurrent::internal::dynamic_tls_get_status();
		printf( "num_of_key_array: %u, num_content_head_: %u, next_base_idx_: %u\n", st.num_key_array_cnt_, st.num_content_head_, st.next_base_idx_ );
#endif
	}

	unsigned int num_thread_;
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
	std::uniform_int_distribution<>       num_sleep( 0, 9 );
	std::uniform_int_distribution<>       num_dist( 1, max_slot_size - 1 );
	std::uniform_int_distribution<size_t> size_dist( 1, max_alloc_size );

	pthread_barrier_wait( &global_shared_barrier );

	// printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	for ( unsigned int i = 0; i < p_test_param->num_loop; i++ ) {
		int cur_alloc_num = num_dist( engine );
		for ( int j = 0; j < cur_alloc_num; j++ ) {
			void* p_tmp_alloc_to_push = p_tmg->allocate( size_dist( engine ) );

			p_test_obj->push( p_tmp_alloc_to_push );

			// std::this_thread::sleep_for( std::chrono::milliseconds( num_sleep( engine ) ) );
			void* p_tmp_alloc;
			auto  ret = p_test_obj->pop();
			if ( !ret.has_value() ) {
				printf( "Bugggggggyyyy  func_test_fifo()!!!\n" );
#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
				printf( "fifo size count: %d\n", p_test_obj->get_size() );
#endif

				auto local_ret = p_test_obj->pop();
				if ( !local_ret.has_value() ) {
					printf( "Bugggggggyyyy, then Bugggggggyyyy func_test_fifo()!!!\n" );
#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
					printf( "fifo size count: %d\n", p_test_obj->get_size() );
#endif

					err_flag.store( true );
					return nullptr;
				} else {
					p_tmp_alloc = local_ret.value();
				}
			} else {
				p_tmp_alloc = ret.value();
			}

			p_tmg->deallocate( p_tmp_alloc );
		}
	}
	// printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 20, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );

	return nullptr;
}

void* func_test_fifo_ggmem( void* p_data )
{
	test_params*    p_test_param = reinterpret_cast<test_params*>( p_data );
	test_fifo_type* p_test_obj   = p_test_param->p_test_obj;

	fflush( NULL );
	std::random_device seed_gen;
	std::mt19937       engine( seed_gen() );

	// 0以上9以下の値を等確率で発生させる
	std::uniform_int_distribution<>       num_sleep( 0, 9 );
	std::uniform_int_distribution<>       num_dist( 1, max_slot_size - 1 );
	std::uniform_int_distribution<size_t> size_dist( 1, max_alloc_size );

	pthread_barrier_wait( &global_shared_barrier );

	// printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	for ( unsigned int i = 0; i < p_test_param->num_loop; i++ ) {
		int cur_alloc_num = num_dist( engine );
		for ( int j = 0; j < cur_alloc_num; j++ ) {
			void* p_tmp_alloc_to_push = alpha::concurrent::gmem_allocate( size_dist( engine ) );

			p_test_obj->push( p_tmp_alloc_to_push );

			// std::this_thread::sleep_for( std::chrono::milliseconds( num_sleep( engine ) ) );
			void* p_tmp_alloc;
			auto  ret = p_test_obj->pop();
			if ( !ret.has_value() ) {
				printf( "Bugggggggyyyy  func_test_fifo()!!!\n" );
#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
				printf( "fifo size count: %d\n", p_test_obj->get_size() );
#endif

				auto local_ret = p_test_obj->pop();
				if ( !local_ret.has_value() ) {
					printf( "Bugggggggyyyy, then Bugggggggyyyy func_test_fifo()!!!\n" );
#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
					printf( "fifo size count: %d\n", p_test_obj->get_size() );
#endif

					err_flag.store( true );
					return nullptr;
				} else {
					p_tmp_alloc = local_ret.value();
				}
			} else {
				p_tmp_alloc = ret.value();
			}

			alpha::concurrent::gmem_deallocate( p_tmp_alloc );
		}
	}
	// printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 20, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );

	return nullptr;
}

void load_test_lockfree_bw_mult_thread( unsigned int num_of_thd, alpha::concurrent::general_mem_allocator* p_tmg_arg )
{
	test_fifo_type fifo;

	test_params tda;
	tda.p_test_obj = &fifo;
	tda.p_tmg      = p_tmg_arg;
	tda.num_loop   = TEST_CONDITION_num_loop / num_of_thd;

	pthread_barrier_init( &global_shared_barrier, NULL, num_of_thd + 1 );
	pthread_t* threads = new pthread_t[num_of_thd];

	for ( unsigned int i = 0; i < num_of_thd; i++ ) {
		pthread_create( &threads[i], NULL, func_test_fifo, &tda );
	}
	pthread_barrier_wait( &global_shared_barrier );

#ifdef DEBUG_LOG
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	std::cout << "!!!GO!!!" << std::endl;
	fflush( NULL );
#endif

	for ( unsigned int i = 0; i < num_of_thd; i++ ) {
		pthread_join( threads[i], nullptr );
	}

	EXPECT_TRUE( fifo.is_empty() );
	EXPECT_FALSE( err_flag.load() );

#ifdef DEBUG_LOG
	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << num_of_thd
			  << " func_test_fifo() Exec time: " << diff.count() << " msec" << std::endl;

	auto statistics = p_tmg_arg->get_statistics();
	printf( "Statistics is;\n%s\n", statistics.print().c_str() );
#endif

#ifdef PERFORMANCE_ANALYSIS_LOG1
	printf( "retry/call %zu(%zu / %zu)\n",
	        alpha::concurrent::internal::spin_count_push_to_free_node_stack.load() / alpha::concurrent::internal::call_count_push_to_free_node_stack.load(),
	        alpha::concurrent::internal::spin_count_push_to_free_node_stack.load(),
	        alpha::concurrent::internal::call_count_push_to_free_node_stack.load() );
#endif

	delete[] threads;
}
void load_test_lockfree_bw_mult_thread_ggmem( unsigned int num_of_thd )
{
	test_fifo_type fifo;

	test_params tda;
	tda.p_test_obj = &fifo;
	tda.p_tmg      = nullptr;
	tda.num_loop   = TEST_CONDITION_num_loop / num_of_thd;

	pthread_barrier_init( &global_shared_barrier, NULL, num_of_thd + 1 );
	pthread_t* threads = new pthread_t[num_of_thd];

	for ( unsigned int i = 0; i < num_of_thd; i++ ) {
		pthread_create( &threads[i], NULL, func_test_fifo_ggmem, &tda );
	}
	pthread_barrier_wait( &global_shared_barrier );

#ifdef DEBUG_LOG
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	std::cout << "!!!GO!!!" << std::endl;
	fflush( NULL );
#endif

	for ( unsigned int i = 0; i < num_of_thd; i++ ) {
		pthread_join( threads[i], nullptr );
	}

	EXPECT_TRUE( fifo.is_empty() );
	EXPECT_FALSE( err_flag.load() );

#ifdef DEBUG_LOG
	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << num_of_thd
			  << " func_test_fifo() Exec time: " << diff.count() << " msec" << std::endl;

	auto statistics = alpha::concurrent::gmem_get_statistics();
	printf( "Statistics is;\n%s\n", statistics.print().c_str() );
#endif

#ifdef PERFORMANCE_ANALYSIS_LOG1
	printf( "retry/call %zu(%zu / %zu)\n",
	        alpha::concurrent::internal::spin_count_push_to_free_node_stack.load() / alpha::concurrent::internal::call_count_push_to_free_node_stack.load(),
	        alpha::concurrent::internal::spin_count_push_to_free_node_stack.load(),
	        alpha::concurrent::internal::call_count_push_to_free_node_stack.load() );
#endif

	delete[] threads;
}

void load_test_lockfree_bw_mult_thread_startstop( unsigned int num_of_thd, alpha::concurrent::general_mem_allocator* p_tmg_arg )
{
	unsigned int   start_stop_reqeat = 2;
	test_fifo_type fifo;

	// printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 0, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );

	test_params tda;
	tda.p_test_obj = &fifo;
	tda.p_tmg      = p_tmg_arg;
	tda.num_loop   = TEST_CONDITION_num_loop / start_stop_reqeat / num_of_thd;

#ifdef DEBUG_LOG
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
#endif

	for ( unsigned int j = 0; j < start_stop_reqeat; j++ ) {
		pthread_barrier_init( &global_shared_barrier, NULL, num_of_thd + 1 );
		pthread_t* threads = new pthread_t[num_of_thd];

		for ( unsigned int i = 0; i < num_of_thd; i++ ) {
			pthread_create( &threads[i], NULL, func_test_fifo, &tda );
		}
		pthread_barrier_wait( &global_shared_barrier );

		for ( unsigned int i = 0; i < num_of_thd; i++ ) {
			pthread_join( threads[i], nullptr );
		}
		delete[] threads;

		// printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", j, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	}

	EXPECT_TRUE( fifo.is_empty() );
	EXPECT_FALSE( err_flag.load() );

#ifdef DEBUG_LOG
	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << num_of_thd
			  << " func_test_fifo() Exec time: " << diff.count() << " msec" << std::endl;

	auto statistics = p_tmg_arg->get_statistics();
	printf( "Statistics is;\n" );
	printf( "%s\n", statistics.print().c_str() );
#endif

	return;
}

void prune_thread( std::atomic_bool* p_loop, alpha::concurrent::general_mem_allocator* p_gma_for_prune )
{
	while ( p_loop->load( std::memory_order_acquire ) ) {
		std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );
		p_gma_for_prune->prune();
	}
}

TEST_P( lfmemAllocFreeBwMultThread, DoPruneTC1 )
{
	// printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 90, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	{
		alpha::concurrent::general_mem_allocator test1_gma( param2, 7 );
		{
			std::atomic_bool prune_loop( true );

			std::thread prune_th( prune_thread, &prune_loop, &test1_gma );

			EXPECT_NO_FATAL_FAILURE( load_test_lockfree_bw_mult_thread_startstop( num_thread_, &test1_gma ) );

			prune_loop.store( false, std::memory_order_release );
			prune_th.join();
			std::this_thread::sleep_for( std::chrono::milliseconds( 200 ) );
			// printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 91, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
		}
		// printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 92, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	}
	// printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 93, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
}

// basic allocatorしか使用しない条件
TEST_P( lfmemAllocFreeBwMultThread, TC1 )
{
	alpha::concurrent::general_mem_allocator test1_gma( nullptr, 0 );

	EXPECT_NO_FATAL_FAILURE( load_test_lockfree_bw_mult_thread( num_thread_, &test1_gma ) );
}

// general_mem_allocatorを使用する条件
TEST_P( lfmemAllocFreeBwMultThread, TC2 )
{
	alpha::concurrent::general_mem_allocator test2_gma( param, 7 );

	EXPECT_NO_FATAL_FAILURE( load_test_lockfree_bw_mult_thread( num_thread_, &test2_gma ) );
}

TEST_P( lfmemAllocFreeBwMultThread, TC3 )
{
	EXPECT_NO_FATAL_FAILURE( load_test_lockfree_bw_mult_thread_ggmem( num_thread_ ) );
}

INSTANTIATE_TEST_SUITE_P( many_tls,
                          lfmemAllocFreeBwMultThread,
                          testing::Values( 1, 2, 5, 30 ) );

TEST( lfmemAllocLoad, TC_Unstable_Threads )
{
	const int total_thread_num     = 200;
	const int generated_thread_num = 10;
	const int gmem_max_alloc_size  = 17 * 1024;
	{
		test_fifo_type          fifo;
		int                     exit_count( 0 );
		std::mutex              exit_count_mtx;
		std::condition_variable exit_cv;

		auto thd_functor1 = [&]( int num_loop ) {
			std::random_device                    seed_gen;
			std::mt19937                          engine( seed_gen() );
			std::uniform_int_distribution<size_t> size_dist( 1, gmem_max_alloc_size );

			for ( int i = 0; i < num_loop; i++ ) {
				void* p_tmp_alloc_to_push = alpha::concurrent::gmem_allocate( size_dist( engine ) );

				fifo.push( p_tmp_alloc_to_push );

				// std::this_thread::sleep_for( std::chrono::milliseconds( num_sleep( engine ) ) );
				auto ret = fifo.pop();
				if ( !ret.has_value() ) {
					printf( "Bugggggggyyyy  func_test_fifo()!!!\n" );
#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
					printf( "fifo size count: %d\n", fifo.get_size() );
#endif
					err_flag.store( true );
					break;
				}

				alpha::concurrent::gmem_deallocate( ret.value() );
			}

			{
				std::lock_guard<std::mutex> lg( exit_count_mtx );
				exit_count++;
				exit_cv.notify_one();
			}
		};

		auto thd_functor2 = [&]( int num_loop ) {
			std::random_device                    seed_gen;
			std::mt19937                          engine( seed_gen() );
			std::uniform_int_distribution<size_t> size_dist( 1, gmem_max_alloc_size );

			for ( int i = 0; i < num_loop; i++ ) {
				void* p_tmp_alloc_to_push = alpha::concurrent::gmem_allocate( size_dist( engine ) );

				fifo.push( p_tmp_alloc_to_push );
			}

			for ( int i = 0; i < num_loop; i++ ) {
				auto ret = fifo.pop();
				if ( !ret.has_value() ) {
					printf( "Bugggggggyyyy  func_test_fifo()!!!\n" );
#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
					printf( "fifo size count: %d\n", fifo.get_size() );
#endif
					err_flag.store( true );
					break;
				}

				alpha::concurrent::gmem_deallocate( ret.value() );
			}

			{
				std::lock_guard<std::mutex> lg( exit_count_mtx );
				exit_count++;
				exit_cv.notify_one();
			}
		};

		std::random_device seed_gen;
		std::mt19937       engine( seed_gen() );

		// 0以上9以下の値を等確率で発生させる
		std::uniform_int_distribution<> loop_num( 50, 10000 );

		for ( int i = 0; i < total_thread_num; i++ ) {
			std::thread tt1( thd_functor1, loop_num( engine ) );
			std::thread tt2( thd_functor2, loop_num( engine ) );
			tt1.detach();
			tt2.detach();
			alpha::concurrent::gmem_prune();

			{
				std::unique_lock<std::mutex> lg( exit_count_mtx );
				exit_cv.wait( lg, [&exit_count, i]() { return exit_count > ( i * 2 - generated_thread_num ); } );
			}
		}

		{
			std::unique_lock<std::mutex> lg( exit_count_mtx );
			exit_cv.wait( lg, [&exit_count]() { return exit_count >= ( total_thread_num * 2 ); } );
		}

		EXPECT_TRUE( fifo.is_empty() );
	}

	// alpha::concurrent::gmem_prune();

	{
		int err_cnt, warn_cnt;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &err_cnt, &warn_cnt );
		EXPECT_EQ( err_cnt, 0 );
		EXPECT_EQ( warn_cnt, 0 );
		alpha::concurrent::GetErrorWarningLogCount( &err_cnt, &warn_cnt );
		EXPECT_EQ( err_cnt, 0 );
		EXPECT_EQ( warn_cnt, 0 );

		EXPECT_FALSE( err_flag.load() );

		auto statistics = alpha::concurrent::gmem_get_statistics();

		for ( auto& e : statistics.ch_st_ ) {
			// EXPECT_EQ( 0, e.valid_chunk_num_ );
			EXPECT_EQ( 0, e.consum_cnt_ );
		}
		printf( "gmem Statistics is;\n%s\n", statistics.print().c_str() );
	}
}