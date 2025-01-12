//============================================================================
// Name        : test_msg_content.cpp
// Author      : Teruaki Ata
// Version     :
// Copyright   : Copyright (C) 2020 by Teruaki Ata <PFA03027@nifty.com>
// Description : Hello World in C, Ansi-style
//============================================================================

#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <random>

#include "gtest/gtest.h"

#include "alconcurrent/lf_fifo.hpp"

#include "test_crc32.hpp"

constexpr unsigned int   num_thread = 32;   // Tested until 128.
constexpr std::uintptr_t loop_num   = 100000;

std::atomic<bool> err_flag( false );

constexpr int data_size = 128;

pthread_barrier_t global_shared_barrier;

std::random_device        gen_seed;
thread_local std::mt19937 engine( gen_seed() );

class test_msg_obj {
public:
	test_msg_obj( bool end_flag_arg = false )
	{
		data_[0] = end_flag_arg ? 1 : 0;
		for ( int i = 1; i < data_size; i++ ) {
			data_[i] = engine() % data_size;
		}
		crc_ = test_crc32::crc32( data_, data_size );

		return;
	}

	bool is_end( void )
	{
		switch ( data_[0] ) {
			case 0:
				return false;
				break;
			case 1:
				return true;
				break;
			default:
				printf( "end flag is Error!!!" );
				err_flag.store( true );
				return true;
				break;
		}
	}

	bool chk_crc( void )
	{
		uint32_t crc_x = test_crc32::crc32( data_, data_size );
		return crc_x == crc_;
	}

private:
	unsigned char data_[data_size];
	uint32_t      crc_;
};

using test_fifo_type = alpha::concurrent::fifo_list<test_msg_obj*>;

void* func_test_fifo_producer( void* data )
{
	test_fifo_type* p_test_obj = reinterpret_cast<test_fifo_type*>( data );

	pthread_barrier_wait( &global_shared_barrier );

	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		test_msg_obj* p_one_msg = new test_msg_obj();
		p_test_obj->push( p_one_msg );
	}

	return reinterpret_cast<void*>( 1 );
}

void* func_test_fifo_consumer( void* data )
{
	// 0以上9以下の値を等確率で発生させる
	std::uniform_int_distribution<> dist( 0, 9 );

	test_fifo_type* p_test_obj = reinterpret_cast<test_fifo_type*>( data );

	pthread_barrier_wait( &global_shared_barrier );

	while ( true ) {
		auto ret = p_test_obj->pop();
		if ( !ret.has_value() ) {
			std::this_thread::sleep_for( std::chrono::milliseconds( 1 + dist( engine ) ) );   // backoff handling
			continue;
		}
		if ( ret.value() == nullptr ) {
			printf( "Bugggggggyyyy  func_test_fifo_consumer()!!! nullptr!!\n" );
			printf( "fifo size count: %zu\n", p_test_obj->count_size() );
			err_flag.store( true );
			break;
		}
		if ( !ret.value()->chk_crc() ) {
			printf( "Bugggggggyyyy  func_test_fifo_consumer()!!! CRC NG!!\n" );
			printf( "fifo size count: %zu\n", p_test_obj->count_size() );
			err_flag.store( true );
			break;
		}

		bool end_ret = ret.value()->is_end();
		delete ret.value();
		if ( end_ret ) {
			break;
		}
	}

	printf( "final count of p_test_obj is %zu", p_test_obj->count_size() );

	return reinterpret_cast<void*>( 1 );
}

TEST( MsgContent, TC1 )
{
	test_fifo_type test_obj;

	unsigned int numof_pro = num_thread / 4;
	unsigned int numof_con = num_thread;

	pthread_barrier_init( &global_shared_barrier, NULL, numof_pro + numof_con + 1 );
	pthread_t* producer_threads = new pthread_t[numof_pro];
	pthread_t* consumer_threads = new pthread_t[numof_con];

	for ( unsigned int i = 0; i < numof_pro; i++ ) {
		pthread_create( &producer_threads[i], NULL, func_test_fifo_producer, reinterpret_cast<void*>( &test_obj ) );
	}
	for ( unsigned int i = 0; i < numof_con; i++ ) {
		pthread_create( &consumer_threads[i], NULL, func_test_fifo_consumer, reinterpret_cast<void*>( &test_obj ) );
	}
	std::cout << "!!!Ready!!!" << std::endl;

	std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
	std::cout << "!!!GO!!!" << std::endl;
	std::chrono::steady_clock::time_point start_time_point = std::chrono::steady_clock::now();
	pthread_barrier_wait( &global_shared_barrier );

	for ( unsigned int i = 0; i < numof_pro; i++ ) {
		uintptr_t e;
		pthread_join( producer_threads[i], reinterpret_cast<void**>( &e ) );
	}

	for ( unsigned int i = 0; i < numof_con; i++ ) {
		test_msg_obj* p_one_msg = new test_msg_obj( true );
		test_obj.push( p_one_msg );
	}

	for ( unsigned int i = 0; i < numof_con; i++ ) {
		uintptr_t e;
		pthread_join( consumer_threads[i], reinterpret_cast<void**>( &e ) );
	}

	std::chrono::steady_clock::time_point end_time_point = std::chrono::steady_clock::now();

	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>( end_time_point - start_time_point );
	std::cout << "thread is " << num_thread << "  Exec time: " << diff.count() << " msec" << std::endl;

	delete[] producer_threads;
	delete[] consumer_threads;

	EXPECT_FALSE( err_flag.load() );

	return;
}
