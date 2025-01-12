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
	EXPECT_EQ( cw, 2 );
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
	EXPECT_EQ( cw, 2 );
}

TEST_F( Test_od_lockfree_fifo, CanPush_Then_Pop )
{
	// Arrange
	test_fifo_type sut( new test_node_type );

	// Act
	sut.push_back( new test_node_type );
	test_node_type* p_poped_node1 = sut.pop_front( nullptr );
	test_node_type* p_poped_node2 = sut.pop_front( nullptr );

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
	test_node_type* p_poped_node1 = sut.pop_front( nullptr );
	test_node_type* p_poped_node2 = sut.pop_front( nullptr );
	test_node_type* p_poped_node3 = sut.pop_front( nullptr );

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

TEST_F( Test_od_lockfree_fifo, Empty_DoPushFront_Then_OneElement )
{
	// Arrange
	test_fifo_type sut( new test_node_type );
	EXPECT_TRUE( sut.is_empty() );

	// Act
	test_node_type* p_old_node = sut.push_front( new test_node_type, new test_node_type );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_NE( p_old_node, nullptr );
	delete p_old_node;

	// Clean-up
	test_node_type* p_poped_node1 = sut.pop_front( nullptr );
	EXPECT_NE( p_poped_node1, nullptr );
	delete p_poped_node1;
	test_node_type* p_released_sentinel_node = sut.release_sentinel_node();
	EXPECT_NE( p_released_sentinel_node, nullptr );
	delete p_released_sentinel_node;
}

TEST_F( Test_od_lockfree_fifo, OneElement_DoPushFront_Then_TwoElement )
{
	// Arrange
	test_fifo_type sut( new test_node_type );
	EXPECT_TRUE( sut.is_empty() );
	sut.push_back( new test_node_type );
	EXPECT_FALSE( sut.is_empty() );

	// Act
	test_node_type* p_old_node = sut.push_front( new test_node_type, new test_node_type );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_NE( p_old_node, nullptr );
	delete p_old_node;
	test_node_type* p_poped_node1 = sut.pop_front( nullptr );
	EXPECT_NE( p_poped_node1, nullptr );
	delete p_poped_node1;
	p_poped_node1 = sut.pop_front( nullptr );
	EXPECT_NE( p_poped_node1, nullptr );
	delete p_poped_node1;
	int cw, ce;
	alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
	EXPECT_EQ( ce, 0 );
	EXPECT_EQ( cw, 0 );

	// Clean-up
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
	int cw, ce;
	alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
	EXPECT_EQ( ce, 0 );
	EXPECT_EQ( cw, 1 );
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

	test_node_type* p_poped_node1 = sut.pop_front( nullptr );
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
	test_node_type* p_poped_node1 = sut.pop_front( nullptr );
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
