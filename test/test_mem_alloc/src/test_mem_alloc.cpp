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

#include "alconcurrent/lf_mem_alloc.hpp"
#include "alconcurrent/lf_mem_alloc_internal.hpp"
#include "alconcurrent/lf_mem_alloc_type.hpp"

alpha::concurrent::param_chunk_allocation param = { 27, 2 };

class ChunkHeaderMultiSlotMaltiThread : public testing::TestWithParam<unsigned int> {
	// You can implement all the usual fixture class members here.
	// To access the test parameter, call GetParam() from class
	// TestWithParam<T>.
public:
	ChunkHeaderMultiSlotMaltiThread( void )
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

	unsigned int num_thread_;
};

TEST_P( ChunkHeaderMultiSlotMaltiThread, TC_one_by_one )
{
	alpha::concurrent::param_chunk_allocation             param = { 27, 2 * num_thread_ };
	alpha::concurrent::internal::chunk_list_statistics    test_st;
	alpha::concurrent::internal::chunk_header_multi_slot* p_chms = new alpha::concurrent::internal::chunk_header_multi_slot( param, 0, &test_st );

	for ( unsigned int i = 0; i < num_thread_; i++ ) {
		std::thread tt( [p_chms]() {
			void* test_ptr1 = p_chms->allocate_mem_slot();
			void* test_ptr2 = p_chms->allocate_mem_slot();

			EXPECT_NE( nullptr, test_ptr1 );
			EXPECT_NE( nullptr, test_ptr2 );

			EXPECT_TRUE( p_chms->recycle_mem_slot( test_ptr1 ) );
			EXPECT_TRUE( p_chms->recycle_mem_slot( test_ptr2 ) );
		} );
		tt.join();
	}

	alpha::concurrent::chunk_statistics e = p_chms->get_statistics();

	printf( "%s\n", e.print().c_str() );

	p_chms->dump();

	// printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	delete p_chms;
	// printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
}

TEST_P( ChunkHeaderMultiSlotMaltiThread, TC_at_same_time )
{
	alpha::concurrent::param_chunk_allocation             param = { 27, 2 * num_thread_ };
	alpha::concurrent::internal::chunk_list_statistics    test_st;
	alpha::concurrent::internal::chunk_header_multi_slot* p_chms = new alpha::concurrent::internal::chunk_header_multi_slot( param, 0, &test_st );
	std::thread                                           tt[num_thread_];
	pthread_barrier_t                                     barrier;
	pthread_barrier_init( &barrier, NULL, num_thread_ + 1 );

	for ( unsigned int i = 0; i < num_thread_; i++ ) {
		tt[i] = std::thread( [p_chms, &barrier]() {
			pthread_barrier_wait( &barrier );
			void* test_ptr1 = p_chms->allocate_mem_slot();
			void* test_ptr2 = p_chms->allocate_mem_slot();

			EXPECT_NE( nullptr, test_ptr1 );
			EXPECT_NE( nullptr, test_ptr2 );

			std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );

			EXPECT_TRUE( p_chms->recycle_mem_slot( test_ptr1 ) );
			EXPECT_TRUE( p_chms->recycle_mem_slot( test_ptr2 ) );
		} );
	}
	pthread_barrier_wait( &barrier );
	for ( unsigned int i = 0; i < num_thread_; i++ ) {
		tt[i].join();
	}
	pthread_barrier_destroy( &barrier );

	alpha::concurrent::chunk_statistics e = p_chms->get_statistics();

	printf( "%s\n", e.print().c_str() );

	p_chms->dump();

	// printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	delete p_chms;
	// printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
}

INSTANTIATE_TEST_SUITE_P( many_threads,
                          ChunkHeaderMultiSlotMaltiThread,
                          testing::Values( 1, 2, 10, 30 ) );

class lfmemAlloc : public testing::Test {
	// You can implement all the usual fixture class members here.
	// To access the test parameter, call GetParam() from class
	// TestWithParam<T>.
public:
	lfmemAlloc( void )
	{
	}

	void SetUp() override
	{

		int err_cnt, warn_cnt;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &err_cnt, &warn_cnt );
		alpha::concurrent::gmem_prune();

		printf( "gmem Statistics is;\n" );
		std::list<alpha::concurrent::chunk_statistics> statistics = alpha::concurrent::gmem_get_statistics();
		for ( auto& e : statistics ) {
			EXPECT_EQ( 0, e.consum_cnt_ );
			printf( "%s\n", e.print().c_str() );
		}
	}
	void TearDown() override
	{
		printf( "gmem Statistics is;\n" );
		std::list<alpha::concurrent::chunk_statistics> statistics = alpha::concurrent::gmem_get_statistics();
		for ( auto& e : statistics ) {
			EXPECT_EQ( 0, e.consum_cnt_ );
			printf( "%s\n", e.print().c_str() );
		}

		int err_cnt, warn_cnt;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &err_cnt, &warn_cnt );
		EXPECT_EQ( err_cnt, 0 );
		EXPECT_EQ( warn_cnt, 0 );
		alpha::concurrent::GetErrorWarningLogCount( &err_cnt, &warn_cnt );
		EXPECT_EQ( err_cnt, 0 );
		EXPECT_EQ( warn_cnt, 0 );
	}
};

TEST_F( lfmemAlloc, TestChunkHeaderMultiSlot )
{
	alpha::concurrent::internal::chunk_list_statistics    test_st;
	alpha::concurrent::internal::chunk_header_multi_slot* p_chms = new alpha::concurrent::internal::chunk_header_multi_slot( param, 0, &test_st );

	void* test_ptr1 = p_chms->allocate_mem_slot();
	void* test_ptr2 = p_chms->allocate_mem_slot();
	void* test_ptr3 = p_chms->allocate_mem_slot();

	EXPECT_NE( nullptr, test_ptr1 );
	EXPECT_NE( nullptr, test_ptr2 );
	EXPECT_EQ( nullptr, test_ptr3 );

	EXPECT_FALSE( p_chms->recycle_mem_slot( test_ptr3 ) );
	EXPECT_FALSE( p_chms->recycle_mem_slot( reinterpret_cast<void*>( test_ptr1 + 1 ) ) );

	EXPECT_TRUE( p_chms->recycle_mem_slot( test_ptr1 ) );
	EXPECT_TRUE( p_chms->recycle_mem_slot( test_ptr2 ) );

	alpha::concurrent::chunk_statistics e = p_chms->get_statistics();

	printf( "%s\n", e.print().c_str() );

	p_chms->dump();

	// printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	delete p_chms;
	// printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
}

TEST_F( lfmemAlloc, TestChunkList_AdditionalAlloc )
{
	// max slot数２に対し、３つ目のスロットを要求した場合のテスト
	alpha::concurrent::internal::chunk_list* p_ch_lst = new alpha::concurrent::internal::chunk_list( param );

	void* test_ptr1 = p_ch_lst->allocate_mem_slot();
	void* test_ptr2 = p_ch_lst->allocate_mem_slot();
	void* test_ptr3 = p_ch_lst->allocate_mem_slot();

	EXPECT_NE( nullptr, test_ptr1 );
	EXPECT_NE( nullptr, test_ptr2 );
	EXPECT_NE( nullptr, test_ptr3 );

	EXPECT_TRUE( p_ch_lst->recycle_mem_slot( test_ptr3 ) );
	EXPECT_TRUE( p_ch_lst->recycle_mem_slot( test_ptr1 ) );
	EXPECT_TRUE( p_ch_lst->recycle_mem_slot( test_ptr2 ) );

	alpha::concurrent::chunk_statistics e = p_ch_lst->get_statistics();

	printf( "%s\n", e.print().c_str() );

	// printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
	delete p_ch_lst;
	// printf( "[%d] used pthread tsd key: %d, max used pthread tsd key: %d\n", 10, alpha::concurrent::internal::get_num_of_tls_key(), alpha::concurrent::internal::get_max_num_of_tls_key() );
}

TEST_F( lfmemAlloc, TestChunkList_IllegalAddressFree )
{
	// max slot数２に対し、３つ目のスロットを要求した場合のテスト
	alpha::concurrent::internal::chunk_list* p_ch_lst = new alpha::concurrent::internal::chunk_list( param );

	void* test_ptr1 = p_ch_lst->allocate_mem_slot();
	void* test_ptr2 = p_ch_lst->allocate_mem_slot();
	void* test_ptr3 = p_ch_lst->allocate_mem_slot();

	EXPECT_NE( nullptr, test_ptr1 );
	EXPECT_NE( nullptr, test_ptr2 );
	EXPECT_NE( nullptr, test_ptr3 );

	EXPECT_FALSE( p_ch_lst->recycle_mem_slot( reinterpret_cast<void*>( test_ptr3 + 1 ) ) );
	EXPECT_FALSE( p_ch_lst->recycle_mem_slot( reinterpret_cast<void*>( test_ptr1 + 1 ) ) );

	EXPECT_FALSE( p_ch_lst->recycle_mem_slot( reinterpret_cast<void*>( test_ptr2 + 1 ) ) );

	alpha::concurrent::chunk_statistics e = p_ch_lst->get_statistics();

	printf( "%s\n", e.print().c_str() );

	delete p_ch_lst;
}

TEST_F( lfmemAlloc, TestGeneralMemAllocator )
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

	printf( "number of keys of dynamic_tls_key_create(),     %d\n", alpha::concurrent::internal::get_num_of_tls_key() );
	printf( "max number of keys of dynamic_tls_key_create(), %d\n", alpha::concurrent::internal::get_max_num_of_tls_key() );
}

TEST_F( lfmemAlloc, TestGeneralMemAllocator_prune )
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

	printf( "number of keys of dynamic_tls_key_create(),     %d\n", alpha::concurrent::internal::get_num_of_tls_key() );
	printf( "max number of keys of dynamic_tls_key_create(), %d\n", alpha::concurrent::internal::get_max_num_of_tls_key() );

	return;
}

#define GM_ALIGN_SIZE ( alignof( std::max_align_t ) )
#define RQ_SIZE       ( GM_ALIGN_SIZE + 1 )

TEST_F( lfmemAlloc, TestGMemAllocator )
{
	std::size_t rq_size = RQ_SIZE;
	for ( int i = 0; i < 13; i++ ) {
		void* test_ptr1 = alpha::concurrent::gmem_allocate( rq_size );
		EXPECT_NE( nullptr, test_ptr1 ) << std::to_string( i ) << ": request size: " << std::to_string( rq_size ) << std::endl;

		// GM_ALIGN_SIZEでメモリがアライメントされていることを確認する
		std::uintptr_t chk_ptr_align = reinterpret_cast<std::uintptr_t>( test_ptr1 );
		EXPECT_EQ( chk_ptr_align % GM_ALIGN_SIZE, 0 ) << std::to_string( i ) << ": request size: " << std::to_string( rq_size ) << std::endl;

		alpha::concurrent::gmem_deallocate( test_ptr1 );

		rq_size *= 2;
	}

	printf( "number of keys of dynamic_tls_key_create(),     %d\n", alpha::concurrent::internal::get_num_of_tls_key() );
	printf( "max number of keys of dynamic_tls_key_create(), %d\n", alpha::concurrent::internal::get_max_num_of_tls_key() );
}

TEST_F( lfmemAlloc, PlatformCheck )
{
	EXPECT_TRUE( alpha::concurrent::test_platform_std_atomic_lockfree_condition() );
	return;
}

#if 0
TEST_F( lfmemAlloc, caller_context )
{
	// default contructor
	alpha::concurrent::caller_context test_val = ;

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
#endif
TEST_F( lfmemAlloc, TestBacktrace )
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
	alpha::concurrent::output_backtrace_info( alpha::concurrent::log_type::TEST, test_ptr1 );

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

	alpha::concurrent::output_backtrace_info( alpha::concurrent::log_type::TEST, test_ptr2 );

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

	return;
}

TEST_F( lfmemAlloc, TestBacktrace2 )
{
#if defined( TEST_ENABLE_THREADSANITIZER ) || defined( TEST_ENABLE_ADDRESSSANITIZER )
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

TEST_F( lfmemAlloc, TestBacktrace3 )
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

TEST( expriment_impl, general_mem_allocator_impl_test )
{
	constexpr alpha::concurrent::param_chunk_allocation test_param_array[] = {
		{ alignof( std::max_align_t ), 32 },       // 1
		{ alignof( std::max_align_t ) * 2, 32 },   // 2
	};                                             //!< pointer to default parameter array

	alpha::concurrent::static_general_mem_allocator<2> a(
		alpha::concurrent::param_chunk_allocation { 24, 32 },      // 1
		alpha::concurrent::param_chunk_allocation { 24 * 2, 32 }   // 2
	);

	auto ret_st = a.get_statistics();
	for ( auto& e : ret_st ) {
		auto result_str = e.print();
		printf( "%s\n", result_str.c_str() );
	}
}