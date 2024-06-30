/**
 * @file test_od_node.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-25
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include <thread>
#include <type_traits>

#include "gtest/gtest.h"

#include "alconcurrent/internal/od_node_pool.hpp"

///////////////////////////////////////////////////////////////////////////////////
class test_od_node_of_pool : public alpha::concurrent::internal::od_node_base<test_od_node_of_pool> {};

using sut_type = alpha::concurrent::internal::od_node_pool<test_od_node_of_pool>;

TEST( od_node_pool_class, CanConstructDestruct )
{
	// Arrange

	// Act
	sut_type sut;

	// Assert
	EXPECT_EQ( sut.pop(), nullptr );
}

TEST( od_node_pool_class, CanPushPop1 )
{
	// Arrange
	sut_type sut;
	auto     p_tmp = new test_od_node_of_pool;

	// Act
	sut.push( p_tmp );
	auto p_tmp2 = sut.pop();

	// Assert
	EXPECT_NE( p_tmp2, nullptr );
	EXPECT_EQ( p_tmp2, p_tmp );

	delete p_tmp2;
}

TEST( od_node_pool_class, CanPushPop_InHazard1 )
{
	// Arrange
	sut_type                                            sut;
	auto                                                p_tmp = new test_od_node_of_pool;
	typename test_od_node_of_pool::hazard_ptr_handler_t hph( p_tmp );
	{
		auto hp_tmp = hph.get();
		sut.push( p_tmp );

		// Act
		auto p_tmp2 = sut.pop();

		// Assert
		EXPECT_EQ( p_tmp2, nullptr );
	}

	// Cleanup
	delete sut.pop();
}

TEST( od_node_pool_class, CanPushPop_InHazard2 )
{
	// Arrange
	sut_type                                            sut;
	auto                                                p_tmp = new test_od_node_of_pool;
	typename test_od_node_of_pool::hazard_ptr_handler_t hph( p_tmp );
	{
		auto hp_tmp = hph.get();
		sut.push( p_tmp );
		auto p_tmp2 = sut.pop();
		EXPECT_EQ( p_tmp2, nullptr );
	}

	// Act
	auto p_tmp3 = sut.pop();

	// Assert
	EXPECT_EQ( p_tmp3, p_tmp );
	delete p_tmp3;
}

TEST( od_node_pool_class, CanPushPop_InHazard3 )
{
	// Arrange
	sut_type                                            sut;
	auto                                                p_tmp  = new test_od_node_of_pool;
	auto                                                p_tmp2 = new test_od_node_of_pool;
	typename test_od_node_of_pool::hazard_ptr_handler_t hph( p_tmp );

	{
		auto hp_tmp = hph.get();
		sut.push( p_tmp );
		auto p_tmp3 = sut.pop();
		ASSERT_EQ( p_tmp3, nullptr );
		sut.push( p_tmp2 );

		// Act
		auto p_tmp4 = sut.pop();

		// Assert
		EXPECT_EQ( p_tmp4, p_tmp2 );
		delete p_tmp4;
	}

	// Cleanup
	delete sut.pop();
}

TEST( od_node_pool_class, CanPushInOtherThread )
{
	// Arrange
	sut_type    sut;
	auto        p_tmp = new test_od_node_of_pool;
	std::thread t(
		[&sut]( test_od_node_of_pool* p ) {
			sut.push( p );
		},
		p_tmp );
	t.join();

	// Act
	auto p_tmp2 = sut.pop();

	// Assert
	EXPECT_EQ( p_tmp2, p_tmp );
	delete p_tmp2;
}
