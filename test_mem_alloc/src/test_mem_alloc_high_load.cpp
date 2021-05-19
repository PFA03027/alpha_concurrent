/*!
 * @file	test_mem_alloc_high_load.cpp
 * @brief
 * @author	alpha
 * @date	Created on 2021/05/16
 * @details
 *
 * Copyright (C) 2021 by alpha <e-mail address>
 */
#include <pthread.h>

#include <chrono>
#include <iostream>
#include <random>

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

alpha::concurrent::general_mem_allocator test_gma( param, 7 );

pthread_barrier_t barrier;

constexpr int max_slot_size  = 1000;
constexpr int max_alloc_size = 1200;
constexpr int num_loop       = 1200;
constexpr int num_thread     = 10;

void* one_load( void* )
{
	fflush( NULL );
	std::random_device seed_gen;
	std::mt19937       engine( seed_gen() );

	// 0以上9以下の値を等確率で発生させる
	std::uniform_int_distribution<> num_dist( 0, max_slot_size - 1 );
	std::uniform_int_distribution<> size_dist( 1, max_alloc_size );

	void* alloc_addr[max_slot_size];

	pthread_barrier_wait( &barrier );

	for ( int i = 0; i < num_loop; i++ ) {
		int cur_alloc_num = num_dist( engine );
		for ( int j = 0; j < cur_alloc_num; j++ ) {
			alloc_addr[j] = test_gma.allocate( size_dist( engine ) );
		}
		for ( int j = 0; j < cur_alloc_num; j++ ) {
			test_gma.deallocate( alloc_addr[j] );
		}
	}

	return nullptr;
}

void* one_load_malloc_free( void* )
{
	fflush( NULL );
	std::random_device seed_gen;
	std::mt19937       engine( seed_gen() );

	// 0以上9以下の値を等確率で発生させる
	std::uniform_int_distribution<> num_dist( 0, max_slot_size - 1 );
	std::uniform_int_distribution<> size_dist( 1, max_alloc_size );

	void* alloc_addr[max_slot_size];

	pthread_barrier_wait( &barrier );

	for ( int i = 0; i < num_loop; i++ ) {
		int cur_alloc_num = num_dist( engine );
		for ( int j = 0; j < cur_alloc_num; j++ ) {
			alloc_addr[j] = malloc( size_dist( engine ) );
		}
		for ( int j = 0; j < cur_alloc_num; j++ ) {
			free( alloc_addr[j] );
		}
	}

	return nullptr;
}

void load_test( void )
{

	{
		pthread_barrier_init( &barrier, NULL, 1 );
		std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();

		one_load( nullptr );

		std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

		std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
		std::cout << "thread is num_thread"
				  << " one_load() Exec time: " << diff.count() << " msec" << std::endl;
	}

	{
		pthread_barrier_init( &barrier, NULL, 1 );
		std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
		//	pthread_barrier_wait( &barrier );

		one_load_malloc_free( nullptr );

		std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

		std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
		std::cout << "thread is num_thread"
				  << " one_load_malloc_free() Exec time: " << diff.count() << " msec" << std::endl;
	}

	{
		pthread_barrier_init( &barrier, NULL, num_thread + 1 );
		pthread_t* threads = new pthread_t[num_thread];

		void* dummy = nullptr;

		for ( int i = 0; i < num_thread; i++ ) {
			pthread_create( &threads[i], NULL, one_load, &dummy );
		}
		std::cout << "!!!Ready!!!" << std::endl;

		pthread_barrier_wait( &barrier );
		std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
		std::cout << "!!!GO!!!" << std::endl;
		fflush( NULL );

		for ( int i = 0; i < num_thread; i++ ) {
			pthread_join( threads[i], nullptr );
		}

		std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

		std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
		std::cout << "thread is " << num_thread
				  << " one_load() Exec time: " << diff.count() << " msec" << std::endl;

		std::list<alpha::concurrent::chunk_statistics> statistics = test_gma.get_statistics();

		for ( auto& e : statistics ) {
			printf( "chunk conf.size=%d, conf.num=%d, chunk_num: %d, total_slot=%d, free_slot=%d, alloc cnt=%d, alloc err=%d, dealloc cnt=%d, dealloc err=%d\n",
			        (int)e.alloc_conf_.size_of_one_piece_,
			        (int)e.alloc_conf_.num_of_pieces_,
			        (int)e.chunk_num_,
			        (int)e.total_slot_cnt_,
			        (int)e.free_slot_cnt_,
			        (int)e.alloc_req_cnt_,
			        (int)e.error_alloc_req_cnt_,
			        (int)e.dealloc_req_cnt_,
			        (int)e.error_dealloc_req_cnt_ );
		}
	}
	{
		pthread_barrier_init( &barrier, NULL, num_thread + 1 );
		pthread_t* threads = new pthread_t[num_thread];

		void* dummy = nullptr;

		for ( int i = 0; i < num_thread; i++ ) {
			pthread_create( &threads[i], NULL, one_load_malloc_free, &dummy );
		}
		std::cout << "!!!Ready!!!" << std::endl;

		pthread_barrier_wait( &barrier );
		std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
		std::cout << "!!!GO!!!" << std::endl;
		fflush( NULL );

		for ( int i = 0; i < num_thread; i++ ) {
			pthread_join( threads[i], nullptr );
		}

		std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

		std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
		std::cout << "thread is " << num_thread
				  << " one_load_malloc_free() Exec time: " << diff.count() << " msec" << std::endl;
	}
	return;
}
