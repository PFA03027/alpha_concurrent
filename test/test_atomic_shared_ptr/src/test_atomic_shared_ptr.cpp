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

#include "alconcurrent/experiment/internal/atomic_shared_ptr.hpp"

#include "gtest/gtest.h"

TEST( ControlBlock_Class, CanConstruct )
{
	// Arrange
	int* p = new int;

	// Act
	alpha::concurrent::internal::control_block<int> cb( p );

	// Assert
}

TEST( ControlBlock_Class, CanConstructWithDeleter )
{
	// Arrange
	int* p       = new int;
	auto deleter = []( int* p_arg ) { delete p_arg; };

	// Act
	alpha::concurrent::internal::control_block<int, decltype( deleter )> cb( p, deleter );

	// Assert
}

TEST( ControlBlock_Class, CanGetResourcePtr )
{
	// Arrange
	int*                                            p = new int;
	alpha::concurrent::internal::control_block<int> cb( p );

	// Act
	void* ptr = cb.get_resource_ptr();

	// Assert
	EXPECT_EQ( p, ptr );
}

TEST( ControlBlock_Class, CanReleaseResource )
{
	// Arrange
	int*                                            p = new int;
	alpha::concurrent::internal::control_block<int> cb( p );

	// Act
	cb.release_resource();

	// Assert
	EXPECT_EQ( nullptr, cb.get_resource_ptr() );
}

TEST( NTS_SharedPtr_Class, CanConstruct )
{
	// Arrange

	// Act
	alpha::concurrent::internal::nts_shared_ptr<int> sut;

	// Assert
	EXPECT_EQ( nullptr, sut.get() );
}

TEST( NTS_SharedPtr_Class, CanConstructWithPointer )
{
	// Arrange
	int* p = new int;

	// Act
	alpha::concurrent::internal::nts_shared_ptr<int> sut( p );

	// Assert
	EXPECT_EQ( p, sut.get() );
}

TEST( NTS_SharedPtr_Class, CanConstructWithPointerAndDeleter )
{
	// Arrange
	int* p       = new int;
	auto deleter = []( int* p_arg ) { delete p_arg; };

	// Act
	alpha::concurrent::internal::nts_shared_ptr<int> sut( p, deleter );

	// Assert
	EXPECT_EQ( p, sut.get() );
}

TEST( NTS_SharedPtr_Class, CanConstructWithUniquePtr )
{
	// Arrange
	int*                 p = new int;
	std::unique_ptr<int> up( p );

	// Act
	alpha::concurrent::internal::nts_shared_ptr<int> sut( std::move( up ) );

	// Assert
	EXPECT_EQ( p, sut.get() );
}

TEST( NTS_SharedPtr_Class, CanDestruct )
{
	// Arrange
	int* p = new int;

	// Act
	{
		alpha::concurrent::internal::nts_shared_ptr<int> sut( p );
	}

	// Assert
}

TEST( NTS_SharedPtr_Class, CanCopyConstructFromValid )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sut1( p );

	// Act
	alpha::concurrent::internal::nts_shared_ptr<int> sut2( sut1 );

	// Assert
	EXPECT_EQ( p, sut1.get() );
	EXPECT_EQ( p, sut2.get() );
}

TEST( NTS_SharedPtr_Class, CanCopyConstructFromEmpty )
{
	// Arrange
	alpha::concurrent::internal::nts_shared_ptr<int> sut1;

	// Act
	alpha::concurrent::internal::nts_shared_ptr<int> sut2( sut1 );

	// Assert
	EXPECT_EQ( nullptr, sut1.get() );
	EXPECT_EQ( nullptr, sut2.get() );
}

TEST( NTS_SharedPtr_Class, CanMoveConstructFromValid )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sut1( p );

	// Act
	alpha::concurrent::internal::nts_shared_ptr<int> sut2( std::move( sut1 ) );

	// Assert
	EXPECT_EQ( nullptr, sut1.get() );
	EXPECT_EQ( p, sut2.get() );
}

TEST( NTS_SharedPtr_Class, CanMoveConstructFromEmpty )
{
	// Arrange
	alpha::concurrent::internal::nts_shared_ptr<int> sut1;

	// Act
	alpha::concurrent::internal::nts_shared_ptr<int> sut2( std::move( sut1 ) );

	// Assert
	EXPECT_EQ( nullptr, sut1.get() );
	EXPECT_EQ( nullptr, sut2.get() );
}

TEST( NTS_SharedPtr_Class, Empty_CanCopyAssignFromEmpty )
{
	// Arrange
	alpha::concurrent::internal::nts_shared_ptr<int> sut1;
	alpha::concurrent::internal::nts_shared_ptr<int> sut2;

	// Act
	sut1 = sut2;

	// Assert
	EXPECT_EQ( nullptr, sut1.get() );
	EXPECT_EQ( nullptr, sut2.get() );
}

TEST( NTS_SharedPtr_Class, Empty_CanCopyAssignFromValid )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sut1;
	alpha::concurrent::internal::nts_shared_ptr<int> sut2( p );

	// Act
	sut1 = sut2;

	// Assert
	EXPECT_EQ( p, sut1.get() );
}

TEST( NTS_SharedPtr_Class, Valid_CanCopyAssignFromEmpty )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sut1( p );
	alpha::concurrent::internal::nts_shared_ptr<int> sut2;

	// Act
	sut1 = sut2;

	// Assert
	EXPECT_EQ( nullptr, sut1.get() );
	EXPECT_EQ( nullptr, sut2.get() );
}

TEST( NTS_SharedPtr_Class, Valid_CanCopyAssignFromValid )
{
	// Arrange
	int*                                             p1 = new int;
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sut1( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sut2( p2 );

	// Act
	sut1 = sut2;

	// Assert
	EXPECT_EQ( p2, sut1.get() );
	EXPECT_EQ( p2, sut2.get() );
}

TEST( NTS_SharedPtr_Class, Empty_CanMoveAssignFromEmpty )
{
	// Arrange
	alpha::concurrent::internal::nts_shared_ptr<int> sut1;
	alpha::concurrent::internal::nts_shared_ptr<int> sut2;

	// Act
	sut1 = std::move( sut2 );

	// Assert
	EXPECT_EQ( nullptr, sut1.get() );
	EXPECT_EQ( nullptr, sut2.get() );
}

TEST( NTS_SharedPtr_Class, Empty_CanMoveAssignFromValid )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sut1;
	alpha::concurrent::internal::nts_shared_ptr<int> sut2( p );

	// Act
	sut1 = std::move( sut2 );

	// Assert
	EXPECT_EQ( p, sut1.get() );
	EXPECT_EQ( nullptr, sut2.get() );
}

TEST( NTS_SharedPtr_Class, Valid_CanMoveAssignFromEmpty )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sut1( p );
	alpha::concurrent::internal::nts_shared_ptr<int> sut2;

	// Act
	sut1 = std::move( sut2 );

	// Assert
	EXPECT_EQ( nullptr, sut1.get() );
	EXPECT_EQ( nullptr, sut2.get() );
}

TEST( NTS_SharedPtr_Class, Valid_CanMoveAssignFromValid )
{
	// Arrange
	int*                                             p1 = new int;
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sut1( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sut2( p2 );

	// Act
	sut1 = std::move( sut2 );

	// Assert
	EXPECT_EQ( p2, sut1.get() );
	EXPECT_EQ( nullptr, sut2.get() );
}

struct test_base {};
struct test_derived : public test_base {};

TEST( NTS_SharedPtr_Class, CanCopyConstructFromValidDerived )
{
	// Arrange
	test_derived*                                             p = new test_derived;
	alpha::concurrent::internal::nts_shared_ptr<test_derived> sut1( p );

	// Act
	alpha::concurrent::internal::nts_shared_ptr<test_base> sut2( sut1 );

	// Assert
	EXPECT_EQ( p, sut1.get() );
	EXPECT_EQ( p, sut2.get() );
}

TEST( NTS_SharedPtr_Class, CanCopyConstructFromEmptyDerived )
{
	// Arrange
	alpha::concurrent::internal::nts_shared_ptr<test_derived> sut1;

	// Act
	alpha::concurrent::internal::nts_shared_ptr<test_base> sut2( sut1 );

	// Assert
	EXPECT_EQ( nullptr, sut1.get() );
	EXPECT_EQ( nullptr, sut2.get() );
}

TEST( NTS_SharedPtr_Class, CanMoveConstructFromValidDerived )
{
	// Arrange
	test_derived*                                             p = new test_derived;
	alpha::concurrent::internal::nts_shared_ptr<test_derived> sut1( p );

	// Act
	alpha::concurrent::internal::nts_shared_ptr<test_base> sut2( std::move( sut1 ) );

	// Assert
	EXPECT_EQ( nullptr, sut1.get() );
	EXPECT_EQ( p, sut2.get() );
}

TEST( NTS_SharedPtr_Class, CanMoveConstructFromEmptyDerived )
{
	// Arrange
	alpha::concurrent::internal::nts_shared_ptr<test_derived> sut1;

	// Act
	alpha::concurrent::internal::nts_shared_ptr<test_base> sut2( std::move( sut1 ) );

	// Assert
	EXPECT_EQ( nullptr, sut1.get() );
	EXPECT_EQ( nullptr, sut2.get() );
}

TEST( NTS_SharedPtr_Class, Empty_CanCopyAssignFromEmptyDerived )
{
	// Arrange
	alpha::concurrent::internal::nts_shared_ptr<test_base>    sut1;
	alpha::concurrent::internal::nts_shared_ptr<test_derived> sut2;

	// Act
	sut1 = sut2;

	// Assert
	EXPECT_EQ( nullptr, sut1.get() );
	EXPECT_EQ( nullptr, sut2.get() );
}

TEST( NTS_SharedPtr_Class, Empty_CanCopyAssignFromValidDerived )
{
	// Arrange
	test_derived*                                             p = new test_derived;
	alpha::concurrent::internal::nts_shared_ptr<test_base>    sut1;
	alpha::concurrent::internal::nts_shared_ptr<test_derived> sut2( p );

	// Act
	sut1 = sut2;

	// Assert
	EXPECT_EQ( p, sut1.get() );
}

TEST( NTS_SharedPtr_Class, Valid_CanCopyAssignFromEmptyDerived )
{
	// Arrange
	test_derived*                                             p = new test_derived;
	alpha::concurrent::internal::nts_shared_ptr<test_base>    sut1( p );
	alpha::concurrent::internal::nts_shared_ptr<test_derived> sut2;

	// Act
	sut1 = sut2;

	// Assert
	EXPECT_EQ( nullptr, sut1.get() );
	EXPECT_EQ( nullptr, sut2.get() );
}

TEST( NTS_SharedPtr_Class, Valid_CanCopyAssignFromValidDerived )
{
	// Arrange
	test_derived*                                             p1 = new test_derived;
	test_derived*                                             p2 = new test_derived;
	alpha::concurrent::internal::nts_shared_ptr<test_base>    sut1( p1 );
	alpha::concurrent::internal::nts_shared_ptr<test_derived> sut2( p2 );

	// Act
	sut1 = sut2;

	// Assert
	EXPECT_EQ( p2, sut1.get() );
	EXPECT_EQ( p2, sut2.get() );
}

TEST( NTS_SharedPtr_Class, Empty_CanMoveAssignFromEmptyDerived )
{
	// Arrange
	alpha::concurrent::internal::nts_shared_ptr<test_base>    sut1;
	alpha::concurrent::internal::nts_shared_ptr<test_derived> sut2;

	// Act
	sut1 = std::move( sut2 );

	// Assert
	EXPECT_EQ( nullptr, sut1.get() );
	EXPECT_EQ( nullptr, sut2.get() );
}

TEST( NTS_SharedPtr_Class, Empty_CanMoveAssignFromValidDerived )
{
	// Arrange
	test_derived*                                             p = new test_derived;
	alpha::concurrent::internal::nts_shared_ptr<test_base>    sut1;
	alpha::concurrent::internal::nts_shared_ptr<test_derived> sut2( p );

	// Act
	sut1 = std::move( sut2 );

	// Assert
	EXPECT_EQ( p, sut1.get() );
	EXPECT_EQ( nullptr, sut2.get() );
}

TEST( NTS_SharedPtr_Class, Valid_CanMoveAssignFromEmptyDerived )
{
	// Arrange
	test_derived*                                             p = new test_derived;
	alpha::concurrent::internal::nts_shared_ptr<test_base>    sut1( p );
	alpha::concurrent::internal::nts_shared_ptr<test_derived> sut2;

	// Act
	sut1 = std::move( sut2 );

	// Assert
	EXPECT_EQ( nullptr, sut1.get() );
	EXPECT_EQ( nullptr, sut2.get() );
}

TEST( NTS_SharedPtr_Class, Valid_CanMoveAssignFromValidDerived )
{
	// Arrange
	test_derived*                                             p1 = new test_derived;
	test_derived*                                             p2 = new test_derived;
	alpha::concurrent::internal::nts_shared_ptr<test_base>    sut1( p1 );
	alpha::concurrent::internal::nts_shared_ptr<test_derived> sut2( p2 );

	// Act
	sut1 = std::move( sut2 );

	// Assert
	EXPECT_EQ( p2, sut1.get() );
	EXPECT_EQ( nullptr, sut2.get() );
}

TEST( NTS_SharedPTr_Class, Empty_CanMoveAssignFromEmptyDerivedUniquePtr )
{
	// Arrange
	alpha::concurrent::internal::nts_shared_ptr<test_base> sut1;
	std::unique_ptr<test_derived>                          up_sut2;

	// Act
	sut1 = std::move( up_sut2 );

	// Assert
	EXPECT_EQ( nullptr, sut1.get() );
	EXPECT_EQ( nullptr, up_sut2.get() );
}

TEST( NTS_SharedPtr_Class, Empty_CanMoveAssignFromValidDerivedUniquePtr )
{
	// Arrange
	alpha::concurrent::internal::nts_shared_ptr<test_base> sut1;
	test_derived*                                          p = new test_derived;
	std::unique_ptr<test_derived>                          up_sut2( p );

	// Act
	sut1 = std::move( up_sut2 );

	// Assert
	EXPECT_EQ( p, sut1.get() );
	EXPECT_EQ( nullptr, up_sut2.get() );
}

TEST( NTS_SharedPtr_Class, Valid_CanMoveAssignFromEmptyDerivedUniquePtr )
{
	// Arrange
	test_derived*                                          p = new test_derived;
	alpha::concurrent::internal::nts_shared_ptr<test_base> sut1( p );
	std::unique_ptr<test_derived>                          up_sut2;

	// Act
	sut1 = std::move( up_sut2 );

	// Assert
	EXPECT_EQ( nullptr, sut1.get() );
	EXPECT_EQ( nullptr, up_sut2.get() );
}

TEST( NTS_SharedPtr_Class, Valid_CanMoveAssignFromValidDerivedUniquePtr )
{
	// Arrange
	test_derived*                                          p1 = new test_derived;
	test_derived*                                          p2 = new test_derived;
	alpha::concurrent::internal::nts_shared_ptr<test_base> sut1( p1 );
	std::unique_ptr<test_derived>                          up_sut2( p2 );

	// Act
	sut1 = std::move( up_sut2 );

	// Assert
	EXPECT_EQ( p2, sut1.get() );
	EXPECT_EQ( nullptr, up_sut2.get() );
}

TEST( NTS_SharedPtr_Class, Empty_CanResetWithoutArgument )
{
	// Arrange
	alpha::concurrent::internal::nts_shared_ptr<int> sut;

	// Act
	sut.reset();

	// Assert
	EXPECT_EQ( nullptr, sut.get() );
}

TEST( NTS_SharedPtr_Class, Valid_CanResetWithoutArgument )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sut( p );

	// Act
	sut.reset();

	// Assert
	EXPECT_EQ( nullptr, sut.get() );
}

TEST( NTS_SharedPtr_Class, Empty_CanResetWithArgument )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sut;

	// Act
	sut.reset( p );

	// Assert
	EXPECT_EQ( p, sut.get() );
}

TEST( NTS_SharedPtr_Class, Valid_CanResetWithArgument )
{
	// Arrange
	int*                                             p1 = new int;
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sut( p1 );

	// Act
	sut.reset( p2 );

	// Assert
	EXPECT_EQ( p2, sut.get() );
}

TEST( NTS_SharedPtr_Class, Empty_CanResetWithArgumentAndDeleter )
{
	// Arrange
	int*                                             p       = new int;
	auto                                             deleter = []( int* p_arg ) { delete p_arg; };
	alpha::concurrent::internal::nts_shared_ptr<int> sut;

	// Act
	sut.reset( p, deleter );

	// Assert
	EXPECT_EQ( p, sut.get() );
}

TEST( NTS_SharedPtr_Class, Valid_CanResetWithArgumentAndDeleter )
{
	// Arrange
	int*                                             p1      = new int;
	int*                                             p2      = new int;
	auto                                             deleter = []( int* p_arg ) { delete p_arg; };
	alpha::concurrent::internal::nts_shared_ptr<int> sut( p1 );

	// Act
	sut.reset( p2, deleter );

	// Assert
	EXPECT_EQ( p2, sut.get() );
}

TEST( NTS_SharedPtr_Class, CanSwap )
{
	// Arrange
	int*                                             p1 = new int;
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sut1( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sut2( p2 );

	// Act
	sut1.swap( sut2 );

	// Assert
	EXPECT_EQ( p2, sut1.get() );
	EXPECT_EQ( p1, sut2.get() );
}

TEST( NTS_SharedPtr_Class, Valid_CanOperatorArrow )
{
	// Arrange
	struct test {
		int value;
	};
	test* p  = new test;
	p->value = 123;
	alpha::concurrent::internal::nts_shared_ptr<test> sut( p );

	// Act
	int value = sut->value;

	// Assert
	EXPECT_EQ( 123, value );
}

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
