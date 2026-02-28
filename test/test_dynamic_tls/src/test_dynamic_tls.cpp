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

#include "alloc_only_allocator.hpp"
#include "mmap_allocator.hpp"

struct keep_argument_value {
	keep_argument_value( void )
	  : param_of_allocate_( nullptr )
	  , data_of_deallocate_( 0 )
	  , param_of_deallocate_( nullptr )
	{
	}

	void*     param_of_allocate_;
	uintptr_t data_of_deallocate_;
	void*     param_of_deallocate_;

	static uintptr_t alloc_handler( void* p_arg )
	{
		keep_argument_value* p_keeper = reinterpret_cast<keep_argument_value*>( p_arg );
		p_keeper->param_of_allocate_  = p_arg;
		return 1U;
	}
	static void dealloc_handler( uintptr_t data, void* p_arg )
	{
		keep_argument_value* p_keeper  = reinterpret_cast<keep_argument_value*>( p_arg );
		p_keeper->data_of_deallocate_  = data;
		p_keeper->param_of_deallocate_ = p_arg;
	}
};

uintptr_t nothing_to_allocate( void* p_arg )
{
	return 0U;
}

void nothing_to_deallocate( uintptr_t data, void* p_arg )
{
	return;
}

TEST( dynamic_tls, TC_create_release )
{
	// Arrange
	keep_argument_value test_data;

	// Act
	alpha::concurrent::internal::dynamic_tls_key_t key = alpha::concurrent::internal::dynamic_tls_key_create(
		&test_data,
		keep_argument_value::alloc_handler,
		keep_argument_value::dealloc_handler );

	// Assert
	ASSERT_NE( nullptr, key );
	EXPECT_EQ( 1, alpha::concurrent::internal::get_num_of_tls_key() );

	alpha::concurrent::internal::dynamic_tls_key_release( key );
	EXPECT_EQ( 0, alpha::concurrent::internal::get_num_of_tls_key() );

	EXPECT_EQ( test_data.param_of_allocate_, nullptr );
	EXPECT_EQ( test_data.data_of_deallocate_, 0U );
	EXPECT_EQ( test_data.param_of_deallocate_, nullptr );

	return;
}

TEST( dynamic_tls, TC_release_with_nullptr )
{
	// Arrange

	// Act
	alpha::concurrent::internal::dynamic_tls_key_release( nullptr );

	// Assert

	return;
}

TEST( dynamic_tls, TC_create_double_release )
{
	// Arrange
	keep_argument_value                            test_data;
	alpha::concurrent::internal::dynamic_tls_key_t key = alpha::concurrent::internal::dynamic_tls_key_create(
		&test_data,
		keep_argument_value::alloc_handler,
		keep_argument_value::dealloc_handler );
	ASSERT_NE( nullptr, key );
	EXPECT_EQ( 1, alpha::concurrent::internal::get_num_of_tls_key() );
	alpha::concurrent::internal::dynamic_tls_key_release( key );
	EXPECT_EQ( 0, alpha::concurrent::internal::get_num_of_tls_key() );
	EXPECT_EQ( test_data.param_of_allocate_, nullptr );
	EXPECT_EQ( test_data.data_of_deallocate_, 0U );
	EXPECT_EQ( test_data.param_of_deallocate_, nullptr );

	// Act
	alpha::concurrent::internal::dynamic_tls_key_release( key );

	// Assert
	// should be no crash

	return;
}

TEST( dynamic_tls, TC_create_get_release )
{
	// Arrange
	keep_argument_value                            test_data;
	alpha::concurrent::internal::dynamic_tls_key_t key = alpha::concurrent::internal::dynamic_tls_key_create(
		&test_data,
		keep_argument_value::alloc_handler,
		keep_argument_value::dealloc_handler );
	ASSERT_NE( nullptr, key );

	// Act
	auto data = alpha::concurrent::internal::dynamic_tls_getspecific( key );
	EXPECT_EQ( alpha::concurrent::internal::op_ret::SUCCESS, data.stat_ );
	EXPECT_EQ( 1, data.p_data_ );
	alpha::concurrent::internal::dynamic_tls_key_release( key );

	// Assert
	EXPECT_EQ( test_data.param_of_allocate_, reinterpret_cast<void*>( &test_data ) );
	EXPECT_EQ( test_data.data_of_deallocate_, 1U );
	EXPECT_EQ( test_data.param_of_deallocate_, reinterpret_cast<void*>( &test_data ) );

	return;
}

TEST( dynamic_tls, TC_get_with_nullptr )
{
	// Arrange

	// Act
	auto data = alpha::concurrent::internal::dynamic_tls_getspecific( nullptr );

	// Assert
	EXPECT_NE( alpha::concurrent::internal::op_ret::SUCCESS, data.stat_ );
	EXPECT_EQ( alpha::concurrent::internal::op_ret::INVALID_KEY, data.stat_ );

	return;
}

TEST( dynamic_tls, TC_set_with_nullptr )
{
	// Arrange

	// Act
	auto ret = alpha::concurrent::internal::dynamic_tls_setspecific( nullptr, 1 );

	// Assert
	EXPECT_EQ( alpha::concurrent::internal::op_ret::INVALID_KEY, ret );

	return;
}

TEST( dynamic_tls, TC_set_get )
{
	alpha::concurrent::internal::dynamic_tls_key_t key = alpha::concurrent::internal::dynamic_tls_key_create( nullptr, nothing_to_allocate, nothing_to_deallocate );

	ASSERT_NE( nullptr, key );
	EXPECT_EQ( 1, alpha::concurrent::internal::get_num_of_tls_key() );

	alpha::concurrent::internal::dynamic_tls_setspecific( key, 1 );

	auto data = alpha::concurrent::internal::dynamic_tls_getspecific( key );
	EXPECT_EQ( alpha::concurrent::internal::op_ret::SUCCESS, data.stat_ );
	EXPECT_EQ( 1, data.p_data_ );

	alpha::concurrent::internal::dynamic_tls_key_release( key );
	EXPECT_EQ( 0, alpha::concurrent::internal::get_num_of_tls_key() );

	return;
}

class dynamic_tls_many_tls : public testing::TestWithParam<size_t> {
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
		alpha::concurrent::internal::print_of_mmap_allocator();
	}

	size_t max_num_;
};

TEST_P( dynamic_tls_many_tls, TC_many_number_set_get )
{
	alpha::concurrent::internal::dynamic_tls_key_t keys[max_num_];

	for ( auto& e_key : keys ) {
		e_key = alpha::concurrent::internal::dynamic_tls_key_create( nullptr, nothing_to_allocate, nothing_to_deallocate );
		ASSERT_NE( nullptr, e_key );
	}

	EXPECT_EQ( max_num_, alpha::concurrent::internal::get_num_of_tls_key() );

	for ( unsigned int i = 0; i < max_num_; i++ ) {
		alpha::concurrent::internal::dynamic_tls_setspecific( keys[i], i + 1 );
	}

	for ( unsigned int i = 0; i < max_num_; i++ ) {
		auto data = alpha::concurrent::internal::dynamic_tls_getspecific( keys[i] );
		EXPECT_EQ( alpha::concurrent::internal::op_ret::SUCCESS, data.stat_ );
		EXPECT_EQ( i + 1, data.p_data_ );
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

#define THREAD_COUNT ( 100 )

class dynamic_tls_many_thd_many : public testing::TestWithParam<size_t> {
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

		for ( size_t i = 0; i < max_num_; i++ ) {
			p_keys_array_[i] = alpha::concurrent::internal::dynamic_tls_key_create( nullptr, nothing_to_allocate, nothing_to_deallocate );
			ASSERT_NE( nullptr, p_keys_array_[i] );
		}
		EXPECT_EQ( max_num_, alpha::concurrent::internal::get_num_of_tls_key() );
	}
	void TearDown() override
	{
		for ( size_t i = 0; i < max_num_; i++ ) {
			alpha::concurrent::internal::dynamic_tls_key_release( p_keys_array_[i] );
		}
		EXPECT_EQ( 0, alpha::concurrent::internal::get_num_of_tls_key() );
		pthread_barrier_destroy( &barrier_ );

		alpha::concurrent::internal::print_of_mmap_allocator();
	}

	size_t                                          max_num_;
	std::atomic<int>                                err_cnt_;
	alpha::concurrent::internal::dynamic_tls_key_t* p_keys_array_;
	pthread_barrier_t                               barrier_;

	static void thread_func( dynamic_tls_many_thd_many* p_test_fixture )
	{
		for ( size_t i = 0; i < p_test_fixture->max_num_; i++ ) {
			alpha::concurrent::internal::dynamic_tls_setspecific( p_test_fixture->p_keys_array_[i], i + 1 );
		}

		pthread_barrier_wait( &( p_test_fixture->barrier_ ) );

		for ( size_t i = 0; i < p_test_fixture->max_num_; i++ ) {
			auto data = alpha::concurrent::internal::dynamic_tls_getspecific( p_test_fixture->p_keys_array_[i] );
			if ( static_cast<uintptr_t>( i + 1 ) != data.p_data_ ) {
				p_test_fixture->err_cnt_.fetch_add( 1, std::memory_order_acq_rel );
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

	for ( size_t i = 0; i < max_num_; i++ ) {
		alpha::concurrent::internal::dynamic_tls_setspecific( p_keys_array_[i], i + 1 );
	}

	for ( size_t i = 0; i < max_num_; i++ ) {
		auto data = alpha::concurrent::internal::dynamic_tls_getspecific( p_keys_array_[i] );
		EXPECT_EQ( alpha::concurrent::internal::op_ret::SUCCESS, data.stat_ );
		EXPECT_EQ( ( i + 1 ), data.p_data_ );
	}

	for ( auto& e : tt ) {
		e.join();
	}

	EXPECT_EQ( 0, err_cnt_.load() );

	alpha::concurrent::internal::dynamic_tls_status_info st = alpha::concurrent::internal::dynamic_tls_get_status();
	printf( "num_of_key_array: %u, num_content_head_: %u, next_base_idx_: %u\n", st.num_key_array_cnt_, st.num_content_head_, st.next_base_idx_ );

	return;
}

INSTANTIATE_TEST_SUITE_P( many_tls,
                          dynamic_tls_many_thd_many,
                          testing::Values( 1, 2,
                                           alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE - 1, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE + 1,
                                           alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 2 - 1, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 2, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 2 + 1,
                                           alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 3 - 1, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 3, alpha::concurrent::internal::ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE * 3 + 1 ) );
