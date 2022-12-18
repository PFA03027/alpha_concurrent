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

alpha::concurrent::param_chunk_allocation param = { 27, 2 };

TEST( lfmemAlloc, TestChunkHeaderMultiSlot )
{
	alpha::concurrent::internal::chunk_header_multi_slot* p_chms = new alpha::concurrent::internal::chunk_header_multi_slot( param );

	void* test_ptr1 = p_chms->allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
		__builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
		nullptr, 0, nullptr
#endif
#endif
	);
	void* test_ptr2 = p_chms->allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
		__builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
		nullptr, 0, nullptr
#endif
#endif
	);
	void* test_ptr3 = p_chms->allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
		__builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
		nullptr, 0, nullptr
#endif
#endif
	);

	EXPECT_NE( nullptr, test_ptr1 );
	EXPECT_NE( nullptr, test_ptr2 );
	EXPECT_EQ( nullptr, test_ptr3 );

	EXPECT_FALSE( p_chms->recycle_mem_slot( test_ptr3
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
	                                        ,
	                                        __builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
	                                        ,
	                                        nullptr, 0, nullptr
#endif
#endif
	                                            ) );
	EXPECT_FALSE( p_chms->recycle_mem_slot( reinterpret_cast<void*>( test_ptr1 + 1 )
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
	                                            ,
	                                        __builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
	                                            ,
	                                        nullptr, 0, nullptr
#endif
#endif
	                                            ) );

	EXPECT_TRUE( p_chms->recycle_mem_slot( test_ptr1
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
	                                       ,
	                                       __builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
	                                       ,
	                                       nullptr, 0, nullptr
#endif
#endif
	                                           ) );
	EXPECT_TRUE( p_chms->recycle_mem_slot( test_ptr2
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
	                                       ,
	                                       __builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
	                                       ,
	                                       nullptr, 0, nullptr
#endif
#endif
	                                           ) );

	alpha::concurrent::chunk_statistics e = p_chms->get_statistics();

	printf( "%s\n", e.print().c_str() );

	delete p_chms;

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

	void* test_ptr1 = p_ch_lst->allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
		__builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
		nullptr, 0, nullptr
#endif
#endif
	);
	void* test_ptr2 = p_ch_lst->allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
		__builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
		nullptr, 0, nullptr
#endif
#endif
	);
	void* test_ptr3 = p_ch_lst->allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
		__builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
		nullptr, 0, nullptr
#endif
#endif
	);

	EXPECT_NE( nullptr, test_ptr1 );
	EXPECT_NE( nullptr, test_ptr2 );
	EXPECT_NE( nullptr, test_ptr3 );

	EXPECT_TRUE( p_ch_lst->recycle_mem_slot( test_ptr3
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
	                                         ,
	                                         __builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
	                                         ,
	                                         nullptr, 0, nullptr
#endif
#endif
	                                             ) );
	EXPECT_TRUE( p_ch_lst->recycle_mem_slot( test_ptr1
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
	                                         ,
	                                         __builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
	                                         ,
	                                         nullptr, 0, nullptr
#endif
#endif
	                                             ) );
	EXPECT_TRUE( p_ch_lst->recycle_mem_slot( test_ptr2
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
	                                         ,
	                                         __builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
	                                         ,
	                                         nullptr, 0, nullptr
#endif
#endif
	                                             ) );

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

TEST( lfmemAlloc, TestChunkList_IllegalAddressFree )
{
	// max slot数２に対し、３つ目のスロットを要求した場合のテスト
	alpha::concurrent::internal::chunk_list* p_ch_lst = new alpha::concurrent::internal::chunk_list( param );

	void* test_ptr1 = p_ch_lst->allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
		__builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
		nullptr, 0, nullptr
#endif
#endif
	);
	void* test_ptr2 = p_ch_lst->allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
		__builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
		nullptr, 0, nullptr
#endif
#endif
	);
	void* test_ptr3 = p_ch_lst->allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
		__builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
		nullptr, 0, nullptr
#endif
#endif
	);

	EXPECT_NE( nullptr, test_ptr1 );
	EXPECT_NE( nullptr, test_ptr2 );
	EXPECT_NE( nullptr, test_ptr3 );

	EXPECT_FALSE( p_ch_lst->recycle_mem_slot( reinterpret_cast<void*>( test_ptr3 + 1 )
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
	                                              ,
	                                          __builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
	                                              ,
	                                          nullptr, 0, nullptr
#endif
#endif
	                                              ) );
	EXPECT_FALSE( p_ch_lst->recycle_mem_slot( reinterpret_cast<void*>( test_ptr1 + 1 )
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
	                                              ,
	                                          __builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
	                                              ,
	                                          nullptr, 0, nullptr
#endif
#endif
	                                              ) );

	EXPECT_FALSE( p_ch_lst->recycle_mem_slot( reinterpret_cast<void*>( test_ptr2 + 1 )
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
#ifdef __GNUC__
	                                              ,
	                                          __builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION()
#else
	                                              ,
	                                          nullptr, 0, nullptr
#endif
#endif
	                                              ) );

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

#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
TEST( lfmemAlloc, TestBacktrace )
{
	std::size_t rq_size   = RQ_SIZE;
	void*       test_ptr1 = alpha::concurrent::gmem_allocate( rq_size );
	ASSERT_NE( nullptr, test_ptr1 );

	auto bt_info1 = alpha::concurrent::get_backtrace_info( test_ptr1 );
	ASSERT_TRUE( std::get<0>( bt_info1 ).correct_ );
	EXPECT_NE( 0, std::get<1>( bt_info1 ).count_ );

	alpha::concurrent::output_backtrace_info( alpha::concurrent::log_type::ERR, test_ptr1 );

	alpha::concurrent::gmem_deallocate( test_ptr1 );

	void* test_ptr2 = alpha::concurrent::gmem_allocate( rq_size );
	ASSERT_NE( nullptr, test_ptr2 );

	auto bt_info2 = alpha::concurrent::get_backtrace_info( test_ptr2 );
	ASSERT_TRUE( std::get<0>( bt_info2 ).correct_ );
	EXPECT_NE( 0, std::get<1>( bt_info2 ).count_ );
	if ( ( test_ptr1 == test_ptr2 ) &&
	     ( std::get<0>( bt_info1 ).p_chms_ != nullptr ) &&
	     ( std::get<0>( bt_info2 ).p_chms_ != nullptr ) ) {
		EXPECT_NE( 0, std::get<2>( bt_info2 ).count_ );
	}

	alpha::concurrent::output_backtrace_info( alpha::concurrent::log_type::ERR, test_ptr2 );

	alpha::concurrent::gmem_deallocate( test_ptr2 );

	int err_cnt, warn_cnt;
	alpha::concurrent::GetErrorWarningLogCountAndReset( &err_cnt, &warn_cnt );
	return;
}

#endif