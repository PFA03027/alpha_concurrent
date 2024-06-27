/**
 * @file test_od_node_base.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-26
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include <type_traits>

#include "gtest/gtest.h"

#include "alconcurrent/internal/od_node_base.hpp"

///////////////////////////////////////////////////////////////////////////////////
class test_od_node_base : public alpha::concurrent::internal::od_node_base<test_od_node_base> {
public:
	test_od_node_base( test_od_node_base* p_arg )
	  : alpha::concurrent::internal::od_node_base<test_od_node_base>( p_arg ) {}
};

TEST( od_node_base_class, CanConstructWithNullptr )
{
	// Arrange

	// Act
	test_od_node_base sut( nullptr );

	// Assert
	EXPECT_EQ( sut.hph_next_.load(), nullptr );
}

TEST( od_node_base_class, CanConstruct )
{
	// Arrange
	test_od_node_base yy( nullptr );

	// Act
	test_od_node_base sut( &yy );

	// Assert
	auto p_x = sut.hph_next_.load();
	EXPECT_EQ( p_x, &yy );
}

///////////////////////////////////////////////////////////////////////////////////
class test_od_node_list_base : public alpha::concurrent::internal::od_node_list_base<test_od_node_base> {
};

TEST( od_node_list_base_class, CanConstruct )
{
	// Arrange

	// Act
	test_od_node_list_base sut;

	// Assert
	EXPECT_EQ( sut.pop_front(), nullptr );
}

TEST( od_node_list_base_class, CanPushFront1 )
{
	// Arrange
	test_od_node_list_base sut;

	// Act
	sut.push_front( new test_od_node_base( nullptr ) );

	// Assert
	auto p = sut.pop_front();
	EXPECT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_base_class, CanPushFront2 )
{
	// Arrange
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );

	// Act
	sut.push_front( new test_od_node_base( nullptr ) );

	// Assert
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_base_class, CanPop1 )
{
	// Arrange
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );

	// Act
	auto p = sut.pop_front();

	// Assert
	EXPECT_NE( p, nullptr );
	EXPECT_EQ( p->hph_next_.load(), nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_base_class, CanPop2 )
{
	// Arrange
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );
	sut.push_front( new test_od_node_base( nullptr ) );

	// Act
	auto p = sut.pop_front();

	// Assert
	EXPECT_NE( p, nullptr );
	EXPECT_EQ( p->hph_next_.load(), nullptr );
	delete p;
}

TEST( od_node_list_base_class, DoesNotAllowCopyConstruct )
{
	static_assert( !std::is_copy_constructible<test_od_node_list_base>::value, "does not allow copy constructor" );
}

TEST( od_node_list_base_class, CanMoveConstruct0 )
{
	// Arrange
	test_od_node_list_base src;

	// Act
	test_od_node_list_base sut( std::move( src ) );

	// Assert
	auto p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_base_class, CanMoveConstruct1 )
{
	// Arrange
	test_od_node_list_base src;
	src.push_front( new test_od_node_base( nullptr ) );

	// Act
	test_od_node_list_base sut( std::move( src ) );

	// Assert
	auto p = sut.pop_front();
	EXPECT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_base_class, CanSwap )
{
	// Arrange
	test_od_node_list_base src;
	src.push_front( new test_od_node_base( nullptr ) );
	test_od_node_list_base sut;

	// Act
	sut.swap( src );

	// Assert
	auto p = sut.pop_front();
	EXPECT_NE( p, nullptr );
	delete p;
	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_base_class, CanMoveAssignment )
{
	// Arrange
	test_od_node_list_base src;
	src.push_front( new test_od_node_base( nullptr ) );
	test_od_node_list_base sut;

	// Act
	sut = std::move( src );

	// Assert
	auto p = sut.pop_front();
	EXPECT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_base_class, CanMergePushFrontToEmptyList1 )
{
	// Arrange
	test_od_node_list_base src;
	src.push_front( new test_od_node_base( nullptr ) );
	src.push_front( new test_od_node_base( nullptr ) );
	test_od_node_list_base sut;

	// Act
	sut.merge_push_front( std::move( src ) );

	// Assert
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_base_class, CanMergePushFrontToEmptyList2 )
{
	// Arrange
	test_od_node_list_base src;
	src.push_front( new test_od_node_base( nullptr ) );
	src.push_front( new test_od_node_base( nullptr ) );
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );
	sut.push_front( new test_od_node_base( nullptr ) );
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	// Act
	sut.merge_push_front( std::move( src ) );

	// Assert
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_base_class, CanPushFrontToEmptyList )
{
	// Arrange
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );
	sut.push_front( new test_od_node_base( nullptr ) );
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	// Act
	sut.push_front( new test_od_node_base( nullptr ) );

	// Assert
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_base_class, CanMergePushFrontToList )
{
	// Arrange
	test_od_node_list_base src;
	src.push_front( new test_od_node_base( nullptr ) );
	src.push_front( new test_od_node_base( nullptr ) );
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );

	// Act
	sut.merge_push_front( std::move( src ) );

	// Assert
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_base_class, CanMergePushBackToEmptyList1 )
{
	// Arrange
	test_od_node_list_base src;
	src.push_front( new test_od_node_base( nullptr ) );
	src.push_front( new test_od_node_base( nullptr ) );
	test_od_node_list_base sut;

	// Act
	sut.merge_push_back( std::move( src ) );

	// Assert
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_base_class, CanMergePushBackToEmptyList2 )
{
	// Arrange
	test_od_node_list_base src;
	src.push_front( new test_od_node_base( nullptr ) );
	src.push_front( new test_od_node_base( nullptr ) );
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );
	sut.push_front( new test_od_node_base( nullptr ) );
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	// Act
	sut.merge_push_back( std::move( src ) );

	// Assert
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_base_class, CanPushBackToEmptyList )
{
	// Arrange
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );
	sut.push_front( new test_od_node_base( nullptr ) );
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	// Act
	sut.push_back( new test_od_node_base( nullptr ) );

	// Assert
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_base_class, CanMergePushBackToList )
{
	// Arrange
	test_od_node_list_base src;
	src.push_front( new test_od_node_base( nullptr ) );
	src.push_front( new test_od_node_base( nullptr ) );
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );

	// Act
	sut.merge_push_back( std::move( src ) );

	// Assert
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_base_class, CanClearWithEmpty )
{
	// Arrange
	test_od_node_list_base sut;
	EXPECT_TRUE( sut.is_empty() );

	// Act
	sut.clear();

	// Assert
	EXPECT_TRUE( sut.is_empty() );
}

TEST( od_node_list_base_class, CanClear )
{
	// Arrange
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );
	EXPECT_FALSE( sut.is_empty() );

	// Act
	sut.clear();

	// Assert
	EXPECT_TRUE( sut.is_empty() );
}

TEST( od_node_list_base_class, CanSplitWithEmpty1 )
{
	// Arrange
	test_od_node_list_base sut;

	// Act
	auto ret = sut.split_if( []( auto& ) -> bool { return true; } );

	// Assert
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_TRUE( ret.is_empty() );
}

TEST( od_node_list_base_class, CanSplitWithEmpty2 )
{
	// Arrange
	test_od_node_list_base sut;

	// Act
	auto ret = sut.split_if( []( auto& ) -> bool { return false; } );

	// Assert
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_TRUE( ret.is_empty() );
}

TEST( od_node_list_base_class, CanSplitWithAllTrue1 )
{
	// Arrange
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );

	// Act
	auto ret = sut.split_if( []( auto& ) -> bool { return true; } );

	// Assert
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_FALSE( ret.is_empty() );
}

TEST( od_node_list_base_class, CanSplitWithAllTrue2 )
{
	// Arrange
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );
	sut.push_front( new test_od_node_base( nullptr ) );

	// Act
	auto ret = sut.split_if( []( auto& ) -> bool { return true; } );

	// Assert
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_FALSE( ret.is_empty() );
}

TEST( od_node_list_base_class, CanSplitWithAllTrue3 )
{
	// Arrange
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );
	sut.push_front( new test_od_node_base( nullptr ) );
	sut.push_front( new test_od_node_base( nullptr ) );

	// Act
	auto ret = sut.split_if( []( auto& ) -> bool { return true; } );

	// Assert
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_FALSE( ret.is_empty() );
}

TEST( od_node_list_base_class, CanSplitWithAllFalse1 )
{
	// Arrange
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );

	// Act
	auto ret = sut.split_if( []( auto& ) -> bool { return false; } );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_TRUE( ret.is_empty() );
}

TEST( od_node_list_base_class, CanSplitWithAllFalse2 )
{
	// Arrange
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );
	sut.push_front( new test_od_node_base( nullptr ) );

	// Act
	auto ret = sut.split_if( []( auto& ) -> bool { return false; } );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_TRUE( ret.is_empty() );
}

TEST( od_node_list_base_class, CanSplitWithAllFalse3 )
{
	// Arrange
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );
	sut.push_front( new test_od_node_base( nullptr ) );
	sut.push_front( new test_od_node_base( nullptr ) );

	// Act
	auto ret = sut.split_if( []( auto& ) -> bool { return false; } );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_TRUE( ret.is_empty() );
}

TEST( od_node_list_base_class, CanSplitWithOddTrue1 )
{
	// Arrange
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );
	int x = 0;

	// Act
	auto ret = sut.split_if( [&x]( auto& ) -> bool { x++; return ((x%2) == 1); } );

	// Assert
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_FALSE( ret.is_empty() );
}

TEST( od_node_list_base_class, CanSplitWithOddTrue2 )
{
	// Arrange
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );
	sut.push_front( new test_od_node_base( nullptr ) );
	int x = 0;

	// Act
	auto ret = sut.split_if( [&x]( auto& ) -> bool { x++; return ((x%2) == 1); } );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_FALSE( ret.is_empty() );
}

TEST( od_node_list_base_class, CanSplitWithOddTrue3 )
{
	// Arrange
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );
	sut.push_front( new test_od_node_base( nullptr ) );
	sut.push_front( new test_od_node_base( nullptr ) );
	int x = 0;

	// Act
	auto ret = sut.split_if( [&x]( auto& ) -> bool { x++; return ((x%2) == 1); } );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_FALSE( ret.is_empty() );
	delete sut.pop_front();
	EXPECT_TRUE( sut.is_empty() );
}

TEST( od_node_list_base_class, CanSplitWithEvenTrue1 )
{
	// Arrange
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );
	int x = 1;

	// Act
	auto ret = sut.split_if( [&x]( auto& ) -> bool { x++; return ((x%2) == 1); } );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_TRUE( ret.is_empty() );
}

TEST( od_node_list_base_class, CanSplitWithEvenTrue2 )
{
	// Arrange
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );
	sut.push_front( new test_od_node_base( nullptr ) );
	int x = 1;

	// Act
	auto ret = sut.split_if( [&x]( auto& ) -> bool { x++; return ((x%2) == 1); } );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_FALSE( ret.is_empty() );
}

TEST( od_node_list_base_class, CanSplitWithEvenTrue3 )
{
	// Arrange
	test_od_node_list_base sut;
	sut.push_front( new test_od_node_base( nullptr ) );
	sut.push_front( new test_od_node_base( nullptr ) );
	sut.push_front( new test_od_node_base( nullptr ) );
	int x = 1;

	// Act
	auto ret = sut.split_if( [&x]( auto& ) -> bool { x++; return ((x%2) == 1); } );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_FALSE( ret.is_empty() );
	delete ret.pop_front();
	EXPECT_TRUE( ret.is_empty() );
}

///////////////////////////////////////////////////////////////////////////////////
class test_od_node_list_lockfree_base : public alpha::concurrent::internal::od_node_stack_lockfree_base<test_od_node_base> {
};

TEST( od_node_list_lockfree_base_class, CanDefaultConstruct )
{
	// Arrange

	// Act
	test_od_node_list_lockfree_base sut;

	// Assert
	EXPECT_EQ( sut.pop_front(), nullptr );
}

TEST( od_node_list_lockfree_base_class, DoesNotAllow )
{
	static_assert( !std::is_copy_constructible<test_od_node_list_lockfree_base>::value, "does not allow copy constructor" );
	static_assert( !std::is_copy_assignable<test_od_node_list_lockfree_base>::value, "does not allow copy assignable" );
	static_assert( !std::is_move_assignable<test_od_node_list_lockfree_base>::value, "does not allow move assignable" );
}

TEST( od_node_list_lockfree_base_class, CanMoveConstruct0 )
{
	// Arrange
	test_od_node_list_lockfree_base src;
	EXPECT_EQ( src.pop_front(), nullptr );

	// Act
	test_od_node_list_lockfree_base sut( std::move( src ) );

	// Assert
	EXPECT_EQ( sut.pop_front(), nullptr );
	EXPECT_EQ( src.pop_front(), nullptr );
}

TEST( od_node_list_lockfree_base_class, CanMoveConstruct1 )
{
	// Arrange
	test_od_node_list_lockfree_base src;
	src.push_front( new test_od_node_base( nullptr ) );

	// Act
	test_od_node_list_lockfree_base sut( std::move( src ) );

	// Assert
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	EXPECT_EQ( sut.pop_front(), nullptr );
	EXPECT_EQ( src.pop_front(), nullptr );
}
