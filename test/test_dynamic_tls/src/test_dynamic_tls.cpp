/**
 * @file test_dynamic_tls.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief Test for dynamic thread local storage
 * @version 0.1
 * @date 2022-12-30
 *
 * @copyright Copyright (c) 2022, Teruaki Ata <PFA03027@nifty.com>
 *
 */

#include <pthread.h>

#include <cstdint>
#include <deque>
#include <iostream>
#include <list>
#include <mutex>
#include <random>
#include <thread>

#include "gtest/gtest.h"

#include "alconcurrent/dynamic_tls.hpp"

void nothing_to_do( void* p_arg )
{
	return;
}

TEST( dynamic_tls, TC_create_release )
{
	alpha::concurrent::internal::dynamic_tls_key_t key = alpha::concurrent::internal::dynamic_tls_key_create( nothing_to_do );

	ASSERT_NE( nullptr, key );
	EXPECT_EQ( 1, alpha::concurrent::internal::get_num_of_tls_key() );

	alpha::concurrent::internal::dynamic_tls_key_release( key );
	EXPECT_EQ( 0, alpha::concurrent::internal::get_num_of_tls_key() );

	return;
}

TEST( dynamic_tls, TC_set_get )
{
	alpha::concurrent::internal::dynamic_tls_key_t key = alpha::concurrent::internal::dynamic_tls_key_create( nothing_to_do );

	ASSERT_NE( nullptr, key );
	EXPECT_EQ( 1, alpha::concurrent::internal::get_num_of_tls_key() );

	alpha::concurrent::internal::dynamic_tls_setspecific( key, reinterpret_cast<void*>( 1 ) );

	auto data = alpha::concurrent::internal::dynamic_tls_getspecific( key );
	EXPECT_EQ( reinterpret_cast<void*>( 1 ), data );

	alpha::concurrent::internal::dynamic_tls_key_release( key );
	EXPECT_EQ( 0, alpha::concurrent::internal::get_num_of_tls_key() );

	return;
}

class dynamic_tls_many_tls : public testing::TestWithParam<int> {
	// You can implement all the usual fixture class members here.
	// To access the test parameter, call GetParam() from class
	// TestWithParam<T>.
public:
	dynamic_tls_many_tls( void )
	  : max_num_( 1 )
	{
		max_num_ = GetParam();
	}

	void SetUp() override
	{
	}
	void TearDown() override
	{
	}

	int max_num_;
};

TEST_P( dynamic_tls_many_tls, TC_many_number_set_get )
{
	alpha::concurrent::internal::dynamic_tls_key_t keys[max_num_];

	for ( auto& e_key : keys ) {
		e_key = alpha::concurrent::internal::dynamic_tls_key_create( nothing_to_do );
		ASSERT_NE( nullptr, e_key );
	}

	EXPECT_EQ( max_num_, alpha::concurrent::internal::get_num_of_tls_key() );

	for ( int i = 0; i < max_num_; i++ ) {
		alpha::concurrent::internal::dynamic_tls_setspecific( keys[i], reinterpret_cast<void*>( i + 1 ) );
	}

	for ( int i = 0; i < max_num_; i++ ) {
		auto data = alpha::concurrent::internal::dynamic_tls_getspecific( keys[i] );
		EXPECT_EQ( reinterpret_cast<void*>( i + 1 ), data );
	}

	for ( auto& e_key : keys ) {
		alpha::concurrent::internal::dynamic_tls_key_release( e_key );
	}
	EXPECT_EQ( 0, alpha::concurrent::internal::get_num_of_tls_key() );

	return;
}

INSTANTIATE_TEST_SUITE_P( many_tls,
                          dynamic_tls_many_tls,
                          testing::Values( 1, 2,
                                           alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE - 1, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE + 1,
                                           alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 2 - 1, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 2, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 2 + 1,
                                           alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 3 - 1, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 3, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 3 + 1 ) );

#define THREAD_COUNT ( 1000 )

class dynamic_tls_many_thd_many : public testing::TestWithParam<int> {
	// You can implement all the usual fixture class members here.
	// To access the test parameter, call GetParam() from class
	// TestWithParam<T>.
public:
	dynamic_tls_many_thd_many( void )
	  : max_num_( 1 )
	  , err_cnt_( 0 )
	  , p_keys_array_( nullptr )
	{
		max_num_      = GetParam();
		p_keys_array_ = new alpha::concurrent::internal::dynamic_tls_key_t[max_num_];
	}

	~dynamic_tls_many_thd_many()
	{
		delete[] p_keys_array_;
	}

	void SetUp() override
	{
		pthread_barrier_init( &barrier_, NULL, THREAD_COUNT + 1 );

		for ( int i = 0; i < max_num_; i++ ) {
			p_keys_array_[i] = alpha::concurrent::internal::dynamic_tls_key_create( nothing_to_do );
			ASSERT_NE( nullptr, p_keys_array_[i] );
		}
		EXPECT_EQ( max_num_, alpha::concurrent::internal::get_num_of_tls_key() );
	}
	void TearDown() override
	{
		for ( int i = 0; i < max_num_; i++ ) {
			alpha::concurrent::internal::dynamic_tls_key_release( p_keys_array_[i] );
		}
		EXPECT_EQ( 0, alpha::concurrent::internal::get_num_of_tls_key() );
		pthread_barrier_destroy( &barrier_ );
	}

	int                                             max_num_;
	std::atomic<int>                                err_cnt_;
	alpha::concurrent::internal::dynamic_tls_key_t* p_keys_array_;
	pthread_barrier_t                               barrier_;

	static void thread_func( dynamic_tls_many_thd_many* p_test_fixture )
	{
		for ( int i = 0; i < p_test_fixture->max_num_; i++ ) {
			alpha::concurrent::internal::dynamic_tls_setspecific( p_test_fixture->p_keys_array_[i], reinterpret_cast<void*>( i + 1 ) );
		}

		pthread_barrier_wait( &( p_test_fixture->barrier_ ) );

		for ( int i = 0; i < p_test_fixture->max_num_; i++ ) {
			auto data = alpha::concurrent::internal::dynamic_tls_getspecific( p_test_fixture->p_keys_array_[i] );
			if ( reinterpret_cast<void*>( i + 1 ) != data ) {
				p_test_fixture->err_cnt_.fetch_add( 1 );
			}
		}
	}
};

TEST_P( dynamic_tls_many_thd_many, TC_many_number_set_get )
{
	std::thread tt[THREAD_COUNT];

	for ( auto& e : tt ) {
		e = std::thread( thread_func, this );
	}

	pthread_barrier_wait( &barrier_ );

	for ( int i = 0; i < max_num_; i++ ) {
		alpha::concurrent::internal::dynamic_tls_setspecific( p_keys_array_[i], reinterpret_cast<void*>( i + 1 ) );
	}

	for ( int i = 0; i < max_num_; i++ ) {
		auto data = alpha::concurrent::internal::dynamic_tls_getspecific( p_keys_array_[i] );
		EXPECT_EQ( reinterpret_cast<void*>( i + 1 ), data );
	}

	for ( auto& e : tt ) {
		e.join();
	}

	EXPECT_EQ( 0, err_cnt_.load() );

	alpha::concurrent::internal::dynamic_tls_status_info st = alpha::concurrent::internal::dynamic_tls_get_status();
	printf( "num_content_head_: %u, next_base_idx_: %u\n", st.num_content_head_, st.next_base_idx_ );

	return;
}

INSTANTIATE_TEST_SUITE_P( many_tls,
                          dynamic_tls_many_thd_many,
                          testing::Values( 1, 2,
                                           alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE - 1, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE + 1,
                                           alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 2 - 1, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 2, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 2 + 1,
                                           alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 3 - 1, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 3, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 3 + 1,
                                           alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 3 - 1, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 3, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 3 + 1 ) );
