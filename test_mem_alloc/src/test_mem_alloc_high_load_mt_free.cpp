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

static pthread_barrier_t barrier;

constexpr int max_slot_size  = 1000;
constexpr int max_alloc_size = 900;
constexpr int num_loop       = 1200;
constexpr int num_thread     = 10;

struct test_deleter {

	static alpha::concurrent::general_mem_allocator* p_tmg;

	void operator()( void* p_mem )
	{
		p_tmg->deallocate( p_mem );
		return;
	}
};
alpha::concurrent::general_mem_allocator* test_deleter::p_tmg;

using test_fifo_type = alpha::concurrent::fifo_list<void*, true, test_deleter>;

struct test_params {
	test_fifo_type*                           p_test_obj;
	alpha::concurrent::general_mem_allocator* p_tmg;
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

	for ( int i = 0; i < num_loop; i++ ) {
		int cur_alloc_num = num_dist( engine );
		for ( int j = 0; j < cur_alloc_num; j++ ) {
			void* p_tmp_alloc = p_tmg->allocate( size_dist( engine ) );

			p_test_obj->push( p_tmp_alloc );
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
			auto [pop_flag, p_tmp_alloc] = p_test_obj->pop();
#else
			auto local_ret = p_test_obj->pop();
			auto pop_flag  = std::get<0>( local_ret );
			p_tmp_alloc    = std::get<1>( local_ret );
#endif
			if ( !pop_flag ) {
				printf( "Bugggggggyyyy  func_test_fifo()!!!\n" );
				printf( "fifo size count: %d\n", p_test_obj->get_size() );
				exit( 1 );
			}

			p_tmg->deallocate( p_tmp_alloc );
		}
	}

	return nullptr;
}

void load_test_lockfree_bw_mult_thread( int num_of_thd, alpha::concurrent::general_mem_allocator* p_tmg_arg )
{
	test_deleter::p_tmg = p_tmg_arg;

	test_fifo_type fifo;

	test_params tda;
	tda.p_test_obj = &fifo;
	tda.p_tmg      = p_tmg_arg;

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

	std::list<alpha::concurrent::chunk_statistics> statistics = p_tmg_arg->get_statistics();

	for ( auto& e : statistics ) {
		printf( "chunk conf.size=%d, conf.num=%d, chunk_num: %d, total_slot=%d, free_slot=%d, alloc cnt=%d, alloc err=%d, dealloc cnt=%d, dealloc err=%d, alloc_collision=%d, dealloc_collision=%d\n",
		        (int)e.alloc_conf_.size_of_one_piece_,
		        (int)e.alloc_conf_.num_of_pieces_,
		        (int)e.chunk_num_,
		        (int)e.total_slot_cnt_,
		        (int)e.free_slot_cnt_,
		        (int)e.alloc_req_cnt_,
		        (int)e.error_alloc_req_cnt_,
		        (int)e.dealloc_req_cnt_,
		        (int)e.error_dealloc_req_cnt_,
		        (int)e.alloc_collision_cnt_,
		        (int)e.dealloc_collision_cnt_ );
	}
}

void load_test_alloc_free_bw_mult_thread( void )
{
	alpha::concurrent::general_mem_allocator test1_gma( nullptr, 0 );
	alpha::concurrent::general_mem_allocator test2_gma( param, 7 );

	load_test_lockfree_bw_mult_thread( num_thread, &test1_gma );
	load_test_lockfree_bw_mult_thread( num_thread, &test2_gma );
}
