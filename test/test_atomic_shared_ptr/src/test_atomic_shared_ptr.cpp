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

#include "test_derived_class.hpp"

TEST( AtomicSharedPtr_Class, CanConstruct )
{
	// Arrange

	// Act
	alpha::concurrent::internal::lf_shared_ptr<int> sut;

	// Assert
	auto sp = sut.load();
	EXPECT_EQ( nullptr, sp.get() );
}

TEST( AtomicSharedPtr_Class, CanConstructWithPointer )
{
	// Arrange
	int* p = new int;

	// Act
	alpha::concurrent::internal::lf_shared_ptr<int> sut( p );

	// Assert
	auto sp = sut.load();
	EXPECT_EQ( p, sp.get() );
}

TEST( AtomicSharedPtr_Class, CanConstructWithPointerAndDeleter )
{
	// Arrange
	int* p       = new int;
	auto deleter = []( int* p_arg ) { delete p_arg; };

	// Act
	alpha::concurrent::internal::lf_shared_ptr<int> sut( p, deleter );

	// Assert
	auto sp = sut.load();
	EXPECT_EQ( p, sp.get() );
}

TEST( AtomicSharedPtr_Class, CanConstructWithUniquePtr )
{
	// Arrange
	int*                 p = new int;
	std::unique_ptr<int> up( p );

	// Act
	alpha::concurrent::internal::lf_shared_ptr<int> sut( std::move( up ) );

	// Assert
	auto sp = sut.load();
	EXPECT_EQ( p, sp.get() );
}

TEST( AtomicSharedPtr_Class, CanDestruct )
{
	// Arrange
	int* p = new int;

	// Act
	{
		alpha::concurrent::internal::lf_shared_ptr<int> sut( p );
	}
}

TEST( AtomicSharedPtr_Class, CanCopyConstructFromEmpty )
{
	// Arrange
	alpha::concurrent::internal::lf_shared_ptr<int> sut1;

	// Act
	alpha::concurrent::internal::lf_shared_ptr<int> sut2( sut1 );

	// Assert
	auto sp1 = sut1.load();
	auto sp2 = sut2.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( AtomicSharedPtr_Class, CanCopyConstructFromValid )
{
	// Arrange
	int*                                            p = new int;
	alpha::concurrent::internal::lf_shared_ptr<int> sut1( p );

	// Act
	alpha::concurrent::internal::lf_shared_ptr<int> sut2( sut1 );

	// Assert
	auto sp1 = sut1.load();
	auto sp2 = sut2.load();
	EXPECT_EQ( p, sp1.get() );
	EXPECT_EQ( p, sp2.get() );
}

TEST( AtomicSharedPtr_Class, CanMoveConstructFromEmpty )
{
	// Arrange
	alpha::concurrent::internal::lf_shared_ptr<int> sut1;

	// Act
	alpha::concurrent::internal::lf_shared_ptr<int> sut2( std::move( sut1 ) );

	// Assert
	auto sp1 = sut1.load();
	auto sp2 = sut2.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( AtomicSharedPtr_Class, CanMoveConstructFromValid )
{
	// Arrange
	int*                                            p = new int;
	alpha::concurrent::internal::lf_shared_ptr<int> sut1( p );

	// Act
	alpha::concurrent::internal::lf_shared_ptr<int> sut2( std::move( sut1 ) );

	// Assert
	auto sp1 = sut1.load();
	auto sp2 = sut2.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( p, sp2.get() );
}

TEST( AtomicSharedPtr_Class, Empty_CanCopyStoreFromEmptySharedPtr )
{
	// Arrange
	alpha::concurrent::internal::lf_shared_ptr<int>  sut;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1;

	// Act
	sut.store( sp1 );

	// Assert
	auto sp2 = sut.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( AtomicSharedPtr_Class, Empty_CanCopyStoreFromValidSharedPtr )
{
	// Arrange
	alpha::concurrent::internal::lf_shared_ptr<int>  sut;
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p );

	// Act
	sut.store( sp1 );

	// Assert
	auto sp2 = sut.load();
	EXPECT_EQ( p, sp1.get() );
	EXPECT_EQ( p, sp2.get() );
}

TEST( AtomicSharedPtr_Class, Valid_CanCopyStoreFromEmptySharedPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::lf_shared_ptr<int>  sut( p );
	alpha::concurrent::internal::nts_shared_ptr<int> sp1;

	// Act
	sut.store( sp1 );

	// Assert
	auto sp2 = sut.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( AtomicSharedPtr_Class, Valid_CanCopyStoreFromValidSharedPtr )
{
	// Arrange
	int*                                             p1 = new int;
	int*                                             p2 = new int;
	alpha::concurrent::internal::lf_shared_ptr<int>  sut( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p2 );

	// Act
	sut.store( sp1 );

	// Assert
	auto sp2 = sut.load();
	EXPECT_EQ( p2, sp1.get() );
	EXPECT_EQ( p2, sp2.get() );
}

TEST( AtomicSharedPtr_Class, Empty_CanMoveStoreFromEmptySharedPtr )
{
	// Arrange
	alpha::concurrent::internal::lf_shared_ptr<int>  sut;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1;

	// Act
	sut.store( std::move( sp1 ) );

	// Assert
	auto sp2 = sut.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( AtomicSharedPtr_Class, Empty_CanMoveStoreFromValidSharedPtr )
{
	// Arrange
	alpha::concurrent::internal::lf_shared_ptr<int>  sut;
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p );

	// Act
	sut.store( std::move( sp1 ) );

	// Assert
	auto sp2 = sut.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( p, sp2.get() );
}

TEST( AtomicSharedPtr_Class, Valid_CanMoveStoreFromEmptySharedPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::lf_shared_ptr<int>  sut( p );
	alpha::concurrent::internal::nts_shared_ptr<int> sp1;

	// Act
	sut.store( std::move( sp1 ) );

	// Assert
	auto sp2 = sut.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( AtomicSharedPtr_Class, Valid_CanMoveStoreFromValidSharedPtr )
{
	// Arrange
	int*                                             p1 = new int;
	int*                                             p2 = new int;
	alpha::concurrent::internal::lf_shared_ptr<int>  sut( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p2 );

	// Act
	sut.store( std::move( sp1 ) );

	// Assert
	auto sp2 = sut.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( p2, sp2.get() );
}

TEST( AtomicSharedPtr_Class, Empty_CanExchangeCopyFromEmptySharedPtr )
{
	// Arrange
	alpha::concurrent::internal::lf_shared_ptr<int>  sut;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1;

	// Act
	auto sp2 = sut.exchange( sp1 );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( nullptr, sp2.get() );
	EXPECT_EQ( nullptr, sp3.get() );
}

TEST( AtomicSharedPtr_Class, Empty_CanExchangeCopyFromValidSharedPtr )
{
	// Arrange
	alpha::concurrent::internal::lf_shared_ptr<int>  sut;
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p );

	// Act
	auto sp2 = sut.exchange( sp1 );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( p, sp1.get() );
	EXPECT_EQ( nullptr, sp2.get() );
	EXPECT_EQ( p, sp3.get() );
}

TEST( AtomicSharedPtr_Class, Valid_CanExchangeCopyFromEmptySharedPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::lf_shared_ptr<int>  sut( p );
	alpha::concurrent::internal::nts_shared_ptr<int> sp1;

	// Act
	auto sp2 = sut.exchange( sp1 );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( p, sp2.get() );
	EXPECT_EQ( nullptr, sp3.get() );
}

TEST( AtomicSharedPtr_Class, Valid_CanExchangeCopyFromValidSharedPtr )
{
	// Arrange
	int*                                             p1 = new int;
	int*                                             p2 = new int;
	alpha::concurrent::internal::lf_shared_ptr<int>  sut( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p2 );

	// Act
	auto sp2 = sut.exchange( sp1 );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( p2, sp1.get() );
	EXPECT_EQ( p1, sp2.get() );
	EXPECT_EQ( p2, sp3.get() );
}

TEST( AtomicSharedPtr_Class, Empty_CanExchangeMoveFromEmptySharedPtr )
{
	// Arrange
	alpha::concurrent::internal::lf_shared_ptr<int>  sut;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1;

	// Act
	auto sp2 = sut.exchange( std::move( sp1 ) );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( nullptr, sp2.get() );
	EXPECT_EQ( nullptr, sp3.get() );
}

TEST( AtomicSharedPtr_Class, Empty_CanExchangeMoveFromValidSharedPtr )
{
	// Arrange
	alpha::concurrent::internal::lf_shared_ptr<int>  sut;
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p );

	// Act
	auto sp2 = sut.exchange( std::move( sp1 ) );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( nullptr, sp2.get() );
	EXPECT_EQ( p, sp3.get() );
}

TEST( AtomicSharedPtr_Class, Valid_CanExchangeMoveFromEmptySharedPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::lf_shared_ptr<int>  sut( p );
	alpha::concurrent::internal::nts_shared_ptr<int> sp1;

	// Act
	auto sp2 = sut.exchange( std::move( sp1 ) );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( p, sp2.get() );
	EXPECT_EQ( nullptr, sp3.get() );
}

TEST( AtomicSharedPtr_Class, Valid_CanExchangeMoveFromValidSharedPtr )
{
	// Arrange
	int*                                             p1 = new int;
	int*                                             p2 = new int;
	alpha::concurrent::internal::lf_shared_ptr<int>  sut( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p2 );

	// Act
	auto sp2 = sut.exchange( std::move( sp1 ) );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( p1, sp2.get() );
	EXPECT_EQ( p2, sp3.get() );
}

TEST( AtomicSharedPtr_Class, Empty_CanCompareExchangeWeakWithEmptySharedPtrAndEmptySharedPtr_Then_ReturnTrue )
{
	// Arrange
	alpha::concurrent::internal::lf_shared_ptr<int>  sut;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1;
	alpha::concurrent::internal::nts_shared_ptr<int> sp2;

	// Act
	auto result = sut.compare_exchange_weak( sp1, sp2 );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( nullptr, sp2.get() );
	EXPECT_TRUE( result );
	EXPECT_EQ( nullptr, sp3.get() );
}

TEST( AtomicSharedPtr_Class, Empty_CanCompareExchangeWeakWithEmptySharedPtrAndValidSharedPtr_Then_ReturnTrue )
{
	// Arrange
	alpha::concurrent::internal::lf_shared_ptr<int>  sut;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1;
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p );

	// Act
	auto result = sut.compare_exchange_weak( sp1, sp2 );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( p, sp2.get() );
	EXPECT_TRUE( result );
	EXPECT_EQ( p, sp3.get() );
}

TEST( AtomicSharedPtr_Class, Empty_CanCompareExchangeWeakWithValidSharedPtrAndEmptySharedPtr_Then_ReturnFalse )
{
	// Arrange
	alpha::concurrent::internal::lf_shared_ptr<int>  sut;
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p );
	alpha::concurrent::internal::nts_shared_ptr<int> sp2;

	// Act
	auto result = sut.compare_exchange_weak( sp1, sp2 );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( nullptr, sp2.get() );
	EXPECT_FALSE( result );
	EXPECT_EQ( nullptr, sp3.get() );
}

TEST( AtomicSharedPtr_Class, Empty_CanCompareExchangeWeakWithValidSharedPtrAndValidSharedPtr_Then_ReturnFalse )
{
	// Arrange
	alpha::concurrent::internal::lf_shared_ptr<int>  sut;
	int*                                             p1 = new int;
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p2 );

	// Act
	auto result = sut.compare_exchange_weak( sp1, sp2 );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( p2, sp2.get() );
	EXPECT_FALSE( result );
	EXPECT_EQ( nullptr, sp3.get() );
}

TEST( AtomicSharedPtr_Class, Valid_CanCompareExchangeWeakWithEmptySharedPtrAndEmptySharedPtr_Then_ReturnFalse )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::lf_shared_ptr<int>  sut( p );
	alpha::concurrent::internal::nts_shared_ptr<int> sp1;
	alpha::concurrent::internal::nts_shared_ptr<int> sp2;

	// Act
	auto result = sut.compare_exchange_weak( sp1, sp2 );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( p, sp1.get() );
	EXPECT_EQ( nullptr, sp2.get() );
	EXPECT_FALSE( result );
	EXPECT_EQ( p, sp3.get() );
}

TEST( AtomicSharedPtr_Class, Valid_CanCompareExchangeWeakWithEmptySharedPtrAndValidSharedPtr_Then_ReturnFalse )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::lf_shared_ptr<int>  sut( p );
	alpha::concurrent::internal::nts_shared_ptr<int> sp1;
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p2 );

	// Act
	auto result = sut.compare_exchange_weak( sp1, sp2 );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( p, sp1.get() );
	EXPECT_EQ( p2, sp2.get() );
	EXPECT_FALSE( result );
	EXPECT_EQ( p, sp3.get() );
}

TEST( AtomicSharedPtr_Class, Valid_CanCompareExchangeWeakWithDifferentValidSharedPtrAndEmptySharedPtr_Then_ReturnFalse )
{
	// Arrange
	int*                                             p1 = new int;
	alpha::concurrent::internal::lf_shared_ptr<int>  sut( p1 );
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p2 );
	alpha::concurrent::internal::nts_shared_ptr<int> sp2;

	// Act
	auto result = sut.compare_exchange_weak( sp1, sp2 );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( p1, sp1.get() );
	EXPECT_EQ( nullptr, sp2.get() );
	EXPECT_FALSE( result );
	EXPECT_EQ( p1, sp3.get() );
}

TEST( AtomicSharedPtr_Class, Valid_CanCompareExchangeWeakWithSameValidSharedPtrAndEmptySharedPtr_Then_ReturnTrue )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::lf_shared_ptr<int>  sut( p );
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( sut.load() );
	alpha::concurrent::internal::nts_shared_ptr<int> sp2;

	// Act
	auto result = sut.compare_exchange_weak( sp1, sp2 );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( p, sp1.get() );
	EXPECT_EQ( nullptr, sp2.get() );
	EXPECT_TRUE( result );
	EXPECT_EQ( nullptr, sp3.get() );
}

TEST( AtomicSharedPtr_Class, Valid_CanCompareExchangeWeakWithDifferentValidSharedPtrAndValidSharedPtr_Then_ReturnFalse )
{
	// Arrange
	int*                                             p1 = new int;
	alpha::concurrent::internal::lf_shared_ptr<int>  sut( p1 );
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p2 );
	int*                                             p3 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p3 );

	// Act
	auto result = sut.compare_exchange_weak( sp1, sp2 );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( p1, sp1.get() );
	EXPECT_EQ( p3, sp2.get() );
	EXPECT_FALSE( result );
	EXPECT_EQ( p1, sp3.get() );
}

TEST( AtomicSharedPtr_Class, Valid_CanCompareExchangeWeakWithSameValidSharedPtrAndValidSharedPtr_Then_ReturnTrue )
{
	// Arrange
	int*                                             p1 = new int;
	alpha::concurrent::internal::lf_shared_ptr<int>  sut( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( sut.load() );
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p2 );

	// Act
	auto result = sut.compare_exchange_weak( sp1, sp2 );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( p1, sp1.get() );
	EXPECT_EQ( p2, sp2.get() );
	EXPECT_TRUE( result );
	EXPECT_EQ( p2, sp3.get() );
}

TEST( AtomicSharedPtr_Class, Valid_CanCompareExchangeStrongWithSameValidSharedPtrAndValidSharedPtr_Then_ReturnTrue )
{
	// Arrange
	int*                                             p1 = new int;
	alpha::concurrent::internal::lf_shared_ptr<int>  sut( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( sut.load() );
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p2 );

	// Act
	auto result = sut.compare_exchange_strong( sp1, sp2 );

	// Assert
	auto sp3 = sut.load();
	EXPECT_EQ( p1, sp1.get() );
	EXPECT_EQ( p2, sp2.get() );
	EXPECT_TRUE( result );
	EXPECT_EQ( p2, sp3.get() );
}
