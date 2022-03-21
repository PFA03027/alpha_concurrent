//============================================================================
// Name        : test_mem_alloc.cpp
// Author      : Teruaki Ata
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C, Ansi-style
//============================================================================

#include <stdio.h>
#include <stdlib.h>

#include <cstdint>
#include <iostream>

#include "gtest/gtest.h"

#include "alconcurrent/lf_mem_alloc.hpp"

alpha::concurrent::param_chunk_allocation param = { 27, 2 };

TEST( lfmemAlloc, TestChunkHeaderMultiSlot )
{
	alpha::concurrent::internal::chunk_header_multi_slot* p_chms = new alpha::concurrent::internal::chunk_header_multi_slot( param );

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

	delete p_chms;
}

TEST( lfmemAlloc, TestChunkList )
{
	alpha::concurrent::internal::chunk_list* p_ch_lst = new alpha::concurrent::internal::chunk_list( param );

	void* test_ptr1 = p_ch_lst->allocate_mem_slot();
	void* test_ptr2 = p_ch_lst->allocate_mem_slot();
	void* test_ptr3 = p_ch_lst->allocate_mem_slot();

	EXPECT_NE( nullptr, test_ptr1 );
	EXPECT_NE( nullptr, test_ptr2 );
	EXPECT_NE( nullptr, test_ptr3 );

	EXPECT_TRUE( p_ch_lst->recycle_mem_slot( test_ptr3 ) );
	EXPECT_FALSE( p_ch_lst->recycle_mem_slot( reinterpret_cast<void*>( test_ptr1 + 1 ) ) );

	EXPECT_TRUE( p_ch_lst->recycle_mem_slot( test_ptr1 ) );
	EXPECT_TRUE( p_ch_lst->recycle_mem_slot( test_ptr2 ) );

	alpha::concurrent::chunk_statistics e = p_ch_lst->get_statistics();

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

	delete p_ch_lst;
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
}

#define CACHE_LINE_BYTES ( 64 )
//#define GM_ALIGN_SIZE ( sizeof( std::max_align_t ) > CACHE_LINE_BYTES ? sizeof( std::max_align_t ) : CACHE_LINE_BYTES )

TEST( lfmemAlloc, TestGMemAllocator )
{
	std::size_t rq_size = CACHE_LINE_BYTES;
	for ( int i = 1; i < 13; i++ ) {
		void* test_ptr1 = alpha::concurrent::gmem_allocate( rq_size );
		EXPECT_NE( nullptr, test_ptr1 ) << std::to_string( i ) << ": request size: " << std::to_string( rq_size ) << std::endl;

		//		// CACHE_LINE_BYTESでメモリがアライメントされていることを確認する
		//		std::uintptr_t chk_ptr_align = reinterpret_cast<std::uintptr_t>( test_ptr1 );
		//		EXPECT_EQ( chk_ptr_align % GM_ALIGN_SIZE, 0 ) << std::to_string( i ) << ": request size: " << std::to_string( rq_size ) << std::endl;

		alpha::concurrent::gmem_deallocate( test_ptr1 );

		rq_size *= 2;
	}

	printf( "number of keys of pthread_key_create(),     %d\n", alpha::concurrent::internal::get_num_of_tls_key() );
	printf( "max number of keys of pthread_key_create(), %d\n", alpha::concurrent::internal::get_max_num_of_tls_key() );
}
