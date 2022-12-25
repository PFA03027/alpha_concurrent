//============================================================================
// Name        : test_mem_alloc.cpp
// Author      : Teruaki Ata
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C, Ansi-style
//============================================================================

#include <stdio.h>
#include <stdlib.h>

#include <cstddef>
#include <cstdint>
#include <iostream>

#include "gtest/gtest.h"

#include "../src/lf_mem_alloc_internal.hpp"
#include "alconcurrent/lf_mem_alloc.hpp"
#include "alconcurrent/lf_mem_alloc_type.hpp"

alpha::concurrent::param_chunk_allocation param = { 27, 2 };

TEST( lfmemAlloc, TestChunkHeaderMultiSlot )
{
	alpha::concurrent::internal::chunk_list_statistics    test_st;
	alpha::concurrent::internal::chunk_header_multi_slot* p_chms = new alpha::concurrent::internal::chunk_header_multi_slot( param, 0, &test_st );

	void* test_ptr1 = p_chms->allocate_mem_slot( ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG );
	void* test_ptr2 = p_chms->allocate_mem_slot( ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG );
	void* test_ptr3 = p_chms->allocate_mem_slot( ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG );

	EXPECT_NE( nullptr, test_ptr1 );
	EXPECT_NE( nullptr, test_ptr2 );
	EXPECT_EQ( nullptr, test_ptr3 );

	EXPECT_FALSE( p_chms->recycle_mem_slot( test_ptr3, ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG ) );
	EXPECT_FALSE( p_chms->recycle_mem_slot( reinterpret_cast<void*>( test_ptr1 + 1 ), ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG ) );

	EXPECT_TRUE( p_chms->recycle_mem_slot( test_ptr1, ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG ) );
	EXPECT_TRUE( p_chms->recycle_mem_slot( test_ptr2, ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG ) );

	alpha::concurrent::chunk_statistics e = p_chms->get_statistics();

	printf( "%s\n", e.print().c_str() );

	printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	delete p_chms;
	printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );

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

TEST( lfmemAlloc, TestChunkHeaderMultiSlot_MT_one_by_one )
{
	unsigned int                                          test_threads = 100;
	alpha::concurrent::param_chunk_allocation             param        = { 27, 2 * test_threads };
	alpha::concurrent::internal::chunk_list_statistics    test_st;
	alpha::concurrent::internal::chunk_header_multi_slot* p_chms = new alpha::concurrent::internal::chunk_header_multi_slot( param, 0, &test_st );

	for ( unsigned int i = 0; i < test_threads; i++ ) {
		std::thread tt( [p_chms]() {
			void* test_ptr1 = p_chms->allocate_mem_slot( ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG );
			void* test_ptr2 = p_chms->allocate_mem_slot( ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG );

			EXPECT_NE( nullptr, test_ptr1 );
			EXPECT_NE( nullptr, test_ptr2 );

			EXPECT_TRUE( p_chms->recycle_mem_slot( test_ptr1, ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG ) );
			EXPECT_TRUE( p_chms->recycle_mem_slot( test_ptr2, ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG ) );
		} );
		tt.join();
	}

	alpha::concurrent::chunk_statistics e = p_chms->get_statistics();

	printf( "%s\n", e.print().c_str() );

	printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	delete p_chms;
	printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );

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

TEST( lfmemAlloc, TestChunkHeaderMultiSlot_MT_at_same_time )
{
	unsigned int                                          test_threads = 100;
	alpha::concurrent::param_chunk_allocation             param        = { 27, 2 * test_threads };
	alpha::concurrent::internal::chunk_list_statistics    test_st;
	alpha::concurrent::internal::chunk_header_multi_slot* p_chms = new alpha::concurrent::internal::chunk_header_multi_slot( param, 0, &test_st );
	std::thread                                           tt[test_threads];
	pthread_barrier_t                                     barrier;
	pthread_barrier_init( &barrier, NULL, test_threads + 1 );

	for ( unsigned int i = 0; i < test_threads; i++ ) {
		tt[i] = std::thread( [p_chms, &barrier]() {
			pthread_barrier_wait( &barrier );
			void* test_ptr1 = p_chms->allocate_mem_slot( ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG );
			void* test_ptr2 = p_chms->allocate_mem_slot( ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG );

			EXPECT_NE( nullptr, test_ptr1 );
			EXPECT_NE( nullptr, test_ptr2 );

			std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );

			EXPECT_TRUE( p_chms->recycle_mem_slot( test_ptr1, ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG ) );
			EXPECT_TRUE( p_chms->recycle_mem_slot( test_ptr2, ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG ) );
		} );
	}
	pthread_barrier_wait( &barrier );
	for ( unsigned int i = 0; i < test_threads; i++ ) {
		tt[i].join();
	}
	pthread_barrier_destroy( &barrier );

	alpha::concurrent::chunk_statistics e = p_chms->get_statistics();

	printf( "%s\n", e.print().c_str() );

	printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	delete p_chms;
	printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );

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

TEST( lfmemAlloc, TestChunkList_AdditionalAlloc )
{
	// max slot数２に対し、３つ目のスロットを要求した場合のテスト
	alpha::concurrent::internal::chunk_list* p_ch_lst = new alpha::concurrent::internal::chunk_list( param );

	void* test_ptr1 = p_ch_lst->allocate_mem_slot( ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG );
	void* test_ptr2 = p_ch_lst->allocate_mem_slot( ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG );
	void* test_ptr3 = p_ch_lst->allocate_mem_slot( ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG );

	EXPECT_NE( nullptr, test_ptr1 );
	EXPECT_NE( nullptr, test_ptr2 );
	EXPECT_NE( nullptr, test_ptr3 );

	EXPECT_TRUE( p_ch_lst->recycle_mem_slot( test_ptr3, ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG ) );
	EXPECT_TRUE( p_ch_lst->recycle_mem_slot( test_ptr1, ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG ) );
	EXPECT_TRUE( p_ch_lst->recycle_mem_slot( test_ptr2, ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG ) );

	alpha::concurrent::chunk_statistics e = p_ch_lst->get_statistics();

	printf( "%s\n", e.print().c_str() );

	printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	delete p_ch_lst;
	printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );

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

TEST( lfmemAlloc, TestChunkList_IllegalAddressFree )
{
	// max slot数２に対し、３つ目のスロットを要求した場合のテスト
	alpha::concurrent::internal::chunk_list* p_ch_lst = new alpha::concurrent::internal::chunk_list( param );

	void* test_ptr1 = p_ch_lst->allocate_mem_slot( ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG );
	void* test_ptr2 = p_ch_lst->allocate_mem_slot( ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG );
	void* test_ptr3 = p_ch_lst->allocate_mem_slot( ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG );

	EXPECT_NE( nullptr, test_ptr1 );
	EXPECT_NE( nullptr, test_ptr2 );
	EXPECT_NE( nullptr, test_ptr3 );

	EXPECT_FALSE( p_ch_lst->recycle_mem_slot( reinterpret_cast<void*>( test_ptr3 + 1 ), ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG ) );
	EXPECT_FALSE( p_ch_lst->recycle_mem_slot( reinterpret_cast<void*>( test_ptr1 + 1 ), ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG ) );

	EXPECT_FALSE( p_ch_lst->recycle_mem_slot( reinterpret_cast<void*>( test_ptr2 + 1 ), ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG ) );

	alpha::concurrent::chunk_statistics e = p_ch_lst->get_statistics();

	printf( "%s\n", e.print().c_str() );

	delete p_ch_lst;

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

TEST( lfmemAlloc, TestGeneralMemAllocator )
{
	alpha::concurrent::param_chunk_allocation param[] = {
		{ 27, 2 },
		{ 100, 2 },
	};

	alpha::concurrent::general_mem_allocator* p_mem_allocator = new alpha::concurrent::general_mem_allocator( param, 2 );

	void* test_ptr1 = p_mem_allocator->allocate( 10 );
	void* test_ptr2 = p_mem_allocator->allocate( 100 );
	void* test_ptr3 = p_mem_allocator->allocate( 1000 );

	EXPECT_NE( nullptr, test_ptr1 );
	EXPECT_NE( nullptr, test_ptr2 );
	EXPECT_NE( nullptr, test_ptr3 );

	p_mem_allocator->deallocate( test_ptr3 );
	p_mem_allocator->deallocate( test_ptr1 );
	p_mem_allocator->deallocate( test_ptr2 );

	delete p_mem_allocator;

	printf( "number of keys of pthread_key_create(),     %d\n", alpha::concurrent::internal::get_num_of_tls_key() );
	printf( "max number of keys of pthread_key_create(), %d\n", alpha::concurrent::internal::get_max_num_of_tls_key() );

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

TEST( lfmemAlloc, TestGeneralMemAllocator_prune )
{
	alpha::concurrent::param_chunk_allocation param[] = {
		{ 27, 2 },
		{ 100, 2 },
	};

	alpha::concurrent::general_mem_allocator* p_mem_allocator = new alpha::concurrent::general_mem_allocator( param, 2 );

	void* test_ptr1 = p_mem_allocator->allocate( 10 );
	void* test_ptr2 = p_mem_allocator->allocate( 10 );
	void* test_ptr3 = p_mem_allocator->allocate( 10 );

	EXPECT_NE( nullptr, test_ptr1 );
	EXPECT_NE( nullptr, test_ptr2 );
	EXPECT_NE( nullptr, test_ptr3 );

	p_mem_allocator->deallocate( test_ptr3 );
	p_mem_allocator->deallocate( test_ptr1 );
	p_mem_allocator->deallocate( test_ptr2 );

	auto ret_st = p_mem_allocator->get_statistics();
	printf( "before prune\n" );
	for ( auto& e : ret_st ) {
		auto result_str = e.print();
		printf( "%s\n", result_str.c_str() );
	}

	p_mem_allocator->prune();

	ret_st = p_mem_allocator->get_statistics();
	printf( "after prune\n" );
	for ( auto& e : ret_st ) {
		auto result_str = e.print();
		printf( "%s\n", result_str.c_str() );
	}

	test_ptr1       = p_mem_allocator->allocate( 10 );
	test_ptr2       = p_mem_allocator->allocate( 10 );
	test_ptr3       = p_mem_allocator->allocate( 10 );
	void* test_ptr4 = p_mem_allocator->allocate( 10 );
	void* test_ptr5 = p_mem_allocator->allocate( 10 );

	ret_st = p_mem_allocator->get_statistics();
	printf( "after prune\n" );
	for ( auto& e : ret_st ) {
		auto result_str = e.print();
		printf( "%s\n", result_str.c_str() );
	}

	p_mem_allocator->deallocate( test_ptr3 );
	p_mem_allocator->deallocate( test_ptr1 );
	p_mem_allocator->deallocate( test_ptr2 );
	p_mem_allocator->deallocate( test_ptr4 );
	p_mem_allocator->deallocate( test_ptr5 );

	delete p_mem_allocator;

	printf( "number of keys of pthread_key_create(),     %d\n", alpha::concurrent::internal::get_num_of_tls_key() );
	printf( "max number of keys of pthread_key_create(), %d\n", alpha::concurrent::internal::get_max_num_of_tls_key() );

	{
		int err_cnt, warn_cnt;
		alpha::concurrent::GetErrorWarningLogCount( &err_cnt, &warn_cnt );
		EXPECT_EQ( err_cnt, 0 );
		EXPECT_EQ( warn_cnt, 0 );
		alpha::concurrent::GetErrorWarningLogCountAndReset( &err_cnt, &warn_cnt );
		EXPECT_EQ( err_cnt, 0 );
		EXPECT_EQ( warn_cnt, 0 );
	}

	return;
}

#define GM_ALIGN_SIZE ( alignof( std::max_align_t ) )
#define RQ_SIZE       ( GM_ALIGN_SIZE + 1 )

TEST( lfmemAlloc, TestGMemAllocator )
{
	std::size_t rq_size = RQ_SIZE;
	for ( int i = 1; i < 13; i++ ) {
		void* test_ptr1 = alpha::concurrent::gmem_allocate( rq_size );
		EXPECT_NE( nullptr, test_ptr1 ) << std::to_string( i ) << ": request size: " << std::to_string( rq_size ) << std::endl;

		// GM_ALIGN_SIZEでメモリがアライメントされていることを確認する
		std::uintptr_t chk_ptr_align = reinterpret_cast<std::uintptr_t>( test_ptr1 );
		EXPECT_EQ( chk_ptr_align % GM_ALIGN_SIZE, 0 ) << std::to_string( i ) << ": request size: " << std::to_string( rq_size ) << std::endl;

		alpha::concurrent::gmem_deallocate( test_ptr1 );

		rq_size *= 2;
	}

	printf( "number of keys of pthread_key_create(),     %d\n", alpha::concurrent::internal::get_num_of_tls_key() );
	printf( "max number of keys of pthread_key_create(), %d\n", alpha::concurrent::internal::get_max_num_of_tls_key() );

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

TEST( lfmemAlloc, PlatformCheck )
{
	EXPECT_TRUE( alpha::concurrent::test_platform_std_atomic_lockfree_condition() );
	return;
}

TEST( lfmemAlloc, caller_context )
{
	// default contructor
	alpha::concurrent::caller_context test_val = ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG;

	// copy constructor
	alpha::concurrent::caller_context test_val2 = test_val;

	EXPECT_EQ( test_val.p_caller_func_name_, test_val2.p_caller_func_name_ );
	EXPECT_EQ( test_val.caller_lineno_, test_val2.caller_lineno_ );
	EXPECT_EQ( test_val.p_caller_src_fname_, test_val2.p_caller_src_fname_ );

	// move constructor
	alpha::concurrent::caller_context test_val3 = std::move( test_val );

	EXPECT_EQ( test_val2.p_caller_func_name_, test_val3.p_caller_func_name_ );
	EXPECT_EQ( test_val2.caller_lineno_, test_val3.caller_lineno_ );
	EXPECT_EQ( test_val2.p_caller_src_fname_, test_val3.p_caller_src_fname_ );

	return;
}

TEST( lfmemAlloc, TestBacktrace )
{
	std::size_t rq_size   = RQ_SIZE;
	void*       test_ptr1 = alpha::concurrent::gmem_allocate( rq_size );
	ASSERT_NE( nullptr, test_ptr1 );

	auto bt_info1 = alpha::concurrent::get_backtrace_info( test_ptr1 );
	ASSERT_TRUE( std::get<0>( bt_info1 ) );
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	EXPECT_NE( 0, std::get<1>( bt_info1 ).count_ );
#else
	EXPECT_EQ( 0, std::get<1>( bt_info1 ).count_ );
#endif
	alpha::concurrent::output_backtrace_info( alpha::concurrent::log_type::ERR, test_ptr1 );

	alpha::concurrent::gmem_deallocate( test_ptr1 );
#ifdef ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
#else
	bt_info1 = alpha::concurrent::get_backtrace_info( test_ptr1 );
	ASSERT_TRUE( std::get<0>( bt_info1 ) );
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	EXPECT_NE( 0, std::get<1>( bt_info1 ).count_ );
	EXPECT_NE( 0, std::get<2>( bt_info1 ).count_ );
#else
	EXPECT_EQ( 0, std::get<1>( bt_info1 ).count_ );
	EXPECT_EQ( 0, std::get<2>( bt_info1 ).count_ );
#endif
#endif

	void* test_ptr2 = alpha::concurrent::gmem_allocate( rq_size );
	ASSERT_NE( nullptr, test_ptr2 );

	auto bt_info2 = alpha::concurrent::get_backtrace_info( test_ptr2 );
	ASSERT_TRUE( std::get<0>( bt_info2 ) );
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	EXPECT_NE( 0, std::get<1>( bt_info2 ).count_ );
#else
	EXPECT_EQ( 0, std::get<1>( bt_info2 ).count_ );
#endif

	alpha::concurrent::output_backtrace_info( alpha::concurrent::log_type::ERR, test_ptr2 );

	alpha::concurrent::gmem_deallocate( test_ptr2 );
#ifdef ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
#else
	bt_info2 = alpha::concurrent::get_backtrace_info( test_ptr2 );
	ASSERT_TRUE( std::get<0>( bt_info2 ) );
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	EXPECT_NE( 0, std::get<1>( bt_info2 ).count_ );
	EXPECT_NE( 0, std::get<2>( bt_info2 ).count_ );
#else
	EXPECT_EQ( 0, std::get<1>( bt_info2 ).count_ );
	EXPECT_EQ( 0, std::get<2>( bt_info2 ).count_ );
#endif
#endif

	int err_cnt, warn_cnt;
	alpha::concurrent::GetErrorWarningLogCountAndReset( &err_cnt, &warn_cnt );
	return;
}

TEST( lfmemAlloc, TestBacktrace2 )
{
#ifdef ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
#else
	std::size_t rq_size   = RQ_SIZE;
	void*       test_ptr1 = std::malloc( rq_size );
	ASSERT_NE( nullptr, test_ptr1 );

	auto bt_info1 = alpha::concurrent::get_backtrace_info( test_ptr1 );
	std::free( test_ptr1 );
	ASSERT_FALSE( std::get<0>( bt_info1 ) );
#endif

	return;
}

TEST( lfmemAlloc, TestBacktrace3 )
{
	std::size_t rq_size   = 10000000;   // over size of max allocation slot size of default configuration
	void*       test_ptr1 = alpha::concurrent::gmem_allocate( rq_size );
	ASSERT_NE( nullptr, test_ptr1 );

	auto bt_info1 = alpha::concurrent::get_backtrace_info( test_ptr1 );
	ASSERT_TRUE( std::get<0>( bt_info1 ) );
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	EXPECT_NE( 0, std::get<1>( bt_info1 ).count_ );
#else
	EXPECT_EQ( 0, std::get<1>( bt_info1 ).count_ );
#endif
	alpha::concurrent::gmem_deallocate( test_ptr1 );

	return;
}
