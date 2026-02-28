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

using test_fifo_type = alpha::concurrent::internal::od_lockfree_fifo;
using test_node_type = alpha::concurrent::internal::od_node_link_by_hazard_handler;

//////////////////////////////////////////////////////////////////////////////////////////////////

struct Nthread_push_pop_task {
	static thread_local std::list<test_node_type*> used_nodes_list_;

	static void push_used_node_to_used_nodes_list( test_node_type* p_used_node )
	{
		used_nodes_list_.push_back( p_used_node );
	}

	static void clean_used_nodes_list( void )
	{
		while ( !used_nodes_list_.empty() ) {
			if ( used_nodes_list_.front() != nullptr ) {
				while ( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( used_nodes_list_.front() ) ) {
					std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
				}
				delete used_nodes_list_.front();
			}
			used_nodes_list_.pop_front();
		}
	}

	static bool pushback_popfront_test( std::latch& start_latch, std::atomic_bool& loop_flag, test_fifo_type& sut )
	{
		start_latch.arrive_and_wait();

		while ( loop_flag.load( std::memory_order_acquire ) ) {
			sut.push_back( new test_node_type );
			test_node_type* p_poped_node = sut.pop_front( nullptr );
			if ( p_poped_node == nullptr ) {
				return false;
			}
			push_used_node_to_used_nodes_list( p_poped_node );
		}

		clean_used_nodes_list();
		return true;
	}

	static bool pushfront_popfront_test( std::latch& start_latch, std::atomic_bool& loop_flag, test_fifo_type& sut )
	{
		start_latch.arrive_and_wait();

		while ( loop_flag.load( std::memory_order_acquire ) ) {
			test_node_type* p_poped_node = sut.push_front( new test_node_type, new test_node_type );
			if ( p_poped_node == nullptr ) {
				return false;
			}
			push_used_node_to_used_nodes_list( p_poped_node );

			p_poped_node = sut.pop_front( nullptr );
			if ( p_poped_node == nullptr ) {
				return false;
			}
			push_used_node_to_used_nodes_list( p_poped_node );
		}

		clean_used_nodes_list();
		return true;
	}

	static bool pushfront_pushback_popfront_test( std::latch& start_latch, std::atomic_bool& loop_flag, test_fifo_type& sut )
	{
		start_latch.arrive_and_wait();

		while ( loop_flag.load( std::memory_order_acquire ) ) {
			test_node_type* p_poped_node;
			p_poped_node = sut.push_front( new test_node_type, new test_node_type );
			if ( p_poped_node == nullptr ) {
				return false;
			}
			push_used_node_to_used_nodes_list( p_poped_node );
			p_poped_node = sut.pop_front( nullptr );
			if ( p_poped_node == nullptr ) {
				return false;
			}
			push_used_node_to_used_nodes_list( p_poped_node );

			sut.push_back( new test_node_type );
			p_poped_node = sut.pop_front( nullptr );
			if ( p_poped_node == nullptr ) {
				return false;
			}
			push_used_node_to_used_nodes_list( p_poped_node );
		}

		clean_used_nodes_list();
		return true;
	}

	static bool test_task_pushback_popfront( size_t nthreads, size_t test_milliseconds, test_fifo_type& sut )
	{
		return test_task_common( nthreads, test_milliseconds, sut, pushback_popfront_test );
	}

	static bool test_task_pushfront_popfront( size_t nthreads, size_t test_milliseconds, test_fifo_type& sut )
	{
		return test_task_common( nthreads, test_milliseconds, sut, pushfront_popfront_test );
	}

	static bool test_task_pushfront_pushback_popfront( size_t nthreads, size_t test_milliseconds, test_fifo_type& sut )
	{
		return test_task_common( nthreads, test_milliseconds, sut, pushfront_pushback_popfront_test );
	}

private:
	static bool test_task_common( size_t nthreads, size_t test_milliseconds, test_fifo_type& sut, bool ( *tf )( std::latch&, std::atomic_bool&, test_fifo_type& ) )
	{
		std::latch       start_sync_latch( static_cast<ptrdiff_t>( nthreads + 1 ) );
		std::atomic_bool loop_flag( true );

		struct thread_data {
			std::thread       worker_;
			std::future<bool> ret_;
		};

		std::vector<thread_data> tasks_and_threads( nthreads );
		for ( auto& t : tasks_and_threads ) {
			std::packaged_task<bool( std::latch&, std::atomic_bool&, test_fifo_type& )> task(
				[tf]( std::latch& sync_latch, std::atomic_bool& lf, test_fifo_type& target_sut ) {
					return ( *tf )( sync_latch, lf, target_sut );
				} );   // 非同期実行する関数を登録する
			t.ret_    = task.get_future();
			t.worker_ = std::thread( std::move( task ), std::ref( start_sync_latch ), std::ref( loop_flag ), std::ref( sut ) );
		}

		start_sync_latch.arrive_and_wait();
		std::this_thread::sleep_for( std::chrono::milliseconds( test_milliseconds ) );
		loop_flag.store( false, std::memory_order_release );

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

thread_local std::list<test_node_type*> Nthread_push_pop_task::used_nodes_list_;

class Test_od_lockfree_fifo_Highload : public ::testing::Test {
protected:
	void SetUp() override
	{
		alpha::concurrent::GetErrorWarningLogCountAndReset( nullptr, nullptr );

		p_sut_ = new test_fifo_type( new test_node_type );
	}

	void TearDown() override
	{
		test_node_type* p_released_sentinel_node = p_sut_->release_sentinel_node();
		EXPECT_NE( p_released_sentinel_node, nullptr );
		delete p_released_sentinel_node;

		delete p_sut_;

		int cw, ce;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
		EXPECT_EQ( ce, 0 );
		EXPECT_EQ( cw, 0 );
	}

	test_fifo_type* p_sut_;
};

TEST_F( Test_od_lockfree_fifo_Highload, NThread_1thread_PushPop1 )
{
	// Arrange

	// Act
	bool ret = Nthread_push_pop_task::test_task_pushback_popfront( 1, 100, *p_sut_ );

	// Assert
	EXPECT_TRUE( ret );
}

TEST_F( Test_od_lockfree_fifo_Highload, NThread_2threads_PushPop1 )
{
	// Arrange

	// Act
	bool ret = Nthread_push_pop_task::test_task_pushback_popfront( 2, 100, *p_sut_ );

	// Assert
	EXPECT_TRUE( ret );
}

TEST_F( Test_od_lockfree_fifo_Highload, NThread_32threads_PushPop1 )
{
	// Arrange

	// Act
	bool ret = Nthread_push_pop_task::test_task_pushback_popfront( 32, 100, *p_sut_ );

	// Assert
	EXPECT_TRUE( ret );
}

TEST_F( Test_od_lockfree_fifo_Highload, NThread_1thread_PushPop2 )
{
	// Arrange

	// Act
	bool ret = Nthread_push_pop_task::test_task_pushfront_popfront( 1, 100, *p_sut_ );

	// Assert
	EXPECT_TRUE( ret );
}

TEST_F( Test_od_lockfree_fifo_Highload, NThread_2threads_PushPop2 )
{
	// Arrange

	// Act
	bool ret = Nthread_push_pop_task::test_task_pushfront_popfront( 2, 100, *p_sut_ );

	// Assert
	EXPECT_TRUE( ret );
}

TEST_F( Test_od_lockfree_fifo_Highload, NThread_32threads_PushPop2 )
{
	// Arrange

	// Act
	bool ret = Nthread_push_pop_task::test_task_pushfront_popfront( 32, 100, *p_sut_ );

	// Assert
	EXPECT_TRUE( ret );
}
TEST_F( Test_od_lockfree_fifo_Highload, NThread_1thread_PushPop3 )
{
	// Arrange

	// Act
	bool ret = Nthread_push_pop_task::test_task_pushfront_pushback_popfront( 1, 100, *p_sut_ );

	// Assert
	EXPECT_TRUE( ret );
}

TEST_F( Test_od_lockfree_fifo_Highload, NThread_2threads_PushPop3 )
{
	// Arrange

	// Act
	bool ret = Nthread_push_pop_task::test_task_pushfront_pushback_popfront( 2, 100, *p_sut_ );

	// Assert
	EXPECT_TRUE( ret );
}

TEST_F( Test_od_lockfree_fifo_Highload, NThread_32threads_PushPop3 )
{
	// Arrange

	// Act
	bool ret = Nthread_push_pop_task::test_task_pushfront_pushback_popfront( 32, 100, *p_sut_ );

	// Assert
	EXPECT_TRUE( ret );
}
