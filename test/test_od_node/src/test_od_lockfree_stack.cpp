/**
 * @file test_od_lockfree_stack.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-10-20
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "gtest/gtest.h"

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/internal/od_lockfree_stack.hpp"

using namespace alpha::concurrent::internal;
/////////////////////////////////////////////////////////////////////////////////

class test_node_type_by_hazard_handler : public od_node_link_by_hazard_handler {
public:
	test_node_type_by_hazard_handler( void )
	  : p_dummy( new int )
	{
	}
	~test_node_type_by_hazard_handler()
	{
		delete p_dummy;
	}

private:
	int* p_dummy;
};

/////////////////////////////////////////////////////////////////////////////////
class TestOdLockFreeStack : public ::testing::Test {
protected:
	void SetUp() override
	{
		alpha::concurrent::GetErrorWarningLogCountAndReset( nullptr, nullptr );
	}

	void TearDown() override
	{
		// alpha::concurrent::internal::retire_mgr::stop_prune_thread();
		// alpha::concurrent::internal::hazard_ptr_mgr::DestoryAll();

		int cw, ce;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
		EXPECT_EQ( ce, 0 );
		EXPECT_EQ( cw, 0 );
	}
};

TEST( TestOdLockFreeStack, CanDefaultConstruct )
{
	// Arrange

	// Act
	od_lockfree_stack sut;

	// Assert
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_EQ( sut.pop_front(), nullptr );
}

TEST( TestOdLockFreeStack, DoesNotAllow )
{
	static_assert( !std::is_copy_constructible<od_lockfree_stack>::value, "does not allow copy constructor" );
	static_assert( !std::is_copy_assignable<od_lockfree_stack>::value, "does not allow copy assignable" );
	static_assert( !std::is_move_assignable<od_lockfree_stack>::value, "does not allow move assignable" );
}

TEST( TestOdLockFreeStack, CanMoveConstruct0 )
{
	// Arrange
	od_lockfree_stack src;
	EXPECT_EQ( src.pop_front(), nullptr );

	// Act
	od_lockfree_stack sut( std::move( src ) );

	// Assert
	EXPECT_TRUE( src.is_empty() );
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_EQ( sut.pop_front(), nullptr );
	EXPECT_EQ( src.pop_front(), nullptr );
}

TEST( TestOdLockFreeStack, CanMoveConstruct1 )
{
	// Arrange
	od_lockfree_stack src;
	src.push_front( new test_node_type_by_hazard_handler );
	EXPECT_FALSE( src.is_empty() );

	// Act
	od_lockfree_stack sut( std::move( src ) );

	// Assert
	EXPECT_TRUE( src.is_empty() );
	EXPECT_FALSE( sut.is_empty() );
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	EXPECT_EQ( sut.pop_front(), nullptr );
	EXPECT_EQ( src.pop_front(), nullptr );
}
