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

using test_fifo_type = alpha::concurrent::internal::x_fifo_list<size_t>;

//////////////////////////////////////////////////////////////////////////////////////////////////

struct Nthread_push_pop_task_of_x_fifo_list {
	test_fifo_type&  sut_;
	const size_t     thread_num_;
	std::latch       start_sync_latch_;
	std::atomic_bool loop_flag_;

	Nthread_push_pop_task_of_x_fifo_list( size_t nthreads, test_fifo_type& target_sut )
	  : sut_( target_sut )
	  , thread_num_( nthreads )
	  , start_sync_latch_( static_cast<ptrdiff_t>( nthreads + 1 ) )
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
			auto [flag, v] = sut_.pop();
			if ( !flag ) {
				return std::tuple<bool, size_t, size_t>( false, 0, 0 );
			}
			cur_val = v + 1;
		}

		return std::tuple<bool, size_t, size_t>( true, count, cur_val );
	}

	std::tuple<bool, bool> test_task( size_t test_milliseconds )
	{

		struct thread_data {
			std::thread                                   worker_;
			std::future<std::tuple<bool, size_t, size_t>> ret_;
		};

		std::vector<thread_data> tasks_and_threads( thread_num_ );
		for ( auto& t : tasks_and_threads ) {
			std::packaged_task<std::tuple<bool, size_t, size_t>( void )> task(
				[this]() {
					return pushpop_test();
				} );   // 非同期実行する関数を登録する
			t.ret_    = task.get_future();
			t.worker_ = std::thread( std::move( task ) );
		}

		start_sync_latch_.arrive_and_wait();
		printf( "start test for %zu msec\n", test_milliseconds );
		std::this_thread::sleep_for( std::chrono::milliseconds( test_milliseconds ) );
		loop_flag_.store( false, std::memory_order_release );
		printf( "spend waiting time %zu msec\n", test_milliseconds );

		bool   total_ret           = true;
		size_t total_loop_count    = 0;
		size_t total_accumlate_val = 0;
		for ( auto& t : tasks_and_threads ) {
			auto [ret, cur_loop_count, cur_accumlate_val] = t.ret_.get();

			total_ret = total_ret && ret;
			total_loop_count += cur_loop_count;
			total_accumlate_val += cur_accumlate_val;
		}

		for ( auto& t : tasks_and_threads ) {
			if ( t.worker_.joinable() ) {
				t.worker_.join();
			}
		}

		printf( "calc result: loop_count=%zu, accumlate value=%zu\n", total_loop_count, total_accumlate_val );
		return std::tuple<bool, bool>( total_ret, ( total_loop_count == total_accumlate_val ) );
	}
};

class Test_od_lockfree_fifo_Highload3 : public ::testing::Test {
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

TEST_F( Test_od_lockfree_fifo_Highload3, NThread_1thread_PushPop )
{
	// Arrange
	Nthread_push_pop_task_of_x_fifo_list sut_env( 1, *p_sut_ );

	// Act
	auto [exec_ret, calc_ret] = sut_env.test_task( 1000 );

	// Assert
	EXPECT_TRUE( exec_ret );
	EXPECT_TRUE( calc_ret );
}

TEST_F( Test_od_lockfree_fifo_Highload3, NThread_2threads_PushPop )
{
	// Arrange
	Nthread_push_pop_task_of_x_fifo_list sut_env( 2, *p_sut_ );

	// Act
	auto [exec_ret, calc_ret] = sut_env.test_task( 1000 );

	// Assert
	EXPECT_TRUE( exec_ret );
	EXPECT_TRUE( calc_ret );
}

TEST_F( Test_od_lockfree_fifo_Highload3, NThread_32threads_PushPop )
{
	// Arrange
	Nthread_push_pop_task_of_x_fifo_list sut_env( 32, *p_sut_ );

	// Act
	auto [exec_ret, calc_ret] = sut_env.test_task( 1000 );

	// Assert
	EXPECT_TRUE( exec_ret );
	EXPECT_TRUE( calc_ret );
}
