/**
 * @file test_conf.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief Test for conf_logger.hpp
 * @version 0.1
 * @date 2022-12-30
 *
 * @copyright Copyright (c) 2022, Teruaki Ata <PFA03027@nifty.com>
 *
 */

#include "alconcurrent/experiment/internal/lf_shared_ptr.hpp"

#include "gtest/gtest.h"

#include "test_derived_class.hpp"

TEST( NTS_WeakPtr_Class, CanConstruct )
{
	// Arrange

	// Act
	alpha::concurrent::internal::nts_weak_ptr<int> sut;

	// Assert
}

TEST( NTS_WeakPtr_Class, Empty_CanLock_Then_ReturnEmptySharedPtr )
{
	// Arrange
	alpha::concurrent::internal::nts_weak_ptr<int> sut;

	// Act
	alpha::concurrent::internal::nts_shared_ptr<int> sp = sut.lock();

	// Assert
	EXPECT_EQ( nullptr, sp.get() );
}

TEST( NTS_WeakPtr_Class, Valid_CanLock_Then_ReturnValidSharedPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut( sp );

	// Act
	alpha::concurrent::internal::nts_shared_ptr<int> sp2 = sut.lock();

	// Assert
	EXPECT_EQ( p, sp2.get() );
}

TEST( NTS_WeakPtr_Class, Valid_Then_Invalidate_Then_CanLock_And_ReturnEmptySharedPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut( sp );

	// Act
	sp.reset();
	alpha::concurrent::internal::nts_shared_ptr<int> sp2 = sut.lock();

	// Assert
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( NTS_WeakPtr_Class, CanConstructFromEmptySharedPtr )
{
	// Arrange
	alpha::concurrent::internal::nts_shared_ptr<int> sp;

	// Act
	alpha::concurrent::internal::nts_weak_ptr<int> sut( sp );

	// Assert
	alpha::concurrent::internal::nts_shared_ptr<int> sp2 = sut.lock();
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( NTS_WeakPtr_Class, CanConstructFromValidSharedPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );

	// Act
	alpha::concurrent::internal::nts_weak_ptr<int> sut( sp );

	// Assert
	alpha::concurrent::internal::nts_shared_ptr<int> sp2 = sut.lock();
	EXPECT_EQ( p, sp2.get() );
}

TEST( NTS_WeakPtr_Class, CanConstructFromEmptyDerivedSharedPtr )
{
	// Arrange
	alpha::concurrent::internal::nts_shared_ptr<test_derived> sp;

	// Act
	alpha::concurrent::internal::nts_weak_ptr<test_base> sut( sp );

	// Assert
	alpha::concurrent::internal::nts_shared_ptr<test_base> sp2 = sut.lock();
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( NTS_WeakPtr_Class, CanConstructFromValidDerivedSharedPtr )
{
	// Arrange
	test_derived*                                             p = new test_derived;
	alpha::concurrent::internal::nts_shared_ptr<test_derived> sp( p );

	// Act
	alpha::concurrent::internal::nts_weak_ptr<test_base> sut( sp );

	// Assert
	alpha::concurrent::internal::nts_shared_ptr<test_base> sp2 = sut.lock();
	EXPECT_EQ( p, sp2.get() );
}

TEST( NTS_WeakPtr_Class, Empty_CanCopyAssignFromEmpty )
{
	// Arrange
	alpha::concurrent::internal::nts_weak_ptr<int> sut1;
	alpha::concurrent::internal::nts_weak_ptr<int> sut2;

	// Act
	sut1 = sut2;

	// Assert
	alpha::concurrent::internal::nts_shared_ptr<int> sp2 = sut1.lock();
	EXPECT_EQ( nullptr, sp2.get() );
	alpha::concurrent::internal::nts_shared_ptr<int> sp3 = sut2.lock();
	EXPECT_EQ( nullptr, sp3.get() );
}

TEST( NTS_WeakPtr_Class, Empty_CanCopyAssignFromValid )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut1;
	alpha::concurrent::internal::nts_weak_ptr<int>   sut2( sp1 );

	// Act
	sut1 = sut2;

	// Assert
	alpha::concurrent::internal::nts_shared_ptr<int> sp2 = sut1.lock();
	EXPECT_EQ( p, sp2.get() );
	alpha::concurrent::internal::nts_shared_ptr<int> sp3 = sut2.lock();
	EXPECT_EQ( p, sp3.get() );
}

TEST( NTS_WeakPtr_Class, Valid_CanCopyAssignFromEmpty )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut1( sp1 );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut2;

	// Act
	sut1 = sut2;

	// Assert
	alpha::concurrent::internal::nts_shared_ptr<int> sp2 = sut1.lock();
	EXPECT_EQ( nullptr, sp2.get() );
	alpha::concurrent::internal::nts_shared_ptr<int> sp3 = sut2.lock();
	EXPECT_EQ( nullptr, sp3.get() );
}

TEST( NTS_WeakPtr_Class, Valid_CanCopyAssignFromValid )
{
	// Arrange
	int*                                             p1 = new int;
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p2 );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut1( sp1 );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut2( sp2 );

	// Act
	sut1 = sut2;

	// Assert
	alpha::concurrent::internal::nts_shared_ptr<int> sp3 = sut1.lock();
	EXPECT_EQ( p2, sp3.get() );
	alpha::concurrent::internal::nts_shared_ptr<int> sp4 = sut2.lock();
	EXPECT_EQ( p2, sp4.get() );
}

TEST( NTS_WeakPtr_Class, Empty_CanMoveAssignFromEmpty )
{
	// Arrange
	alpha::concurrent::internal::nts_weak_ptr<int> sut1;
	alpha::concurrent::internal::nts_weak_ptr<int> sut2;

	// Act
	sut1 = std::move( sut2 );

	// Assert
	alpha::concurrent::internal::nts_shared_ptr<int> sp2 = sut1.lock();
	EXPECT_EQ( nullptr, sp2.get() );
	alpha::concurrent::internal::nts_shared_ptr<int> sp3 = sut2.lock();
	EXPECT_EQ( nullptr, sp3.get() );
}

TEST( NTS_WeakPtr_Class, Empty_CanMoveAssignFromValid )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut1;
	alpha::concurrent::internal::nts_weak_ptr<int>   sut2( sp1 );

	// Act
	sut1 = std::move( sut2 );

	// Assert
	alpha::concurrent::internal::nts_shared_ptr<int> sp2 = sut1.lock();
	EXPECT_EQ( p, sp2.get() );
	alpha::concurrent::internal::nts_shared_ptr<int> sp3 = sut2.lock();
	EXPECT_EQ( nullptr, sp3.get() );
}

TEST( NTS_WeakPtr_Class, Valid_CanMoveAssignFromEmpty )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut1( sp1 );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut2;

	// Act
	sut1 = std::move( sut2 );

	// Assert
	alpha::concurrent::internal::nts_shared_ptr<int> sp2 = sut1.lock();
	EXPECT_EQ( nullptr, sp2.get() );
	alpha::concurrent::internal::nts_shared_ptr<int> sp3 = sut2.lock();
	EXPECT_EQ( nullptr, sp3.get() );
}

TEST( NTS_WeakPtr_Class, Valid_CanMoveAssignFromValid )
{
	// Arrange
	int*                                             p1 = new int;
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p2 );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut1( sp1 );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut2( sp2 );

	// Act
	sut1 = std::move( sut2 );

	// Assert
	alpha::concurrent::internal::nts_shared_ptr<int> sp3 = sut1.lock();
	EXPECT_EQ( p2, sp3.get() );
	alpha::concurrent::internal::nts_shared_ptr<int> sp4 = sut2.lock();
	EXPECT_EQ( nullptr, sp4.get() );
}

TEST( NTS_WeakPtr_Class, Empty_CanAssignFromEmtpySharedPtr )
{
	// Arrange
	alpha::concurrent::internal::nts_shared_ptr<int> sp;
	alpha::concurrent::internal::nts_weak_ptr<int>   sut;

	// Act
	sut = sp;

	// Assert
	alpha::concurrent::internal::nts_shared_ptr<int> sp2 = sut.lock();
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( NTS_WeakPtr_Class, Empty_CanAssignFromValidSharedPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut;

	// Act
	sut = sp;

	// Assert
	alpha::concurrent::internal::nts_shared_ptr<int> sp2 = sut.lock();
	EXPECT_EQ( p, sp2.get() );
}

TEST( NTS_WeakPtr_Class, Valid_CanAssignFromEmptySharedPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut( sp1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sp2;

	// Act
	sut = sp2;

	// Assert
	alpha::concurrent::internal::nts_shared_ptr<int> sp3 = sut.lock();
	EXPECT_EQ( nullptr, sp3.get() );
}

TEST( NTS_WeakPtr_Class, Valid_CanAssignFromValidSharedPtr )
{
	// Arrange
	int*                                             p1 = new int;
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p2 );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut( sp1 );

	// Act
	sut = sp2;

	// Assert
	alpha::concurrent::internal::nts_shared_ptr<int> sp3 = sut.lock();
	EXPECT_EQ( p2, sp3.get() );
}

TEST( NTS_WeakPtr_Class, Empty_CanExpired_Then_ReturnTrue )
{
	// Arrange
	alpha::concurrent::internal::nts_weak_ptr<int> sut;

	// Act
	bool result = sut.expired();

	// Assert
	EXPECT_TRUE( result );
}

TEST( NTS_WeakPtr_Class, Valid_CanExpired_Then_ReturnFalse )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut( sp );

	// Act
	bool result = sut.expired();

	// Assert
	EXPECT_FALSE( result );
}

TEST( NTS_WeakPtr_Class, Valid_Then_Invalidate_Then_CanExpired_Then_ReturnTrue )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut( sp );

	// Act
	sp.reset();
	bool result = sut.expired();

	// Assert
	EXPECT_TRUE( result );
}

TEST( NTS_WeakPtr_Class, Empty_CanReset_Then_ExpiredReturnTrue )
{
	// Arrange
	alpha::concurrent::internal::nts_weak_ptr<int> sut;

	// Act
	sut.reset();

	// Assert
	bool result = sut.expired();
	EXPECT_TRUE( result );
}

TEST( NTS_WeakPtr_Class, Valid_CanReset_Then_ExpiredReturnTrue )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut( sp );

	// Act
	sut.reset();

	// Assert
	bool result = sut.expired();
	EXPECT_TRUE( result );
}

TEST( NTS_WeakPtr_Class, CanSwap )
{
	// Arrange
	int*                                             p1 = new int;
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p2 );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut1( sp1 );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut2( sp2 );

	// Act
	sut1.swap( sut2 );

	// Assert
	alpha::concurrent::internal::nts_shared_ptr<int> sp3 = sut1.lock();
	EXPECT_EQ( p2, sp3.get() );
	alpha::concurrent::internal::nts_shared_ptr<int> sp4 = sut2.lock();
	EXPECT_EQ( p1, sp4.get() );
}
