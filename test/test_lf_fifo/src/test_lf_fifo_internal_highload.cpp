/**
 * @file test_od_lf_fifo.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-11-04
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include <chrono>
#include <future>
#include <latch>
#include <list>
#include <thread>

#include "gtest/gtest.h"

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/internal/od_node_essence.hpp"
#include "alconcurrent/lf_fifo.hpp"

using test_fifo_type = alpha::concurrent::internal::x_lockfree_fifo<size_t>;

//////////////////////////////////////////////////////////////////////////////////////////////////

struct Nthread_push_pop_task_of_x_fifo_list {
	test_fifo_type&  sut_;
	const size_t     thread_num_;
	std::latch       start_sync_latch_;
	std::atomic_bool loop_flag_;

	Nthread_push_pop_task_of_x_fifo_list( size_t nthreads, test_fifo_type& target_sut )
	  : sut_( target_sut )
	  , thread_num_( nthreads )
	  , start_sync_latch_( static_cast<ptrdiff_t>( nthreads * 2 + 1 ) )
	  , loop_flag_( true )
	{
	}

	std::tuple<bool, size_t, size_t> pushpop_test( void )
	{
		size_t count = 0;
		start_sync_latch_.arrive_and_wait();

		size_t cur_val = 0;
		while ( loop_flag_.load( std::memory_order_acquire ) ) {
			count++;
			sut_.push( cur_val );
			auto ret = sut_.pop();
			if ( !ret.has_value() ) {
				return std::tuple<bool, size_t, size_t>( false, 0, 0 );
			}
			cur_val = ret.value() + 1;
		}

		return std::tuple<bool, size_t, size_t>( true, count, cur_val );
	}

	std::tuple<bool, size_t, size_t> pushheadpop_test( void )
	{
		size_t count = 0;
		start_sync_latch_.arrive_and_wait();

		size_t cur_val = 0;
		while ( loop_flag_.load( std::memory_order_acquire ) ) {
			count++;
			sut_.push_head( cur_val );
			auto ret = sut_.pop();
			if ( !ret.has_value() ) {
				return std::tuple<bool, size_t, size_t>( false, 0, 0 );
			}
			cur_val = ret.value() + 1;
		}

		return std::tuple<bool, size_t, size_t>( true, count, cur_val );
	}

	std::tuple<bool, bool> test_task( size_t test_milliseconds )
	{

		using result_future_t = std::future<std::tuple<bool, size_t, size_t>>;

		std::vector<result_future_t> tasks_result1( thread_num_ );
		std::vector<result_future_t> tasks_result2( thread_num_ );
		std::vector<std::thread>     workers1( thread_num_ );
		std::vector<std::thread>     workers2( thread_num_ );
		for ( auto& t : tasks_result1 ) {
			std::packaged_task<std::tuple<bool, size_t, size_t>( void )> task(
				[this]() {
					return pushpop_test();
				} );   // 非同期実行する関数を登録する
			t = task.get_future();

			workers1.emplace_back( std::thread( std::move( task ) ) );
		}
		for ( auto& t : tasks_result2 ) {
			std::packaged_task<std::tuple<bool, size_t, size_t>( void )> task(
				[this]() {
					return pushheadpop_test();
				} );   // 非同期実行する関数を登録する
			t = task.get_future();

			workers2.emplace_back( std::thread( std::move( task ) ) );
		}

		start_sync_latch_.arrive_and_wait();
		printf( "start test for %zu msec\n", test_milliseconds );
		std::this_thread::sleep_for( std::chrono::milliseconds( test_milliseconds ) );
		loop_flag_.store( false, std::memory_order_release );
		printf( "spend waiting time %zu msec\n", test_milliseconds );

		for ( auto& t : workers1 ) {
			if ( t.joinable() ) {
				t.join();
			}
		}
		for ( auto& t : workers2 ) {
			if ( t.joinable() ) {
				t.join();
			}
		}

		bool   total_ret           = true;
		size_t total_loop_count    = 0;
		size_t total_accumlate_val = 0;
		for ( auto& t : tasks_result1 ) {
			auto [ret, cur_loop_count, cur_accumlate_val] = t.get();

			total_ret = total_ret && ret;
			total_loop_count += cur_loop_count;
			total_accumlate_val += cur_accumlate_val;
		}
		for ( auto& t : tasks_result2 ) {
			auto [ret, cur_loop_count, cur_accumlate_val] = t.get();

			total_ret = total_ret && ret;
			total_loop_count += cur_loop_count;
			total_accumlate_val += cur_accumlate_val;
		}

		printf( "calc result: loop_count=%zu, accumlate value=%zu\n", total_loop_count, total_accumlate_val );
		return std::tuple<bool, bool>( total_ret, ( total_loop_count == total_accumlate_val ) );
	}
};

class Test_x_lockfree_fifo_Highload3 : public ::testing::Test {
protected:
	void SetUp() override
	{
		alpha::concurrent::GetErrorWarningLogCountAndReset( nullptr, nullptr );

		p_sut_ = new test_fifo_type;
	}

	void TearDown() override
	{
		delete p_sut_;

		int cw, ce;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
		EXPECT_EQ( ce, 0 );
		EXPECT_EQ( cw, 0 );
	}

	test_fifo_type* p_sut_;
};

TEST_F( Test_x_lockfree_fifo_Highload3, NThread_1thread_PushPop )
{
	// Arrange
	Nthread_push_pop_task_of_x_fifo_list sut_env( 1, *p_sut_ );

	// Act
	auto [exec_ret, calc_ret] = sut_env.test_task( 1000 );

	// Assert
	EXPECT_TRUE( exec_ret );
	EXPECT_TRUE( calc_ret );
}

TEST_F( Test_x_lockfree_fifo_Highload3, NThread_2threads_PushPop )
{
	// Arrange
	Nthread_push_pop_task_of_x_fifo_list sut_env( 2, *p_sut_ );

	// Act
	auto [exec_ret, calc_ret] = sut_env.test_task( 1000 );

	// Assert
	EXPECT_TRUE( exec_ret );
	EXPECT_TRUE( calc_ret );
}

TEST_F( Test_x_lockfree_fifo_Highload3, NThread_32threads_PushPop )
{
	// Arrange
	Nthread_push_pop_task_of_x_fifo_list sut_env( 32, *p_sut_ );

	// Act
	auto [exec_ret, calc_ret] = sut_env.test_task( 1000 );

	// Assert
	EXPECT_TRUE( exec_ret );
	EXPECT_TRUE( calc_ret );
}
