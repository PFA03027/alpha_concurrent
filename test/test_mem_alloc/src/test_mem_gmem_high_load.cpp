/**
 * @file test_mem_gmem_high_load.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-27
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include <future>
#include <random>
#include <thread>
#if __has_include( <latch>)
#include <latch>
#endif

#include "gtest/gtest.h"

#include "alconcurrent/lf_fifo.hpp"
#include "alconcurrent/lf_mem_alloc.hpp"

#if __cpp_lib_latch >= 201907L

using tut = alpha::concurrent::fifo_list<void*>;

std::random_device                                 gen_seed;
thread_local std::mt19937                          engine( gen_seed() );
thread_local std::uniform_int_distribution<size_t> repeat_num_dist( 0, 100 );

// mmapを使用し始める128kbyteを超えると、mallocに負ける。
// thread_local std::uniform_int_distribution<size_t> alloc_req_size_dist( 1, 1024 * 1024 * 5 );
thread_local std::uniform_int_distribution<size_t> alloc_req_size_dist( 1, 1024 * 127 );

struct allocator_if {
	virtual void* allocate( size_t size ) = 0;
	virtual void  deallocate( void* p )   = 0;
};

struct allocator_gmem : public allocator_if {
	void* allocate( size_t size ) override
	{
		return alpha::concurrent::gmem_allocate( size );
	}

	void deallocate( void* p ) override
	{
		alpha::concurrent::gmem_deallocate( p );
	}
};

struct allocator_malloc : public allocator_if {
	void* allocate( size_t size ) override
	{
		return malloc( size );
	}

	void deallocate( void* p ) override
	{
		free( p );
	}
};

size_t push_memory_random_size_and_number( tut& fifo, allocator_if& allocator )
{
	size_t total_num  = repeat_num_dist( engine ) + 1;
	size_t actual_num = 0;
	for ( size_t i = 0; i < total_num; i++ ) {
		size_t size = alloc_req_size_dist( engine );
		void*  p    = allocator.allocate( size );
		if ( p == nullptr ) {
			return 0;
		}
		fifo.push( p );
		actual_num++;
	}
	return actual_num;
}

bool pop_memory_from_fifo( tut& fifo, allocator_if& allocator, size_t num )
{
	for ( size_t i = 0; i < num; i++ ) {
		auto p = fifo.pop();
		if ( !p.has_value() ) {
			return false;
		}
		allocator.deallocate( p.value() );
	}
	return true;
}

alpha::concurrent::alcc_optional<size_t> random_size_memory_allocation_deallocation( std::latch* p_latch, tut* p_fifo, allocator_if* p_allocator, std::atomic<bool>* p_loop_flag )
{
	size_t total_num = 0;

	p_latch->arrive_and_wait();
	while ( p_loop_flag->load() ) {
		size_t num = push_memory_random_size_and_number( *p_fifo, *p_allocator );
		if ( num == 0 ) {
			return alpha::concurrent::alcc_nullopt;
		}
		pop_memory_from_fifo( *p_fifo, *p_allocator, num );
		total_num += num;
	}

	return total_num;
}

TEST( test_mem_gmem_high_load, test_mem_gmem_high_load )
{
	constexpr size_t num_thread = 32;

	tut                                                   fifo;
	std::atomic<bool>                                     loop_flag( true );
	std::future<alpha::concurrent::alcc_optional<size_t>> results[num_thread];
	allocator_gmem                                        gmem_allocator;
	std::latch                                            latch( num_thread + 1 );

	for ( size_t i = 0; i < num_thread; i++ ) {
		std::packaged_task<alpha::concurrent::alcc_optional<size_t>( std::latch*, tut*, allocator_if*, std::atomic<bool>* )> task( random_size_memory_allocation_deallocation );
		results[i] = task.get_future();
		std::thread t( std::move( task ), &latch, &fifo, &gmem_allocator, &loop_flag );
		t.detach();
	}

	latch.arrive_and_wait();
	std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
	loop_flag.store( false );

	size_t total_num = 0;
	for ( size_t i = 0; i < num_thread; i++ ) {
		alpha::concurrent::alcc_optional<size_t> result = results[i].get();
		EXPECT_TRUE( result.has_value() );
		if ( result.has_value() ) {
			printf( "total_num = %zu\n", result.value() );
			total_num += result.value();
		}
	}

	printf( "sum of total_num = %zu\n", total_num );
}

TEST( test_mem_gmem_high_load, compare_malloc_high_load )
{
	constexpr size_t num_thread = 32;

	tut                                                   fifo;
	std::atomic<bool>                                     loop_flag( true );
	std::future<alpha::concurrent::alcc_optional<size_t>> results[num_thread];
	allocator_malloc                                      malloc_allocator;
	std::latch                                            latch( num_thread + 1 );

	for ( size_t i = 0; i < num_thread; i++ ) {
		std::packaged_task<alpha::concurrent::alcc_optional<size_t>( std::latch*, tut*, allocator_if*, std::atomic<bool>* )> task( random_size_memory_allocation_deallocation );
		results[i] = task.get_future();
		std::thread t( std::move( task ), &latch, &fifo, &malloc_allocator, &loop_flag );
		t.detach();
	}

	latch.arrive_and_wait();
	std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
	loop_flag.store( false );

	size_t total_num = 0;
	for ( size_t i = 0; i < num_thread; i++ ) {
		alpha::concurrent::alcc_optional<size_t> result = results[i].get();
		EXPECT_TRUE( result.has_value() );
		if ( result.has_value() ) {
			printf( "total_num = %zu\n", result.value() );
			total_num += result.value();
		}
	}

	printf( "sum of total_num = %zu\n", total_num );
}

#endif
