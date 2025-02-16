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

TEST( AtomicWeakPtr_Class, CanConstruct )
{
	// Arrange

	// Act
	alpha::concurrent::internal::lf_weak_ptr<int> sut;

	// Assert
	auto sp = sut.load().lock();
	EXPECT_EQ( nullptr, sp.get() );
}

TEST( AtomicWeakPtr_Class, CanCopyConstructFromEmptySharedPtr )
{
	// Arrange
	alpha::concurrent::internal::nts_shared_ptr<int> sp;

	// Act
	alpha::concurrent::internal::lf_weak_ptr<int> sut( sp );

	// Assert
	auto sp2 = sut.load().lock();
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( AtomicWeakPtr_Class, CanCopyConstructFromValidSharedPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );

	// Act
	alpha::concurrent::internal::lf_weak_ptr<int> sut( sp );

	// Assert
	auto sp2 = sut.load().lock();
	EXPECT_EQ( p, sp2.get() );
}

TEST( AtomicWeakPtr_Class, CanCopyConstructFromEmptyWeakPtr )
{
	// Arrange
	alpha::concurrent::internal::nts_weak_ptr<int> sut;

	// Act
	alpha::concurrent::internal::lf_weak_ptr<int> sut2( sut );

	// Assert
	auto sp2 = sut2.load().lock();
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( AtomicWeakPtr_Class, CanCopyConstructFromValidWeakPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut( sp );

	// Act
	alpha::concurrent::internal::lf_weak_ptr<int> sut2( sut );

	// Assert
	auto sp2 = sut2.load().lock();
	EXPECT_EQ( p, sp2.get() );
}

TEST( AtomicWeakPtr_Class, CanMoveConstructFromEmptyWeakPtr )
{
	// Arrange
	alpha::concurrent::internal::nts_weak_ptr<int> sut;

	// Act
	alpha::concurrent::internal::lf_weak_ptr<int> sut2( std::move( sut ) );

	// Assert
	auto sp2 = sut2.load().lock();
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( AtomicWeakPtr_Class, CanMoveConstructFromValidWeakPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   sut( sp );

	// Act
	alpha::concurrent::internal::lf_weak_ptr<int> sut2( std::move( sut ) );

	// Assert
	auto sp2 = sut2.load().lock();
	EXPECT_EQ( p, sp2.get() );
}

TEST( AtomicWeakPtr_Class, CanCopyConstructFromEmpty )
{
	// Arrange
	alpha::concurrent::internal::lf_weak_ptr<int> sut1;

	// Act
	alpha::concurrent::internal::lf_weak_ptr<int> sut2( sut1 );

	// Assert
	auto sp1 = sut1.load().lock();
	auto sp2 = sut2.load().lock();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( AtomicWeakPtr_Class, CanCopyConstructFromValid )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut1( sp );

	// Act
	alpha::concurrent::internal::lf_weak_ptr<int> sut2( sut1 );

	// Assert
	auto sp1 = sut1.load().lock();
	auto sp2 = sut2.load().lock();
	EXPECT_EQ( p, sp1.get() );
	EXPECT_EQ( p, sp2.get() );
}

TEST( AtomicWeakPtr_Class, CanMoveConstructFromEmpty )
{
	// Arrange
	alpha::concurrent::internal::lf_weak_ptr<int> sut1;

	// Act
	alpha::concurrent::internal::lf_weak_ptr<int> sut2( std::move( sut1 ) );

	// Assert
	auto sp1 = sut1.load().lock();
	auto sp2 = sut2.load().lock();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( AtomicWeakPtr_Class, CanMoveConstructFromValid )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut1( sp );

	// Act
	alpha::concurrent::internal::lf_weak_ptr<int> sut2( std::move( sut1 ) );

	// Assert
	auto sp1 = sut1.load().lock();
	auto sp2 = sut2.load().lock();
	EXPECT_EQ( nullptr, sp1.get() );
	EXPECT_EQ( p, sp2.get() );
}

TEST( AtomicWeakPtr_Class, Empty_CanCopyStoreFromEmptyWeakPtr )
{
	// Arrange
	alpha::concurrent::internal::lf_weak_ptr<int>  sut;
	alpha::concurrent::internal::nts_weak_ptr<int> sp;

	// Act
	sut.store( sp );

	// Assert
	auto sp2 = sut.load().lock();
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( AtomicWeakPtr_Class, Empty_CanCopyStoreFromValidWeakPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp( sp );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut;

	// Act
	sut.store( wp );

	// Assert
	auto sp2 = sut.load().lock();
	EXPECT_EQ( p, sp2.get() );
}

TEST( AtomicWeakPtr_Class, Valid_CanCopyStoreFromEmptyWeakPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut( sp );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp;

	// Act
	sut.store( wp );

	// Assert
	auto sp2 = sut.load().lock();
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( AtomicWeakPtr_Class, Valid_CanCopyStoreFromValidWeakPtr )
{
	// Arrange
	int*                                             p1 = new int;
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p2 );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp( sp1 );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut( sp2 );

	// Act
	sut.store( wp );

	// Assert
	auto sp3 = sut.load().lock();
	EXPECT_EQ( p1, sp3.get() );
}

TEST( AtomicWeakPtr_Class, Empty_CanMoveStoreFromEmptyWeakPtr )
{
	// Arrange
	alpha::concurrent::internal::lf_weak_ptr<int>  sut;
	alpha::concurrent::internal::nts_weak_ptr<int> sp;

	// Act
	sut.store( std::move( sp ) );

	// Assert
	auto sp2 = sut.load().lock();
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( AtomicWeakPtr_Class, Empty_CanMoveStoreFromValidWeakPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp( sp );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut;

	// Act
	sut.store( std::move( wp ) );

	// Assert
	auto sp2 = sut.load().lock();
	EXPECT_EQ( p, sp2.get() );
}

TEST( AtomicWeakPtr_Class, Valid_CanMoveStoreFromEmptyWeakPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut( sp );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp;

	// Act
	sut.store( std::move( wp ) );

	// Assert
	auto sp2 = sut.load().lock();
	EXPECT_EQ( nullptr, sp2.get() );
}

TEST( AtomicWeakPtr_Class, Valid_CanMoveStoreFromValidWeakPtr )
{
	// Arrange
	int*                                             p1 = new int;
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p2 );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp( sp1 );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut( sp2 );

	// Act
	sut.store( std::move( wp ) );

	// Assert
	auto sp3 = sut.load().lock();
	EXPECT_EQ( p1, sp3.get() );
}

TEST( AtomicWeakPtr_Class, Empty_CanCopyExchangeFromEmptyWeakPtr )
{
	// Arrange
	alpha::concurrent::internal::lf_weak_ptr<int>  sut;
	alpha::concurrent::internal::nts_weak_ptr<int> sp;

	// Act
	auto sp2 = sut.exchange( sp );

	// Assert
	auto sp3 = sut.load().lock();
	EXPECT_EQ( nullptr, sp2.lock().get() );
	EXPECT_EQ( nullptr, sp3.get() );
}

TEST( AtomicWeakPtr_Class, Empty_CanCopyExchangeFromValidWeakPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp( sp );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut;

	// Act
	auto sp2 = sut.exchange( wp );

	// Assert
	auto sp3 = sut.load().lock();
	EXPECT_EQ( nullptr, sp2.lock().get() );
	EXPECT_EQ( p, sp3.get() );
}

TEST( AtomicWeakPtr_Class, Valid_CanCopyExchangeFromEmptyWeakPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut( sp );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp;

	// Act
	auto sp2 = sut.exchange( wp );

	// Assert
	auto sp3 = sut.load().lock();
	EXPECT_EQ( p, sp2.lock().get() );
	EXPECT_EQ( nullptr, sp3.get() );
}

TEST( AtomicWeakPtr_Class, Valid_CanCopyExchangeFromValidWeakPtr )
{
	// Arrange
	int*                                             p1 = new int;
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p2 );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp( sp1 );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut( sp2 );

	// Act
	auto sp3 = sut.exchange( wp );

	// Assert
	auto sp4 = sut.load().lock();
	EXPECT_EQ( p2, sp3.lock().get() );
	EXPECT_EQ( p1, sp4.get() );
}

TEST( AtomicWeakPtr_Class, Empty_CanMoveExchangeFromEmptyWeakPtr )
{
	// Arrange
	alpha::concurrent::internal::lf_weak_ptr<int>  sut;
	alpha::concurrent::internal::nts_weak_ptr<int> sp;

	// Act
	auto sp2 = sut.exchange( std::move( sp ) );

	// Assert
	auto sp3 = sut.load().lock();
	EXPECT_EQ( nullptr, sp2.lock().get() );
	EXPECT_EQ( nullptr, sp3.get() );
}

TEST( AtomicWeakPtr_Class, Empty_CanMoveExchangeFromValidWeakPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp( sp );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut;

	// Act
	auto sp2 = sut.exchange( std::move( wp ) );

	// Assert
	auto sp3 = sut.load().lock();
	EXPECT_EQ( nullptr, sp2.lock().get() );
	EXPECT_EQ( p, sp3.get() );
}

TEST( AtomicWeakPtr_Class, Valid_CanMoveExchangeFromEmptyWeakPtr )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut( sp );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp;

	// Act
	auto sp2 = sut.exchange( std::move( wp ) );

	// Assert
	auto sp3 = sut.load().lock();
	EXPECT_EQ( p, sp2.lock().get() );
	EXPECT_EQ( nullptr, sp3.get() );
}

TEST( AtomicWeakPtr_Class, Valid_CanMoveExchangeFromValidWeakPtr )
{
	// Arrange
	int*                                             p1 = new int;
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p2 );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp( sp1 );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut( sp2 );

	// Act
	auto sp3 = sut.exchange( std::move( wp ) );

	// Assert
	auto sp4 = sut.load().lock();
	EXPECT_EQ( p2, sp3.lock().get() );
	EXPECT_EQ( p1, sp4.get() );
}

TEST( AtomicWeakPtr_Class, Empty_CanCompareExchangeWeakWithEmptyWeakPtrAndEmptyWeakPtr_Then_ReturnTrue )
{
	// Arrange
	alpha::concurrent::internal::lf_weak_ptr<int>  sut;
	alpha::concurrent::internal::nts_weak_ptr<int> wp1;
	alpha::concurrent::internal::nts_weak_ptr<int> wp2;

	// Act
	auto result = sut.compare_exchange_weak( wp1, wp2 );

	// Assert
	auto sp = sut.load().lock();
	EXPECT_EQ( nullptr, sp.get() );
	EXPECT_TRUE( result );
	EXPECT_EQ( nullptr, wp1.lock().get() );
	EXPECT_EQ( nullptr, wp2.lock().get() );
}

TEST( AtomicWeakPtr_Class, Empty_CanCompareExchangeWeakWithEmptyWeakPtrAndValidWeakPtr_Then_ReturnTrue )
{
	// Arrange
	alpha::concurrent::internal::lf_weak_ptr<int>    sut;
	alpha::concurrent::internal::nts_weak_ptr<int>   wp1;
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp2( sp1 );

	// Act
	auto result = sut.compare_exchange_weak( wp1, wp2 );

	// Assert
	auto sp2 = sut.load().lock();
	EXPECT_EQ( p, sp2.get() );
	EXPECT_TRUE( result );
	EXPECT_EQ( nullptr, wp1.lock().get() );
	EXPECT_EQ( p, wp2.lock().get() );
}

TEST( AtomicWeakPtr_Class, Empty_CanCompareExchangeWeakWithValidWeakPtrAndEmptyWeakPtr_Then_ReturnFalse )
{
	// Arrange
	alpha::concurrent::internal::lf_weak_ptr<int>    sut;
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp1( sp1 );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp2;

	// Act
	auto result = sut.compare_exchange_weak( wp1, wp2 );

	// Assert
	auto sp2 = sut.load().lock();
	EXPECT_EQ( nullptr, sp2.get() );
	EXPECT_FALSE( result );
	EXPECT_EQ( nullptr, wp1.lock().get() );
	EXPECT_EQ( nullptr, wp2.lock().get() );
}

TEST( AtomicWeakPtr_Class, Empty_CanCompareExchangeWeakWithValidWeakPtrAndValidWeakPtr_Then_ReturnFalse )
{
	// Arrange
	alpha::concurrent::internal::lf_weak_ptr<int>    sut;
	int*                                             p1 = new int;
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p1 );
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p2 );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp1( sp1 );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp2( sp2 );

	// Act
	auto result = sut.compare_exchange_weak( wp1, wp2 );

	// Assert
	auto sp3 = sut.load().lock();
	EXPECT_EQ( nullptr, sp3.get() );
	EXPECT_FALSE( result );
	EXPECT_EQ( nullptr, wp1.lock().get() );
	EXPECT_EQ( p2, wp2.lock().get() );
}

TEST( AtomicWeakPtr_Class, Valid_CanCompareExchangeWeakWithEmptyWeakPtrAndEmptyWeakPtr_Then_ReturnFalse )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut( sp );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp1;
	alpha::concurrent::internal::nts_weak_ptr<int>   wp2;

	// Act
	auto result = sut.compare_exchange_weak( wp1, wp2 );

	// Assert
	auto sp2 = sut.load().lock();
	EXPECT_EQ( p, sp2.get() );
	EXPECT_FALSE( result );
	EXPECT_EQ( p, wp1.lock().get() );
	EXPECT_EQ( nullptr, wp2.lock().get() );
}

TEST( AtomicWeakPtr_Class, Valid_CanCompareExchangeWeakWithEmptyWeakPtrAndValidWeakPtr_Then_ReturnFalse )
{
	// Arrange
	int*                                             p = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp( p );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut( sp );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp1;
	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p2 );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp2( sp2 );

	// Act
	auto result = sut.compare_exchange_weak( wp1, wp2 );

	// Assert
	auto sp3 = sut.load().lock();
	EXPECT_EQ( p, sp3.get() );
	EXPECT_FALSE( result );
	EXPECT_EQ( p, wp1.lock().get() );
	EXPECT_EQ( p2, wp2.lock().get() );
}

TEST( AtomicWeakPtr_Class, Valid_CanCompareExchangeWeakWithDifferentValidWeakPtrAndEmptyWeakPtr_Then_ReturnFalse )
{
	// Arrange
	int*                                             p1 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p1 );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut( sp1 );

	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p2 );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp1( sp2 );

	alpha::concurrent::internal::nts_weak_ptr<int> wp2;

	// Act
	auto result = sut.compare_exchange_weak( wp1, wp2 );

	// Assert
	auto sp3 = sut.load().lock();
	EXPECT_EQ( p1, sp3.get() );
	EXPECT_FALSE( result );
	EXPECT_EQ( p1, wp1.lock().get() );
	EXPECT_EQ( nullptr, wp2.lock().get() );
}

TEST( AtomicWeakPtr_Class, Valid_CanCompareExchangeWeakWithSameValidWeakPtrAndEmptyWeakPtr_Then_ReturnTrue )
{
	// Arrange
	int*                                             p1 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p1 );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut( sp1 );

	alpha::concurrent::internal::nts_weak_ptr<int> wp1( sp1 );
	alpha::concurrent::internal::nts_weak_ptr<int> wp2;

	// Act
	auto result = sut.compare_exchange_weak( wp1, wp2 );

	// Assert
	auto sp3 = sut.load().lock();
	EXPECT_EQ( nullptr, sp3.get() );
	EXPECT_TRUE( result );
	EXPECT_EQ( p1, wp1.lock().get() );
	EXPECT_EQ( nullptr, wp2.lock().get() );
}

TEST( AtomicWeakPtr_Class, Valid_CanCompareExchangeWeakWithDifferentValidWeakPtrAndValidWeakPtr_Then_ReturnFalse )
{
	// Arrange
	int*                                             p1 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p1 );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut( sp1 );

	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p2 );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp1( sp2 );

	int*                                             p3 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp3( p3 );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp2( sp3 );

	// Act
	auto result = sut.compare_exchange_weak( wp1, wp2 );

	// Assert
	auto sp4 = sut.load().lock();
	EXPECT_EQ( p1, sp4.get() );
	EXPECT_FALSE( result );
	EXPECT_EQ( p1, wp1.lock().get() );
	EXPECT_EQ( p3, wp2.lock().get() );
}

TEST( AtomicWeakPtr_Class, Valid_CanCompareExchangeWeakWithSameValidWeakPtrAndValidWeakPtr_Then_ReturnTrue )
{
	// Arrange
	int*                                             p1 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p1 );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut( sp1 );

	alpha::concurrent::internal::nts_weak_ptr<int> wp1( sp1 );

	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p2 );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp2( sp2 );

	// Act
	auto result = sut.compare_exchange_weak( wp1, wp2 );

	// Assert
	auto sp3 = sut.load().lock();
	EXPECT_EQ( p2, sp3.get() );
	EXPECT_TRUE( result );
	EXPECT_EQ( p1, wp1.lock().get() );
	EXPECT_EQ( p2, wp2.lock().get() );
}

TEST( AtomicWeakPtr_Class, Valid_CanCompareExchangeStrongWithSameValidWeakPtrAndValidWeakPtr_Then_ReturnTrue )
{
	// Arrange
	int*                                             p1 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp1( p1 );
	alpha::concurrent::internal::lf_weak_ptr<int>    sut( sp1 );

	alpha::concurrent::internal::nts_weak_ptr<int> wp1( sp1 );

	int*                                             p2 = new int;
	alpha::concurrent::internal::nts_shared_ptr<int> sp2( p2 );
	alpha::concurrent::internal::nts_weak_ptr<int>   wp2( sp2 );

	// Act
	auto result = sut.compare_exchange_strong( wp1, wp2 );

	// Assert
	auto sp3 = sut.load().lock();
	EXPECT_EQ( p2, sp3.get() );
	EXPECT_TRUE( result );
	EXPECT_EQ( p1, wp1.lock().get() );
	EXPECT_EQ( p2, wp2.lock().get() );
}
