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
#include "mem_small_memory_slot.hpp"

#if __cpp_lib_latch >= 201907L

using tut = alpha::concurrent::fifo_list<void*>;

struct allocator_if {
	virtual void*       allocate( size_t size ) = 0;
	virtual void        deallocate( void* p )   = 0;
	virtual const char* name( void )            = 0;
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

	const char* name( void ) override
	{
		return "gmem";
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

	const char* name( void ) override
	{
		return "malloc";
	}
};

class Test_MemGmemHighLoadParam : public testing::TestWithParam<std::tuple<size_t, size_t, allocator_if*>> {
public:
	static constexpr size_t num_thread = 32;

	static std::atomic<std::random_device::result_type> make_stable_test_pattern;
	static thread_local std::mt19937                    engine;

	static thread_local std::uniform_int_distribution<size_t> repeat_num_dist;

	// mmapを使用し始める128kbyteを超えると、mallocに負ける。
	static std::atomic<size_t>                                test_pattern_idx;
	static std::pair<size_t, size_t>                          test_pattern;
	static thread_local std::uniform_int_distribution<size_t> alloc_req_size_dist;

	static size_t push_memory_random_size_and_number( tut& fifo, allocator_if& allocator )
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

	static bool pop_memory_from_fifo( tut& fifo, allocator_if& allocator, size_t num )
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

	static alpha::concurrent::alcc_optional<size_t> random_size_memory_allocation_deallocation(
		std::latch*        p_latch,
		tut*               p_fifo,
		allocator_if*      p_allocator,
		std::atomic<bool>* p_loop_flag )
	{
		size_t total_num = 0;

		p_latch->arrive_and_wait();
		while ( p_loop_flag->load() ) {
			size_t num = push_memory_random_size_and_number( *p_fifo, *p_allocator );
			if ( num == 0 ) {
				return alpha::concurrent::alcc_nullopt;
			}
			bool pop_ret = pop_memory_from_fifo( *p_fifo, *p_allocator, num );
			if ( !pop_ret ) {
				throw std::runtime_error( "fail to pop" );
			}
			total_num += num;
		}

		return total_num;
	}
};

std::atomic<std::random_device::result_type> Test_MemGmemHighLoadParam::make_stable_test_pattern( 1 );
thread_local std::mt19937                    Test_MemGmemHighLoadParam::engine( make_stable_test_pattern.fetch_add( 1 ) );

thread_local std::uniform_int_distribution<size_t> Test_MemGmemHighLoadParam::repeat_num_dist( 0, 100 );

// mmapを使用し始める128kbyteを超えると、mallocに負ける。
std::pair<size_t, size_t>                          Test_MemGmemHighLoadParam::test_pattern;
thread_local std::uniform_int_distribution<size_t> Test_MemGmemHighLoadParam::alloc_req_size_dist(
	Test_MemGmemHighLoadParam::test_pattern.first,
	Test_MemGmemHighLoadParam::test_pattern.second );

TEST_P( Test_MemGmemHighLoadParam, test_mem_gmem_high_load )
{
	// Arrange
	test_pattern = { std::get<0>( GetParam() ), std::get<1>( GetParam() ) };
	make_stable_test_pattern.store( 1 );

	allocator_if*                                         p_allocator_obj = std::get<2>( GetParam() );
	tut                                                   fifo;
	std::atomic<bool>                                     loop_flag( true );
	std::future<alpha::concurrent::alcc_optional<size_t>> results[num_thread];
	std::latch                                            latch( num_thread + 1 );

	for ( size_t i = 0; i < num_thread; i++ ) {
		std::packaged_task<alpha::concurrent::alcc_optional<size_t>( std::latch*, tut*, allocator_if*, std::atomic<bool>* )> task( random_size_memory_allocation_deallocation );
		results[i] = task.get_future();
		std::thread t( std::move( task ), &latch, &fifo, p_allocator_obj, &loop_flag );
		t.detach();
	}

	// Act
	latch.arrive_and_wait();
	std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
	loop_flag.store( false );

	// Assert
	size_t total_num = 0;
	for ( size_t i = 0; i < num_thread; i++ ) {
		alpha::concurrent::alcc_optional<size_t> result;
		ASSERT_NO_THROW( result = results[i].get() );
		EXPECT_TRUE( result.has_value() );
		if ( result.has_value() ) {
			// printf( "total_num = %zu\n", result.value() );
			total_num += result.value();
		}
	}

	printf( "(%zu,%zu, %s)\tsum of total_num = %zu\n", test_pattern.first, test_pattern.second, p_allocator_obj->name(), total_num );
	alpha::concurrent::internal::memory_slot_group_list::dump_log( alpha::concurrent::log_type::DUMP, 'a', 1 );
}

allocator_gmem   test_gmem_allocator;
allocator_malloc test_malloc_allocator;

INSTANTIATE_TEST_SUITE_P(
	allocation_size_variation,
	Test_MemGmemHighLoadParam,
	testing::Values(
		std::tuple<size_t, size_t, allocator_if*> { 0, 512, &test_gmem_allocator },
		std::tuple<size_t, size_t, allocator_if*> { 0, 512, &test_malloc_allocator },
		std::tuple<size_t, size_t, allocator_if*> { 512 + 1, 1024 * 128 - 1, &test_gmem_allocator },
		std::tuple<size_t, size_t, allocator_if*> { 512 + 1, 1024 * 128 - 1, &test_malloc_allocator },
		std::tuple<size_t, size_t, allocator_if*> { 1024 * 129, 1024 * 1024 * 5, &test_gmem_allocator },
		std::tuple<size_t, size_t, allocator_if*> { 1024 * 129, 1024 * 1024 * 5, &test_malloc_allocator },
		std::tuple<size_t, size_t, allocator_if*> { 0, 1024 * 16, &test_gmem_allocator },
		std::tuple<size_t, size_t, allocator_if*> { 0, 1024 * 16, &test_malloc_allocator },
		std::tuple<size_t, size_t, allocator_if*> { 0, 1024 * 127, &test_gmem_allocator },
		std::tuple<size_t, size_t, allocator_if*> { 0, 1024 * 127, &test_malloc_allocator },
		std::tuple<size_t, size_t, allocator_if*> { 0, 1024 * 1024 * 5, &test_gmem_allocator },
		std::tuple<size_t, size_t, allocator_if*> { 0, 1024 * 1024 * 5, &test_malloc_allocator }
		//
		) );

#endif
