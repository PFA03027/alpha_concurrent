/**
 * @file test_hazard_ptr2.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-01
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include <iostream>
#include <pthread.h>

#include "gtest/gtest.h"

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/hazard_ptr.hpp"
#include "alconcurrent/internal/hazard_ptr_internal.hpp"
#include "alconcurrent/internal/retire_mgr.hpp"

TEST( HazardPtrGroup, CanDefaultConstruct )
{
	// Arrange

	// Act
	alpha::concurrent::internal::hazard_ptr_group sut;

	// Assert
}

TEST( HazardPtrGroup, CanTryAssing )
{
	// Arrange
	alpha::concurrent::internal::hazard_ptr_group sut;
	char                                          dummy = 1;

	// Act
	auto righofownership = sut.try_assign( &dummy );

	// Assert
	EXPECT_NE( righofownership, nullptr );
}

TEST( HazardPtrGroup, CanTryAssingForNullptr )
{
	// Arrange
	alpha::concurrent::internal::hazard_ptr_group sut;

	// Act
	auto righofownership = sut.try_assign( nullptr );

	// Assert
	EXPECT_EQ( righofownership, nullptr );
}

TEST( HazardPtrGroup, CanTryAssingWithFull )
{
	// Arrange
	alpha::concurrent::internal::hazard_ptr_group      sut;
	char                                               dummy = 1;
	int                                                dummy_array[alpha::concurrent::internal::hazard_ptr_group::kArraySize];
	alpha::concurrent::internal::hzrd_slot_ownership_t righofownership_array[alpha::concurrent::internal::hazard_ptr_group::kArraySize];
	for ( size_t i = 0; i < alpha::concurrent::internal::hazard_ptr_group::kArraySize; i++ ) {
		righofownership_array[i] = sut.try_assign( &dummy_array[i] );
		EXPECT_NE( righofownership_array[i], nullptr );
	}

	// Act
	auto righofownership = sut.try_assign( &dummy );

	// Assert
	EXPECT_EQ( righofownership, nullptr );
}

TEST( HazardPtrGroup, CanTryOcupy1 )
{
	// Arrange
	alpha::concurrent::internal::hazard_ptr_group sut;

	// Act
	auto ownership = sut.try_ocupy();

	// Assert
	EXPECT_NE( ownership, nullptr );
}

TEST( HazardPtrGroup, CanTryOcupy2 )
{
	// Arrange
	alpha::concurrent::internal::hazard_ptr_group sut;
	auto                                          ownership1 = sut.try_ocupy();

	// Act
	auto ownership2 = sut.try_ocupy();

	// Assert
	EXPECT_EQ( ownership2, nullptr );
}

/////////////////////////////////////////////////////////////////////////////////
class TestGlobalScopeHazardPtrChain : public ::testing::Test {
protected:
	void SetUp() override
	{
		alpha::concurrent::GetErrorWarningLogCountAndReset( nullptr, nullptr );
	}

	void TearDown() override
	{
		alpha::concurrent::internal::retire_mgr::stop_prune_thread();
		alpha::concurrent::internal::hazard_ptr_mgr::DestoryAll();

		int cw, ce;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
		EXPECT_EQ( ce, 0 );
		EXPECT_EQ( cw, 0 );
	}
};
TEST_F( TestGlobalScopeHazardPtrChain, CallDestroyAll )
{
	{
		// Arrange
		alpha::concurrent::internal::bind_hazard_ptr_list sut1;
		alpha::concurrent::internal::bind_hazard_ptr_list sut2;

		char                                               dummy1 = 1;
		char                                               dummy2 = 2;
		int                                                dummy_array[alpha::concurrent::internal::hazard_ptr_group::kArraySize];
		alpha::concurrent::internal::hzrd_slot_ownership_t righofownership_array[alpha::concurrent::internal::hazard_ptr_group::kArraySize];
		for ( size_t i = 0; i < alpha::concurrent::internal::hazard_ptr_group::kArraySize; i++ ) {
			righofownership_array[i] = sut1.slot_assign( &dummy_array[i] );
			EXPECT_NE( righofownership_array[i], nullptr );
		}
		auto righofownership1 = sut1.slot_assign( &dummy1 );
		EXPECT_NE( righofownership1, nullptr );
		auto righofownership2 = sut2.slot_assign( &dummy2 );
		EXPECT_NE( righofownership2, nullptr );

		alpha::concurrent::internal::retire_mgr::stop_prune_thread();
	}

	// Act
	alpha::concurrent::internal::hazard_ptr_mgr::DestoryAll();

	// Assert
	// no memory leak
}

TEST_F( TestGlobalScopeHazardPtrChain, CallCheckPtrIsHazardPtr1 )
{
	// Arrange
	alpha::concurrent::internal::bind_hazard_ptr_list sut1;

	char dummy1           = 1;
	char dummy2           = 2;
	auto righofownership1 = sut1.slot_assign( &dummy1 );
	EXPECT_NE( righofownership1, nullptr );

	// Act
	bool ret1 = alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy1 );
	bool ret2 = alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy2 );

	// Assert
	EXPECT_TRUE( ret1 );
	EXPECT_FALSE( ret2 );
}

TEST_F( TestGlobalScopeHazardPtrChain, CallCheckPtrIsHazardPtr2 )
{
	// Arrange
	alpha::concurrent::internal::bind_hazard_ptr_list sut1;

	char dummy1           = 1;
	auto righofownership1 = sut1.slot_assign( &dummy1 );
	EXPECT_NE( righofownership1, nullptr );
	bool ret1 = alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy1 );
	EXPECT_TRUE( ret1 );
	righofownership1 = alpha::concurrent::internal::hzrd_slot_ownership_t {};

	// Act
	bool ret2 = alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy1 );

	// Assert
	EXPECT_FALSE( ret2 );
}

TEST_F( TestGlobalScopeHazardPtrChain, CallCheckPtrIsHazardPtr3 )
{
	// Arrange
	alpha::concurrent::internal::bind_hazard_ptr_list sut1;
	alpha::concurrent::internal::bind_hazard_ptr_list sut2;

	char                                               dummy1 = 1;
	char                                               dummy2 = 2;
	char                                               dummy3 = 3;
	int                                                dummy_array[alpha::concurrent::internal::hazard_ptr_group::kArraySize];
	alpha::concurrent::internal::hzrd_slot_ownership_t righofownership_array[alpha::concurrent::internal::hazard_ptr_group::kArraySize];
	for ( size_t i = 0; i < alpha::concurrent::internal::hazard_ptr_group::kArraySize; i++ ) {
		righofownership_array[i] = sut1.slot_assign( &dummy_array[i] );
		EXPECT_NE( righofownership_array[i], nullptr );
	}
	auto righofownership1 = sut1.slot_assign( &dummy1 );
	EXPECT_NE( righofownership1, nullptr );
	auto righofownership2 = sut2.slot_assign( &dummy2 );
	EXPECT_NE( righofownership2, nullptr );

	// Act
	bool ret1 = alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy1 );
	bool ret2 = alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy2 );
	bool ret3 = alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy3 );
	bool ret4 = alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy_array[0] );

	// Assert
	EXPECT_TRUE( ret1 );
	EXPECT_TRUE( ret2 );
	EXPECT_FALSE( ret3 );
	EXPECT_TRUE( ret4 );
}

/////////////////////////////////////////////////////////////////////////////////
class TestBindHazardPtrList : public ::testing::Test {
protected:
	void SetUp() override
	{
		alpha::concurrent::GetErrorWarningLogCountAndReset( nullptr, nullptr );
	}

	void TearDown() override
	{
		alpha::concurrent::internal::retire_mgr::stop_prune_thread();
		alpha::concurrent::internal::hazard_ptr_mgr::DestoryAll();

		int cw, ce;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
		EXPECT_EQ( ce, 0 );
		EXPECT_EQ( cw, 0 );
	}
};

TEST_F( TestBindHazardPtrList, CallConstructor )
{
	// Arrange

	// Act
	alpha::concurrent::internal::bind_hazard_ptr_list sut;

	// Assert
}

TEST_F( TestBindHazardPtrList, CallAssing1 )
{
	// Arrange
	// alpha::concurrent::internal::bind_hazard_ptr_list sut( p_hpg1_->try_ocupy() );
	alpha::concurrent::internal::bind_hazard_ptr_list sut;
	char                                              dummy;

	// Act
	auto ownership = sut.slot_assign( &dummy );

	// Assert
	EXPECT_NE( ownership, nullptr );
}

TEST_F( TestBindHazardPtrList, CallAssingForNullptr )
{
	// Arrange
	alpha::concurrent::internal::bind_hazard_ptr_list sut;

	// Act
	auto ownership = sut.slot_assign( nullptr );

	// Assert
	EXPECT_EQ( ownership, nullptr );
}

TEST_F( TestBindHazardPtrList, CallAssingWithFull )
{
	// Arrange
	alpha::concurrent::internal::bind_hazard_ptr_list  sut;
	char                                               dummy = 1;
	int                                                dummy_array[alpha::concurrent::internal::hazard_ptr_group::kArraySize];
	alpha::concurrent::internal::hzrd_slot_ownership_t righofownership_array[alpha::concurrent::internal::hazard_ptr_group::kArraySize];
	for ( size_t i = 0; i < alpha::concurrent::internal::hazard_ptr_group::kArraySize; i++ ) {
		righofownership_array[i] = sut.slot_assign( &dummy_array[i] );
		EXPECT_NE( righofownership_array[i], nullptr );
	}

	// Act
	auto righofownership = sut.slot_assign( &dummy );

	// Assert
	EXPECT_NE( righofownership, nullptr );
}

/////////////////////////////////////////////////////////////////////////////////
class TestHazardPtrHandler : public ::testing::Test {
protected:
	void SetUp() override
	{
		alpha::concurrent::GetErrorWarningLogCountAndReset( nullptr, nullptr );
	}

	void TearDown() override
	{
		alpha::concurrent::internal::retire_mgr::stop_prune_thread();
		alpha::concurrent::internal::hazard_ptr_mgr::DestoryAll();

		int cw, ce;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
		EXPECT_EQ( ce, 0 );
		EXPECT_EQ( cw, 0 );
	}
};

TEST_F( TestHazardPtrHandler, CallDefaultConstructor )
{
	// Arrange

	// Act
	alpha::concurrent::hazard_ptr_handler<int> sut;

	// Assert
	auto hp2 = sut.get();
	EXPECT_EQ( hp2, nullptr );
	EXPECT_FALSE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( nullptr ) );
}

TEST_F( TestHazardPtrHandler, CallTransConstructor )
{
	// Arrange
	int dummy1 = 1;

	// Act
	alpha::concurrent::hazard_ptr_handler<int> sut( &dummy1 );

	// Assert
	auto hp2 = sut.get();
	EXPECT_EQ( hp2, &dummy1 );
	EXPECT_TRUE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy1 ) );
}

TEST_F( TestHazardPtrHandler, CallCopyConstructor )
{
	// Arrange
	int                                        dummy1 = 1;
	alpha::concurrent::hazard_ptr_handler<int> src( &dummy1 );

	auto hp1 = src.get();
	EXPECT_EQ( hp1, &dummy1 );

	// Act
	alpha::concurrent::hazard_ptr_handler<int> sut( src );

	// Assert
	auto hp2 = sut.get();
	EXPECT_EQ( hp2, &dummy1 );
	EXPECT_TRUE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy1 ) );
}

TEST_F( TestHazardPtrHandler, CallMoveConstructor )
{
	// Arrange
	int                                        dummy1 = 1;
	alpha::concurrent::hazard_ptr_handler<int> src( &dummy1 );

	auto hp1 = src.get();
	EXPECT_EQ( hp1, &dummy1 );

	// Act
	alpha::concurrent::hazard_ptr_handler<int> sut( std::move( src ) );

	// Assert
	hp1 = src.get();
	EXPECT_EQ( hp1, nullptr );
	auto hp2 = sut.get();
	EXPECT_EQ( hp2, &dummy1 );
	EXPECT_TRUE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy1 ) );
}

TEST_F( TestHazardPtrHandler, CallCopyAssingment )
{
	// Arrange
	int                                        dummy1 = 1;
	int                                        dummy2 = 2;
	alpha::concurrent::hazard_ptr_handler<int> src( &dummy1 );
	alpha::concurrent::hazard_ptr_handler<int> sut( &dummy2 );

	auto hp1 = src.get();
	EXPECT_EQ( hp1, &dummy1 );
	auto hp2 = sut.get();
	EXPECT_EQ( hp2, &dummy2 );
	EXPECT_TRUE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy1 ) );
	EXPECT_TRUE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy2 ) );

	// Act
	sut = src;

	// Assert
	hp1 = src.get();
	EXPECT_EQ( hp1, &dummy1 );
	hp2 = sut.get();
	EXPECT_EQ( hp2, &dummy1 );
	EXPECT_TRUE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy1 ) );
	EXPECT_FALSE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy2 ) );
}

TEST_F( TestHazardPtrHandler, CallMoveAssingment )
{
	// Arrange
	int                                        dummy1 = 1;
	int                                        dummy2 = 2;
	alpha::concurrent::hazard_ptr_handler<int> src( &dummy1 );
	alpha::concurrent::hazard_ptr_handler<int> sut( &dummy2 );

	auto hp1 = src.get();
	EXPECT_EQ( hp1, &dummy1 );
	auto hp2 = sut.get();
	EXPECT_EQ( hp2, &dummy2 );
	EXPECT_TRUE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy1 ) );
	EXPECT_TRUE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy2 ) );

	// Act
	sut = std::move( src );

	// Assert
	hp1 = src.get();
	EXPECT_EQ( hp1, nullptr );
	hp2 = sut.get();
	EXPECT_EQ( hp2, &dummy1 );
	EXPECT_TRUE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy1 ) );
	EXPECT_FALSE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( &dummy2 ) );
}

TEST_F( TestHazardPtrHandler, Call_HazardPtr_get1 )
{
	// Arrange
	int                                        dummy1 = 1;
	alpha::concurrent::hazard_ptr_handler<int> sut( &dummy1 );

	// Act
	auto hp2 = sut.get();

	// Assert
	EXPECT_EQ( *hp2, 1 );
}

TEST_F( TestHazardPtrHandler, Call_HazardPtr_get1_and_assignment )
{
	// Arrange
	int                                        dummy1 = 1;
	alpha::concurrent::hazard_ptr_handler<int> sut( &dummy1 );

	auto hp2 = sut.get();
	EXPECT_EQ( *hp2, 1 );

	// Act
	*hp2 = 2;

	// Assert
	EXPECT_EQ( dummy1, 2 );
}

TEST_F( TestHazardPtrHandler, Call_HazardPtr_get2 )
{
	// Arrange
	struct A {
		int x;
		int y;
	};
	A                                        dummy1 { 1, 2 };
	alpha::concurrent::hazard_ptr_handler<A> sut( &dummy1 );

	// Act
	auto hp2 = sut.get();

	// Assert
	EXPECT_EQ( hp2->x, 1 );
	EXPECT_EQ( hp2->y, 2 );
}

TEST_F( TestHazardPtrHandler, Call_HazardPtr_get2_assignment )
{
	// Arrange
	struct A {
		int x;
		int y;
	};
	A                                        dummy1 { 1, 2 };
	alpha::concurrent::hazard_ptr_handler<A> sut( &dummy1 );

	auto hp2 = sut.get();
	EXPECT_EQ( hp2->x, 1 );
	EXPECT_EQ( hp2->y, 2 );

	// Act
	hp2->x = 3;
	hp2->y = 4;

	// Assert
	EXPECT_EQ( dummy1.x, 3 );
	EXPECT_EQ( dummy1.y, 4 );
}

TEST_F( TestHazardPtrHandler, Call_HazardPtr_get_for_nullptr )
{
	// Arrange
	alpha::concurrent::hazard_ptr_handler<int> sut( nullptr );

	// Act
	auto hp2 = sut.get();

	// Assert
	EXPECT_EQ( hp2.get(), nullptr );
}
