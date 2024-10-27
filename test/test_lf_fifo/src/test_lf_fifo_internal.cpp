/**
 * @file test_lf_fifo_internal.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-10-14
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "gtest/gtest.h"

#include "alconcurrent/lf_fifo.hpp"

TEST( Internal_New_FIFO, CanConstruct )
{
	// Arrenge
	using sut_type      = alpha::concurrent::internal::node_fifo_lockfree_base<int>;
	using sut_node_type = alpha::concurrent::internal::node_fifo_lockfree_base<int>::node_type;

	sut_node_type* p_sentinel = new sut_node_type( nullptr );

	// Act
	sut_type sut( p_sentinel );

	// Assert
	EXPECT_TRUE( sut.is_empty() );

	// Cleanup
	delete sut.release_sentinel_node();
}

TEST( Internal_New_FIFO, CanPop_from_Empty_return_Nullptr )
{
	// Arrenge
	using sut_type      = alpha::concurrent::internal::node_fifo_lockfree_base<int>;
	using sut_node_type = alpha::concurrent::internal::node_fifo_lockfree_base<int>::node_type;

	sut_node_type* p_sentinel = new sut_node_type( nullptr );
	sut_type       sut( p_sentinel );

	// Act
	auto ret = sut.pop_front();

	// Assert
	EXPECT_EQ( std::get<0>( ret ), nullptr );

	// Cleanup
	delete sut.release_sentinel_node();
}

TEST( Internal_New_FIFO, CanPush_then_Pop_return_ValidNode_and_value )
{
	// Arrenge
	using sut_type      = alpha::concurrent::internal::node_fifo_lockfree_base<int>;
	using sut_node_type = alpha::concurrent::internal::node_fifo_lockfree_base<int>::node_type;

	sut_node_type* p_sentinel = new sut_node_type( nullptr );
	sut_type       sut( p_sentinel );
	sut_node_type* p_node = new sut_node_type( nullptr );

	// Act
	sut.push_back( 1, p_node );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	auto ret = sut.pop_front();
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_NE( std::get<0>( ret ), nullptr );
	EXPECT_EQ( std::get<1>( ret ), 1 );

	// Cleanup
	delete std::get<0>( ret );
	delete sut.release_sentinel_node();
}

TEST( Internal_New_FIFO, CanPushPush_then_Pop_return_ValidNode_and_value )
{
	// Arrenge
	using sut_type      = alpha::concurrent::internal::node_fifo_lockfree_base<int>;
	using sut_node_type = alpha::concurrent::internal::node_fifo_lockfree_base<int>::node_type;

	sut_node_type* p_sentinel = new sut_node_type( nullptr );
	sut_type       sut( p_sentinel );
	sut_node_type* p_node = new sut_node_type( nullptr );
	sut.push_back( 1, p_node );
	p_node = new sut_node_type( nullptr );

	// Act
	sut.push_back( 2, p_node );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	auto ret = sut.pop_front();
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_NE( std::get<0>( ret ), nullptr );
	EXPECT_EQ( std::get<1>( ret ), 1 );
	delete std::get<0>( ret );

	ret = sut.pop_front();
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_NE( std::get<0>( ret ), nullptr );
	EXPECT_EQ( std::get<1>( ret ), 2 );
	delete std::get<0>( ret );

	// Cleanup
	delete sut.release_sentinel_node();
}
