/**
 * @file test_od_simple_list.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-10-20
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "gtest/gtest.h"

#include "alconcurrent/internal/od_simple_list.hpp"

using namespace alpha::concurrent::internal;
/////////////////////////////////////////////////////////////////////////////////

class test_node_type : public od_node_simple_link {
public:
	test_node_type( void )
	  : p_dummy( new int )
	{
	}
	~test_node_type()
	{
		delete p_dummy;
	}

private:
	int* p_dummy;
};

/////////////////////////////////////////////////////////////////////////////////
class TestOdSimpleList : public ::testing::Test {
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

TEST_F( TestOdSimpleList, CanConstruct )
{
	// Arrange

	// Act
	od_simple_list sut;

	// Assert
	EXPECT_EQ( sut.pop_front(), nullptr );
}

TEST_F( TestOdSimpleList, CanPushFront_ThenPop_ReturnPointerKeepOriginalType )
{
	// Arrange
	od_simple_list sut;
	test_node_type td_dummy;

	// Act
	sut.push_front( &td_dummy );

	// Assert
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );

	test_node_type* p_orig = dynamic_cast<test_node_type*>( p );
	ASSERT_NE( p_orig, nullptr );
	EXPECT_EQ( p_orig, &td_dummy );
}

TEST_F( TestOdSimpleList, CanPushFront1 )
{
	// Arrange
	od_simple_list sut;

	// Act
	sut.push_front( new test_node_type );

	// Assert
	auto p = sut.pop_front();
	EXPECT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST_F( TestOdSimpleList, CanPushFront2 )
{
	// Arrange
	od_simple_list sut;
	sut.push_front( new test_node_type );

	// Act
	sut.push_front( new test_node_type );

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

TEST_F( TestOdSimpleList, CanPop1 )
{
	// Arrange
	od_simple_list sut;
	sut.push_front( new test_node_type );

	// Act
	auto p = sut.pop_front();

	// Assert
	EXPECT_NE( p, nullptr );
	EXPECT_EQ( p->next(), nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST_F( TestOdSimpleList, CanPop2 )
{
	// Arrange
	od_simple_list sut;
	sut.push_front( new test_node_type );
	sut.push_front( new test_node_type );

	// Act
	auto p = sut.pop_front();

	// Assert
	EXPECT_NE( p, nullptr );
	EXPECT_EQ( p->next(), nullptr );
	delete p;
}

TEST_F( TestOdSimpleList, DoesNotAllowCopyConstruct )
{
	static_assert( !std::is_copy_constructible<od_simple_list>::value, "does not allow copy constructor" );
}

TEST_F( TestOdSimpleList, CanMoveConstruct0 )
{
	// Arrange
	od_simple_list src;

	// Act
	od_simple_list sut( std::move( src ) );

	// Assert
	auto p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST_F( TestOdSimpleList, CanMoveConstruct1 )
{
	// Arrange
	od_simple_list src;
	src.push_front( new test_node_type );

	// Act
	od_simple_list sut( std::move( src ) );

	// Assert
	auto p = sut.pop_front();
	EXPECT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST_F( TestOdSimpleList, CanSwap )
{
	// Arrange
	od_simple_list src;
	src.push_front( new test_node_type );
	od_simple_list sut;

	// Act
	sut.swap( src );

	// Assert
	auto p = sut.pop_front();
	EXPECT_NE( p, nullptr );
	delete p;
	p = src.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST_F( TestOdSimpleList, CanMoveAssignment )
{
	// Arrange
	od_simple_list src;
	src.push_front( new test_node_type );
	od_simple_list sut;

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

TEST_F( TestOdSimpleList, CanMergePushFrontToEmptyList1 )
{
	// Arrange
	od_simple_list src;
	src.push_front( new test_node_type );
	src.push_front( new test_node_type );
	od_simple_list sut;

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

TEST_F( TestOdSimpleList, CanMergePushFrontToEmptyList2 )
{
	// Arrange
	od_simple_list src;
	src.push_front( new test_node_type );
	src.push_front( new test_node_type );
	od_simple_list sut;
	sut.push_front( new test_node_type );
	sut.push_front( new test_node_type );
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

TEST_F( TestOdSimpleList, CanPushFrontToEmptyList )
{
	// Arrange
	od_simple_list sut;
	sut.push_front( new test_node_type );
	sut.push_front( new test_node_type );
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	// Act
	sut.push_front( new test_node_type );

	// Assert
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST_F( TestOdSimpleList, CanMergePushFrontToList )
{
	// Arrange
	od_simple_list src;
	src.push_front( new test_node_type );
	src.push_front( new test_node_type );
	od_simple_list sut;
	sut.push_front( new test_node_type );

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

TEST_F( TestOdSimpleList, CanMergePushBackToEmptyList1 )
{
	// Arrange
	od_simple_list src;
	src.push_front( new test_node_type );
	src.push_front( new test_node_type );
	od_simple_list sut;

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

TEST_F( TestOdSimpleList, CanMergePushBackToEmptyList2 )
{
	// Arrange
	od_simple_list src;
	src.push_front( new test_node_type );
	src.push_front( new test_node_type );
	od_simple_list sut;
	sut.push_front( new test_node_type );
	sut.push_front( new test_node_type );
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

TEST_F( TestOdSimpleList, CanPushBackToEmptyList )
{
	// Arrange
	od_simple_list sut;
	sut.push_front( new test_node_type );
	sut.push_front( new test_node_type );
	auto p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );

	// Act
	sut.push_back( new test_node_type );

	// Assert
	p = sut.pop_front();
	ASSERT_NE( p, nullptr );
	delete p;
	p = sut.pop_front();
	EXPECT_EQ( p, nullptr );
}

TEST_F( TestOdSimpleList, CanMergePushBackToList )
{
	// Arrange
	od_simple_list src;
	src.push_front( new test_node_type );
	src.push_front( new test_node_type );
	od_simple_list sut;
	sut.push_front( new test_node_type );

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

TEST_F( TestOdSimpleList, CanClearWithEmpty )
{
	// Arrange
	od_simple_list sut;
	EXPECT_TRUE( sut.is_empty() );

	// Act
	sut.clear();

	// Assert
	EXPECT_TRUE( sut.is_empty() );
}

TEST_F( TestOdSimpleList, CanClear )
{
	// Arrange
	od_simple_list sut;
	sut.push_front( new test_node_type );
	EXPECT_FALSE( sut.is_empty() );

	// Act
	sut.clear();

	// Assert
	EXPECT_TRUE( sut.is_empty() );
}

TEST_F( TestOdSimpleList, CanSplitWithEmpty1 )
{
	// Arrange
	od_simple_list sut;

	// Act
	auto ret = sut.split_if( []( const od_node_simple_link* ) -> bool { return true; } );

	// Assert
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_TRUE( ret.is_empty() );
}

TEST_F( TestOdSimpleList, CanSplitWithEmpty2 )
{
	// Arrange
	od_simple_list sut;

	// Act
	auto ret = sut.split_if( []( const od_node_simple_link* ) -> bool { return false; } );

	// Assert
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_TRUE( ret.is_empty() );
}

TEST_F( TestOdSimpleList, CanSplitWithAllTrue1 )
{
	// Arrange
	od_simple_list sut;
	sut.push_front( new test_node_type );

	// Act
	auto ret = sut.split_if( []( const od_node_simple_link* ) -> bool { return true; } );

	// Assert
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_FALSE( ret.is_empty() );
}

TEST_F( TestOdSimpleList, CanSplitWithAllTrue2 )
{
	// Arrange
	od_simple_list sut;
	sut.push_front( new test_node_type );
	sut.push_front( new test_node_type );

	// Act
	auto ret = sut.split_if( []( const od_node_simple_link* ) -> bool { return true; } );

	// Assert
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_FALSE( ret.is_empty() );
}

TEST_F( TestOdSimpleList, CanSplitWithAllTrue3 )
{
	// Arrange
	od_simple_list sut;
	sut.push_front( new test_node_type );
	sut.push_front( new test_node_type );
	sut.push_front( new test_node_type );

	// Act
	auto ret = sut.split_if( []( const od_node_simple_link* ) -> bool { return true; } );

	// Assert
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_FALSE( ret.is_empty() );
}

TEST_F( TestOdSimpleList, CanSplitWithAllFalse1 )
{
	// Arrange
	od_simple_list sut;
	sut.push_front( new test_node_type );

	// Act
	auto ret = sut.split_if( []( const od_node_simple_link* ) -> bool { return false; } );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_TRUE( ret.is_empty() );
}

TEST_F( TestOdSimpleList, CanSplitWithAllFalse2 )
{
	// Arrange
	od_simple_list sut;
	sut.push_front( new test_node_type );
	sut.push_front( new test_node_type );

	// Act
	auto ret = sut.split_if( []( const od_node_simple_link* ) -> bool { return false; } );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_TRUE( ret.is_empty() );
}

TEST_F( TestOdSimpleList, CanSplitWithAllFalse3 )
{
	// Arrange
	od_simple_list sut;
	sut.push_front( new test_node_type );
	sut.push_front( new test_node_type );
	sut.push_front( new test_node_type );

	// Act
	auto ret = sut.split_if( []( const od_node_simple_link* ) -> bool { return false; } );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_TRUE( ret.is_empty() );
}

TEST_F( TestOdSimpleList, CanSplitWithOddTrue1 )
{
	// Arrange
	od_simple_list sut;
	sut.push_front( new test_node_type );
	int x = 0;

	// Act
	auto ret = sut.split_if( [&x]( const od_node_simple_link* ) -> bool { x++; return ((x%2) == 1); } );

	// Assert
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_FALSE( ret.is_empty() );
}

TEST_F( TestOdSimpleList, CanSplitWithOddTrue2 )
{
	// Arrange
	od_simple_list sut;
	sut.push_front( new test_node_type );
	sut.push_front( new test_node_type );
	int x = 0;

	// Act
	auto ret = sut.split_if( [&x]( const od_node_simple_link* ) -> bool { x++; return ((x%2) == 1); } );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_FALSE( ret.is_empty() );
}

TEST_F( TestOdSimpleList, CanSplitWithOddTrue3 )
{
	// Arrange
	od_simple_list sut;
	sut.push_front( new test_node_type );
	sut.push_front( new test_node_type );
	sut.push_front( new test_node_type );
	int x = 0;

	// Act
	auto ret = sut.split_if( [&x]( const od_node_simple_link* ) -> bool { x++; return ((x%2) == 1); } );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_FALSE( ret.is_empty() );
	delete sut.pop_front();
	EXPECT_TRUE( sut.is_empty() );
}

TEST_F( TestOdSimpleList, CanSplitWithEvenTrue1 )
{
	// Arrange
	od_simple_list sut;
	sut.push_front( new test_node_type );
	int x = 1;

	// Act
	auto ret = sut.split_if( [&x]( const od_node_simple_link* ) -> bool { x++; return ((x%2) == 1); } );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_TRUE( ret.is_empty() );
}

TEST_F( TestOdSimpleList, CanSplitWithEvenTrue2 )
{
	// Arrange
	od_simple_list sut;
	sut.push_front( new test_node_type );
	sut.push_front( new test_node_type );
	int x = 1;

	// Act
	auto ret = sut.split_if( [&x]( const od_node_simple_link* ) -> bool { x++; return ((x%2) == 1); } );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_FALSE( ret.is_empty() );
}

TEST_F( TestOdSimpleList, CanSplitWithEvenTrue3 )
{
	// Arrange
	od_simple_list sut;
	sut.push_front( new test_node_type );
	sut.push_front( new test_node_type );
	sut.push_front( new test_node_type );
	int x = 1;

	// Act
	auto ret = sut.split_if( [&x]( const od_node_simple_link* ) -> bool { x++; return ((x%2) == 1); } );

	// Assert
	EXPECT_FALSE( sut.is_empty() );
	EXPECT_FALSE( ret.is_empty() );
	delete ret.pop_front();
	EXPECT_TRUE( ret.is_empty() );
}

/////////////////////////////////////////////////////////////////////////////////
class TestOdSimpleListLockable : public ::testing::Test {
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

TEST_F( TestOdSimpleListLockable, CanConstruct )
{
	// Arrange

	// Act
	od_simple_list_lockable sut;

	// Assert
	EXPECT_EQ( sut.lock().ref().pop_front(), nullptr );
}

TEST_F( TestOdSimpleListLockable, CanMoveConstruct )
{
	// Arrange
	od_simple_list_lockable src;
	src.lock().ref().push_front( new test_node_type );
	EXPECT_FALSE( src.lock().ref().is_empty() );

	// Act
	od_simple_list_lockable sut( std::move( src ) );

	// Assert
	EXPECT_TRUE( src.lock().ref().is_empty() );
	EXPECT_FALSE( sut.lock().ref().is_empty() );
	auto p = sut.lock().ref().pop_front();
	EXPECT_NE( p, nullptr );
	delete p;
}

TEST_F( TestOdSimpleListLockable, CanMoveAssignment )
{
	// Arrange
	od_simple_list_lockable src;
	src.lock().ref().push_front( new test_node_type );
	EXPECT_FALSE( src.lock().ref().is_empty() );
	od_simple_list_lockable sut;
	EXPECT_TRUE( sut.lock().ref().is_empty() );

	// Act
	sut = std::move( src );

	// Assert
	EXPECT_TRUE( src.lock().ref().is_empty() );
	EXPECT_FALSE( sut.lock().ref().is_empty() );
	auto p = sut.lock().ref().pop_front();
	EXPECT_NE( p, nullptr );
	delete p;
}

TEST_F( TestOdSimpleListLockable, LockAndExclusive )
{
	// Arrange
	od_simple_list_lockable sut;
	auto                    lk1 = sut.lock();
	EXPECT_TRUE( lk1.owns_lock() );

	// Act
	auto lk2 = sut.try_lock();

	// Assert
	EXPECT_FALSE( lk2.owns_lock() );
}

TEST_F( TestOdSimpleListLockable, AccessNoLockObj_Then_ThrowException )
{
	// Arrange
	od_simple_list_lockable sut;
	auto                    lk1 = sut.lock();
	EXPECT_TRUE( lk1.owns_lock() );
	auto lk2 = sut.try_lock();
	EXPECT_FALSE( lk2.owns_lock() );

	// Act
	EXPECT_ANY_THROW( lk2.ref() );

	// Assert
}

TEST_F( TestOdSimpleListLockable, AccessNoLockConstObj_Then_ThrowException )
{
	// Arrange
	od_simple_list_lockable sut;
	auto                    lk1 = sut.lock();
	EXPECT_TRUE( lk1.owns_lock() );
	const auto lk2 = sut.try_lock();
	EXPECT_FALSE( lk2.owns_lock() );

	// Act
	EXPECT_ANY_THROW( lk2.ref() );

	// Assert
}

/////////////////////////////////////////////////////////////////////////////////
class TestOdSimpleListConditionalLockable : public ::testing::Test {
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

TEST_F( TestOdSimpleListConditionalLockable, CanConstruct )
{
	// Arrange

	// Act
	od_simple_list_conditional_lockable sut;

	// Assert
	EXPECT_EQ( sut.lock().ref().pop_front(), nullptr );
}

TEST_F( TestOdSimpleListConditionalLockable, CanMoveConstruct )
{
	// Arrange
	od_simple_list_conditional_lockable src;
	src.lock().ref().push_front( new test_node_type );
	EXPECT_FALSE( src.lock().ref().is_empty() );

	// Act
	od_simple_list_conditional_lockable sut( std::move( src ) );

	// Assert
	EXPECT_TRUE( src.lock().ref().is_empty() );
	EXPECT_FALSE( sut.lock().ref().is_empty() );
	auto p = sut.lock().ref().pop_front();
	EXPECT_NE( p, nullptr );
	delete p;
}

TEST_F( TestOdSimpleListConditionalLockable, CanMoveAssignment )
{
	// Arrange
	od_simple_list_conditional_lockable src;
	src.lock().ref().push_front( new test_node_type );
	EXPECT_FALSE( src.lock().ref().is_empty() );
	od_simple_list_conditional_lockable sut;
	EXPECT_TRUE( sut.lock().ref().is_empty() );

	// Act
	sut = std::move( src );

	// Assert
	EXPECT_TRUE( src.lock().ref().is_empty() );
	EXPECT_FALSE( sut.lock().ref().is_empty() );
	auto p = sut.lock().ref().pop_front();
	EXPECT_NE( p, nullptr );
	delete p;
}

TEST_F( TestOdSimpleListConditionalLockable, LockAndExclusive )
{
	// Arrange
	od_simple_list_conditional_lockable sut;
	auto                                lk1 = sut.lock();
	EXPECT_TRUE( lk1.owns_lock() );

	// Act
	auto lk2 = sut.try_lock();

	// Assert
	EXPECT_FALSE( lk2.owns_lock() );
}

TEST_F( TestOdSimpleListConditionalLockable, AccessNoLockObj_Then_ThrowException )
{
	// Arrange
	od_simple_list_conditional_lockable sut;
	auto                                lk1 = sut.lock();
	EXPECT_TRUE( lk1.owns_lock() );
	auto lk2 = sut.try_lock();
	EXPECT_FALSE( lk2.owns_lock() );

	// Act
	EXPECT_ANY_THROW( lk2.ref() );

	// Assert
}

TEST_F( TestOdSimpleListConditionalLockable, AccessNoLockConstObj_Then_ThrowException )
{
	// Arrange
	od_simple_list_conditional_lockable sut;
	auto                                lk1 = sut.lock();
	EXPECT_TRUE( lk1.owns_lock() );
	const auto lk2 = sut.try_lock();
	EXPECT_FALSE( lk2.owns_lock() );

	// Act
	EXPECT_ANY_THROW( lk2.ref() );

	// Assert
}

TEST_F( TestOdSimpleListConditionalLockable, WaitNoLockObj_Then_ThrowException )
{
	// Arrange
	od_simple_list_conditional_lockable sut;
	auto                                lk1 = sut.lock();
	EXPECT_TRUE( lk1.owns_lock() );
	auto lk2 = sut.try_lock();
	EXPECT_FALSE( lk2.owns_lock() );

	// Act
	EXPECT_ANY_THROW( lk2.wait() );

	// Assert
}

TEST_F( TestOdSimpleListConditionalLockable, TryWaitNoLockObj_Then_ThrowException )
{
	// Arrange
	od_simple_list_conditional_lockable sut;
	auto                                lk1 = sut.lock();
	EXPECT_TRUE( lk1.owns_lock() );
	auto lk2 = sut.try_lock();
	EXPECT_FALSE( lk2.owns_lock() );

	// Act
	EXPECT_ANY_THROW( lk2.wait( []() { return true; } ) );

	// Assert
}
