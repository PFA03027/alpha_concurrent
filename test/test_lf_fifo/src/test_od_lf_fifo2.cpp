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
#include "alconcurrent/internal/od_lockfree_fifo.hpp"
#include "alconcurrent/internal/od_node_essence.hpp"
#include "alconcurrent/internal/od_node_pool.hpp"

using test_fifo_type = alpha::concurrent::internal::od_lockfree_fifo;
class test_node_type : public alpha::concurrent::internal::od_node_simple_link, public alpha::concurrent::internal::od_node_link_by_hazard_handler {};
// class test_node_type : public alpha::concurrent::internal::od_node_link_by_hazard_handler, public alpha::concurrent::internal::od_node_simple_link {};
using test_pool_type = alpha::concurrent::internal::od_node_pool<test_node_type>;

//////////////////////////////////////////////////////////////////////////////////////////////////

struct Nthread_push_pop_task_with_node_pool {
	test_fifo_type&  sut_;
	const size_t     thread_num_;
	std::latch       start_sync_latch_;
	std::atomic_bool loop_flag_;
	test_pool_type   unused_node_;

	Nthread_push_pop_task_with_node_pool( size_t nthreads, test_fifo_type& target_sut )
	  : sut_( target_sut )
	  , thread_num_( nthreads )
	  , start_sync_latch_( static_cast<ptrdiff_t>( nthreads + 1 ) )
	  , loop_flag_( true )
	  , unused_node_()
	{
	}

	bool pushpop_test( void )
	{
		start_sync_latch_.arrive_and_wait();

		while ( loop_flag_.load( std::memory_order_acquire ) ) {
			test_node_type* p_nd_tmp = unused_node_.pop();
			if ( p_nd_tmp == nullptr ) {
				p_nd_tmp = new test_node_type;
			}
			sut_.push_back( p_nd_tmp );

			typename test_fifo_type::node_pointer p_poped_node = sut_.pop_front( nullptr );
			if ( p_poped_node == nullptr ) {
				return false;
			}
			test_node_type* p_casted_poped_node = dynamic_cast<test_node_type*>( p_poped_node );
			if ( p_poped_node == nullptr ) {
				// p_poped_node should be leaked to avoid unexpected behaviour by unexpected memory access
				return false;
			}
			unused_node_.push( p_casted_poped_node );
		}

		return true;
	}

	bool test_task( size_t test_milliseconds )
	{

		struct thread_data {
			std::thread       worker_;
			std::future<bool> ret_;
		};

		std::vector<thread_data> tasks_and_threads( thread_num_ );
		for ( auto& t : tasks_and_threads ) {
			std::packaged_task<bool( void )> task(
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

		bool ret = true;
		for ( auto& t : tasks_and_threads ) {
			ret = ret && t.ret_.get();
		}

		for ( auto& t : tasks_and_threads ) {
			if ( t.worker_.joinable() ) {
				t.worker_.join();
			}
		}

		return ret;
	}
};

class Test_od_lockfree_fifo_Highload2 : public ::testing::Test {
protected:
	void SetUp() override
	{
		alpha::concurrent::GetErrorWarningLogCountAndReset( nullptr, nullptr );

		p_sut_ = new test_fifo_type( new test_node_type );
	}

	void TearDown() override
	{
		typename test_fifo_type::node_pointer p_released_sentinel_node = p_sut_->release_sentinel_node();
		ASSERT_NE( p_released_sentinel_node, nullptr );
		delete p_released_sentinel_node;

		delete p_sut_;

		int cw, ce;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
		EXPECT_EQ( ce, 0 );
		EXPECT_EQ( cw, 0 );
	}

	test_fifo_type* p_sut_;
};

TEST_F( Test_od_lockfree_fifo_Highload2, NThread_1thread_PushPop )
{
	// Arrange
	Nthread_push_pop_task_with_node_pool sut_env( 1, *p_sut_ );

	// Act
	bool ret = sut_env.test_task( 1000 );

	// Assert
	EXPECT_TRUE( ret );
}

TEST_F( Test_od_lockfree_fifo_Highload2, NThread_2threads_PushPop )
{
	// Arrange
	Nthread_push_pop_task_with_node_pool sut_env( 2, *p_sut_ );

	// Act
	bool ret = sut_env.test_task( 1000 );

	// Assert
	EXPECT_TRUE( ret );
}

TEST_F( Test_od_lockfree_fifo_Highload2, NThread_32threads_PushPop )
{
	// Arrange
	Nthread_push_pop_task_with_node_pool sut_env( 32, *p_sut_ );

	// Act
	bool ret = sut_env.test_task( 1000 );

	// Assert
	EXPECT_TRUE( ret );
}
