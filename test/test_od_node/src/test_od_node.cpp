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

#include <type_traits>

#include "gtest/gtest.h"

#include "alconcurrent/internal/free_node_storage.hpp"

///////////////////////////////////////////////////////////////////////////////////

TEST( od_node_class, CanConstructWithDefaultConstructible )
{
	// Arrange
	struct X {
		int a;

		X( void )
		  : a( 10 ) {}
	};

	// Act
	alpha::concurrent::internal::od_node<X> sut( nullptr );

	// Assert
	EXPECT_EQ( sut.get().a, 10 );
}

TEST( od_node_class, CanConstructWithCopyable )
{
	// Arrange
	alpha::concurrent::internal::od_node<int> yy( nullptr, 0 );
	int                                       x = 1;

	// Act
	alpha::concurrent::internal::od_node<int> sut( &yy, x );

	// Assert
	EXPECT_EQ( sut.get(), x );
	auto p_x = sut.hph_next_.load();
	EXPECT_EQ( p_x, &yy );
}

TEST( od_node_class, CanConstructWithCopyableRref )
{
	// Arrange
	int x = 1;

	// Act
	alpha::concurrent::internal::od_node<int> sut( nullptr, std::move( x ) );

	// Assert
	EXPECT_EQ( sut.get(), x );
	auto p_x = sut.hph_next_.load();
	EXPECT_EQ( p_x, nullptr );
}

TEST( od_node_class, CanConstructWithMovableOnly )
{
	// Arrange
	int*                 target_pointer_value = new int {};
	std::unique_ptr<int> up_x( target_pointer_value );

	// Act
	alpha::concurrent::internal::od_node<std::unique_ptr<int>> sut( nullptr, std::move( up_x ) );

	// Assert
	EXPECT_EQ( sut.get().get(), target_pointer_value );
	auto p_x = sut.hph_next_.load();
	EXPECT_EQ( p_x, nullptr );
}

TEST( od_node_class, CanConstructWithMultArgs )
{
	// Arrange
	struct X {
		int a;
		int b;

		X( int a_arg, int b_arg )
		  : a( a_arg )
		  , b( b_arg )
		{
		}
	};

	// Act
	alpha::concurrent::internal::od_node<X> sut( nullptr, 10, 11 );

	// Assert
	EXPECT_EQ( sut.get().a, 10 );
	EXPECT_EQ( sut.get().b, 11 );
}

TEST( od_node_class, CanGetWithMovableOnly )
{
	// Arrange
	int*                                                       target_pointer_value = new int {};
	std::unique_ptr<int>                                       up_x( target_pointer_value );
	alpha::concurrent::internal::od_node<std::unique_ptr<int>> sut( nullptr, std::move( up_x ) );

	// Act
	std::unique_ptr<int> up_ret = std::move( sut.get() );

	// Assert
	EXPECT_EQ( up_ret.get(), target_pointer_value );
	auto p_x = sut.hph_next_.load();
	EXPECT_EQ( p_x, nullptr );
}

TEST( od_node_class, CanRvalueGetWithCopyable )
{
	// Arrange

	// Act
	int ret = alpha::concurrent::internal::od_node<int>( nullptr, 1 ).get();

	// Assert
	EXPECT_EQ( ret, 1 );
}

TEST( od_node_class, CanRvalueGetWithMovableOnly )
{
	// Arrange
	int*                 target_pointer_value = new int {};
	std::unique_ptr<int> up_x( target_pointer_value );

	// Act
	std::unique_ptr<int> up_ret = alpha::concurrent::internal::od_node<std::unique_ptr<int>>( nullptr, std::move( up_x ) ).get();

	// Assert
	EXPECT_EQ( up_ret.get(), target_pointer_value );
}

TEST( od_node_class, CanRvalueGetWithCopyableOnly )
{
	// Arrange
	struct X {
		int a;

		X( int a_a )
		  : a( a_a ) {}
		X( void )                = default;
		X( const X& )            = default;
		X( X&& )                 = delete;
		X& operator=( const X& ) = default;
		X& operator=( X&& )      = delete;
	};

	// Act
	X ret = alpha::concurrent::internal::od_node<X>( nullptr, X { 1 } ).get();

	// Assert
	EXPECT_EQ( ret.a, 1 );
}

TEST( od_node_class, CanSetWithCopyable )
{
	// Arrange
	alpha::concurrent::internal::od_node<int> yy( nullptr, 0 );
	alpha::concurrent::internal::od_node<int> sut( nullptr, 1 );
	EXPECT_EQ( sut.get(), 1 );
	auto p_x = sut.hph_next_.load();
	EXPECT_EQ( p_x, nullptr );
	int x = 2;

	// Act
	sut.set( x, &yy );

	// Assert
	EXPECT_EQ( sut.get(), 2 );
	p_x = sut.hph_next_.load();
	EXPECT_EQ( p_x, &yy );
}

TEST( od_node_class, CanSetWithMovable )
{
	// Arrange
	alpha::concurrent::internal::od_node<std::unique_ptr<int>> yy( nullptr, std::unique_ptr<int>( new int {} ) );

	int*                                                       target_pointer_value = new int {};
	std::unique_ptr<int>                                       up_x( target_pointer_value );
	alpha::concurrent::internal::od_node<std::unique_ptr<int>> sut( nullptr, std::move( up_x ) );
	EXPECT_EQ( sut.get().get(), target_pointer_value );
	auto p_x = sut.hph_next_.load();
	EXPECT_EQ( p_x, nullptr );

	int*                 target_pointer_value2 = new int {};
	std::unique_ptr<int> up_x2( target_pointer_value2 );

	// Act
	sut.set( std::move( up_x2 ), &yy );

	// Assert
	EXPECT_EQ( sut.get().get(), target_pointer_value2 );
	p_x = sut.hph_next_.load();
	EXPECT_EQ( p_x, &yy );
}

///////////////////////////////////////////////////////////////////////////////////

TEST( od_node_list_class, CanConstruct )
{
	// Arrange

	// Act
	alpha::concurrent::internal::od_node_list<int> sut;

	// Assert
	EXPECT_EQ( sut.pop_front(), nullptr );
}

TEST( od_node_list_class, CanPushFront1 )
{
	// Arrange
	alpha::concurrent::internal::od_node_list<int> sut;

	// Act
	sut.push_front( new alpha::concurrent::internal::od_node<int>( nullptr ) );

	// Assert
	auto p = sut.pop_front();
	EXPECT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_class, CanPushFront2 )
{
	// Arrange
	alpha::concurrent::internal::od_node_list<int> sut;
	sut.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 1 ) );

	// Act
	sut.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 2 ) );

	// Assert
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 2 );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 1 );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_class, DoesNotAllowCopyConstruct )
{
	static_assert( !std::is_copy_constructible<alpha::concurrent::internal::od_node_list<int>>::value, "does not allow copy constructor" );
}

TEST( od_node_list_class, CanMoveConstruct0 )
{
	// Arrange
	alpha::concurrent::internal::od_node_list<int> src;

	// Act
	alpha::concurrent::internal::od_node_list<int> sut( std::move( src ) );

	// Assert
	auto p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_class, CanMoveConstruct1 )
{
	// Arrange
	alpha::concurrent::internal::od_node_list<int> src;
	src.push_front( new alpha::concurrent::internal::od_node<int>( nullptr ) );

	// Act
	alpha::concurrent::internal::od_node_list<int> sut( std::move( src ) );

	// Assert
	auto p = sut.pop_front();
	EXPECT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_class, CanSwap )
{
	// Arrange
	alpha::concurrent::internal::od_node_list<int> src;
	src.push_front( new alpha::concurrent::internal::od_node<int>( nullptr ) );
	alpha::concurrent::internal::od_node_list<int> sut;

	// Act
	sut.swap( src );

	// Assert
	auto p = sut.pop_front();
	EXPECT_NE( p, nullptr );
	delete p;
	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_class, CanMoveAssignment )
{
	// Arrange
	alpha::concurrent::internal::od_node_list<int> src;
	src.push_front( new alpha::concurrent::internal::od_node<int>( nullptr ) );
	alpha::concurrent::internal::od_node_list<int> sut;

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

TEST( od_node_list_class, CanMergePushFrontToEmptyList1 )
{
	// Arrange
	alpha::concurrent::internal::od_node_list<int> src;
	src.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 1 ) );
	src.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 2 ) );
	alpha::concurrent::internal::od_node_list<int> sut;

	// Act
	sut.merge_push_front( std::move( src ) );

	// Assert
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 2 );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 1 );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_class, CanMergePushFrontToEmptyList2 )
{
	// Arrange
	alpha::concurrent::internal::od_node_list<int> src;
	src.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 1 ) );
	src.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 2 ) );
	alpha::concurrent::internal::od_node_list<int> sut;
	sut.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 3 ) );
	sut.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 4 ) );
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 4 );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 3 );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	// Act
	sut.merge_push_front( std::move( src ) );

	// Assert
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 2 );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 1 );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_class, CanPushFrontToEmptyList )
{
	// Arrange
	alpha::concurrent::internal::od_node_list<int> sut;
	sut.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 1 ) );
	sut.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 2 ) );
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 2 );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 1 );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	// Act
	sut.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 3 ) );

	// Assert
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 3 );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_class, CanMergePushFrontToList )
{
	// Arrange
	alpha::concurrent::internal::od_node_list<int> src;
	src.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 1 ) );
	src.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 2 ) );
	alpha::concurrent::internal::od_node_list<int> sut;
	sut.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 3 ) );

	// Act
	sut.merge_push_front( std::move( src ) );

	// Assert
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 2 );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 1 );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 3 );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_class, CanMergePushBackToEmptyList1 )
{
	// Arrange
	alpha::concurrent::internal::od_node_list<int> src;
	src.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 1 ) );
	src.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 2 ) );
	alpha::concurrent::internal::od_node_list<int> sut;

	// Act
	sut.merge_push_back( std::move( src ) );

	// Assert
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 2 );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 1 );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_class, CanMergePushBackToEmptyList2 )
{
	// Arrange
	alpha::concurrent::internal::od_node_list<int> src;
	src.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 1 ) );
	src.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 2 ) );
	alpha::concurrent::internal::od_node_list<int> sut;
	sut.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 3 ) );
	sut.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 4 ) );
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 4 );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 3 );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	// Act
	sut.merge_push_back( std::move( src ) );

	// Assert
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 2 );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 1 );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_class, CanPushBackToEmptyList )
{
	// Arrange
	alpha::concurrent::internal::od_node_list<int> sut;
	sut.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 1 ) );
	sut.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 2 ) );
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 2 );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 1 );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	// Act
	sut.push_back( new alpha::concurrent::internal::od_node<int>( nullptr, 3 ) );

	// Assert
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 3 );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST( od_node_list_class, CanMergePushBackToList )
{
	// Arrange
	alpha::concurrent::internal::od_node_list<int> src;
	src.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 1 ) );
	src.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 2 ) );
	alpha::concurrent::internal::od_node_list<int> sut;
	sut.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 3 ) );

	// Act
	sut.merge_push_back( std::move( src ) );

	// Assert
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 3 );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 2 );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 1 );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

///////////////////////////////////////////////////////////////////////////////////

TEST( od_node_list_lockfree_class, CanDefaultConstruct )
{
	// Arrange

	// Act
	alpha::concurrent::internal::od_node_list_lockfree<int> sut;

	// Assert
	EXPECT_EQ( sut.pop_front(), nullptr );
}

TEST( od_node_list_lockfree_class, DoesNotAllow )
{
	static_assert( !std::is_copy_constructible<alpha::concurrent::internal::od_node_list_lockfree<int>>::value, "does not allow copy constructor" );
	static_assert( !std::is_copy_assignable<alpha::concurrent::internal::od_node_list_lockfree<int>>::value, "does not allow copy assignable" );
	static_assert( !std::is_move_assignable<alpha::concurrent::internal::od_node_list_lockfree<int>>::value, "does not allow move assignable" );
}

TEST( od_node_list_lockfree_class, CanMoveConstruct0 )
{
	// Arrange
	alpha::concurrent::internal::od_node_list_lockfree<int> src;
	EXPECT_EQ( src.pop_front(), nullptr );

	// Act
	alpha::concurrent::internal::od_node_list_lockfree<int> sut( std::move( src ) );

	// Assert
	EXPECT_EQ( sut.pop_front(), nullptr );
	EXPECT_EQ( src.pop_front(), nullptr );
}

TEST( od_node_list_lockfree_class, CanMoveConstruct1 )
{
	// Arrange
	alpha::concurrent::internal::od_node_list_lockfree<int> src;
	src.push_front( new alpha::concurrent::internal::od_node<int>( nullptr, 1 ) );

	// Act
	alpha::concurrent::internal::od_node_list_lockfree<int> sut( std::move( src ) );

	// Assert
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	EXPECT_EQ( p->get(), 1 );
	delete p;
	EXPECT_EQ( sut.pop_front(), nullptr );
	EXPECT_EQ( src.pop_front(), nullptr );
}
