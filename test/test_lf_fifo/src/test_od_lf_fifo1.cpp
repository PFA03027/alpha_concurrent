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

class Test_od_lockfree_fifo : public ::testing::Test {
protected:
	void SetUp() override
	{
		alpha::concurrent::GetErrorWarningLogCountAndReset( nullptr, nullptr );
	}

	void TearDown() override
	{
		int cw, ce;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
		EXPECT_EQ( ce, 0 );
		EXPECT_EQ( cw, 0 );
	}
};

TEST_F( Test_od_lockfree_fifo, CanConstruct_Then_Destruct )
{
	// Arrange
	test_fifo_type* p_sut = nullptr;

	// Act
	ASSERT_NO_THROW( p_sut = new test_fifo_type( nullptr ) );

	// Assert
	ASSERT_NO_THROW( delete p_sut );
}

TEST_F( Test_od_lockfree_fifo, CanConstruct_WithSentinel_Then_Destruct )
{
	// Arrange
	test_fifo_type* p_sut = nullptr;
	test_node_type  sentinel;

	// Act
	ASSERT_NO_THROW( p_sut = new test_fifo_type( &sentinel ) );
	test_node_type* p_released_sentinel_node = p_sut->release_sentinel_node();

	// Assert
	EXPECT_EQ( p_released_sentinel_node, &sentinel );
	ASSERT_NO_THROW( delete p_sut );
}

TEST_F( Test_od_lockfree_fifo, DoubleReleaseSentinel_Then_Destruct )
{
	// Arrange
	test_node_type  sentinel;
	test_fifo_type  sut( &sentinel );
	test_node_type* p_released_sentinel_node = sut.release_sentinel_node();
	EXPECT_EQ( p_released_sentinel_node, &sentinel );

	// Act
	p_released_sentinel_node = sut.release_sentinel_node();

	// Assert
	EXPECT_EQ( p_released_sentinel_node, nullptr );
	int cw, ce;
	alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
	EXPECT_EQ( ce, 0 );
	EXPECT_EQ( cw, 1 );
}

TEST_F( Test_od_lockfree_fifo, CanMoveConstruct_Then_Destruct )
{
	// Arrange
	test_fifo_type src( new test_node_type );

	// Act
	test_fifo_type sut( std::move( src ) );

	// Assert
	test_node_type* p_released_sentinel_node = sut.release_sentinel_node();
	EXPECT_NE( p_released_sentinel_node, nullptr );
	delete p_released_sentinel_node;

	p_released_sentinel_node = src.release_sentinel_node();
	EXPECT_EQ( p_released_sentinel_node, nullptr );
	int cw, ce;
	alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
	EXPECT_EQ( ce, 0 );
	EXPECT_EQ( cw, 1 );
}

TEST_F( Test_od_lockfree_fifo, CanPush_Then_Pop )
{
	// Arrange
	test_fifo_type sut( new test_node_type );

	// Act
	sut.push_back( new test_node_type );
	test_node_type* p_poped_node1 = sut.pop_front();
	test_node_type* p_poped_node2 = sut.pop_front();

	// Assert
	EXPECT_NE( p_poped_node1, nullptr );
	delete p_poped_node1;
	EXPECT_EQ( p_poped_node2, nullptr );

	test_node_type* p_released_sentinel_node = sut.release_sentinel_node();
	EXPECT_NE( p_released_sentinel_node, nullptr );
	delete p_released_sentinel_node;
}

TEST_F( Test_od_lockfree_fifo, CanPush2_Then_Pop2 )
{
	// Arrange
	test_fifo_type sut( new test_node_type );
	sut.push_back( new test_node_type );
	sut.push_back( new test_node_type );

	// Act
	test_node_type* p_poped_node1 = sut.pop_front();
	test_node_type* p_poped_node2 = sut.pop_front();
	test_node_type* p_poped_node3 = sut.pop_front();

	// Assert
	EXPECT_NE( p_poped_node1, nullptr );
	delete p_poped_node1;
	EXPECT_NE( p_poped_node2, nullptr );
	delete p_poped_node2;
	EXPECT_EQ( p_poped_node3, nullptr );

	test_node_type* p_released_sentinel_node = sut.release_sentinel_node();
	EXPECT_NE( p_released_sentinel_node, nullptr );
	delete p_released_sentinel_node;
}

TEST_F( Test_od_lockfree_fifo, ConstructNoSentinel_CallIsEmpty_Then_True )
{
	// Arrange
	test_fifo_type sut( nullptr );

	// Act
	bool ret = sut.is_empty();

	// Assert
	EXPECT_TRUE( ret );
}

TEST_F( Test_od_lockfree_fifo, Construct_CallIsEmpty_Then_True )
{
	// Arrange
	test_fifo_type sut( new test_node_type );

	// Act
	bool ret = sut.is_empty();

	// Assert
	EXPECT_TRUE( ret );

	test_node_type* p_released_sentinel_node = sut.release_sentinel_node();
	EXPECT_NE( p_released_sentinel_node, nullptr );
	delete p_released_sentinel_node;
}

TEST_F( Test_od_lockfree_fifo, Push_CallIsEmpty_Then_False )
{
	// Arrange
	test_fifo_type sut( new test_node_type );
	sut.push_back( new test_node_type );

	// Act
	bool ret = sut.is_empty();

	// Assert
	EXPECT_FALSE( ret );

	test_node_type* p_poped_node1 = sut.pop_front();
	EXPECT_NE( p_poped_node1, nullptr );
	delete p_poped_node1;

	test_node_type* p_released_sentinel_node = sut.release_sentinel_node();
	EXPECT_NE( p_released_sentinel_node, nullptr );
	delete p_released_sentinel_node;
}

TEST_F( Test_od_lockfree_fifo, PushPop_CallIsEmpty_Then_True )
{
	// Arrange
	test_fifo_type sut( new test_node_type );
	sut.push_back( new test_node_type );
	test_node_type* p_poped_node1 = sut.pop_front();
	EXPECT_NE( p_poped_node1, nullptr );
	delete p_poped_node1;

	// Act
	bool ret = sut.is_empty();

	// Assert
	EXPECT_TRUE( ret );

	test_node_type* p_released_sentinel_node = sut.release_sentinel_node();
	EXPECT_NE( p_released_sentinel_node, nullptr );
	delete p_released_sentinel_node;
}

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
			}
			used_nodes_list_.pop_front();
		}
	}

	static bool pushpop_test( std::latch& start_latch, std::atomic_bool& loop_flag, test_fifo_type& sut )
	{
		start_latch.arrive_and_wait();

		while ( loop_flag.load( std::memory_order_acquire ) ) {
			sut.push_back( new test_node_type );
			test_node_type* p_poped_node = sut.pop_front();
			if ( p_poped_node == nullptr ) {
				return false;
			}
			push_used_node_to_used_nodes_list( p_poped_node );
		}

		clean_used_nodes_list();
		return true;
	}

	static bool test_task( size_t nthreads, size_t test_milliseconds, test_fifo_type& sut )
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
				[]( std::latch& sync_latch, std::atomic_bool& lf, test_fifo_type& target_sut ) {
					return pushpop_test( sync_latch, lf, target_sut );
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

		int cw, ce;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
		EXPECT_EQ( ce, 0 );
		EXPECT_EQ( cw, 0 );
	}

	test_fifo_type* p_sut_;
};

TEST_F( Test_od_lockfree_fifo_Highload, NThread_PushPop )
{
	// Arrange

	// Act
	bool ret = Nthread_push_pop_task::test_task( 32, 1000, *p_sut_ );

	// Assert
	EXPECT_TRUE( ret );
}
