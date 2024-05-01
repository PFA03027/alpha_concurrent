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

#include "../src/hazard_ptr_internal.hpp"
#include "alconcurrent/conf_logger.hpp"

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

TEST( HazardPtrGroup, CanTryAssingWithFull )
{
	// Arrange
	alpha::concurrent::internal::hazard_ptr_group                        sut;
	char                                                                 dummy = 1;
	int                                                                  dummy_array[alpha::concurrent::internal::hazard_ptr_group::kArraySize];
	alpha::concurrent::internal::hazard_ptr_group::hzrd_slot_ownership_t righofownership_array[alpha::concurrent::internal::hazard_ptr_group::kArraySize];
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
TEST( DestroyFunc, CallDestroyAll )
{
	{
		// Arrange
		alpha::concurrent::internal::bind_hazard_ptr_list sut1;
		alpha::concurrent::internal::bind_hazard_ptr_list sut2;

		char                                                                 dummy1 = 1;
		char                                                                 dummy2 = 2;
		int                                                                  dummy_array[alpha::concurrent::internal::hazard_ptr_group::kArraySize];
		alpha::concurrent::internal::hazard_ptr_group::hzrd_slot_ownership_t righofownership_array[alpha::concurrent::internal::hazard_ptr_group::kArraySize];
		for ( size_t i = 0; i < alpha::concurrent::internal::hazard_ptr_group::kArraySize; i++ ) {
			righofownership_array[i] = sut1.assign( &dummy_array[i] );
			EXPECT_NE( righofownership_array[i], nullptr );
		}
		auto righofownership1 = sut1.assign( &dummy1 );
		EXPECT_NE( righofownership1, nullptr );
		auto righofownership2 = sut2.assign( &dummy2 );
		EXPECT_NE( righofownership2, nullptr );
	}

	// Act
	alpha::concurrent::internal::global_scope_hazard_ptr_chain::DestoryAll();

	// Assert
	// no memory leak
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
		alpha::concurrent::internal::global_scope_hazard_ptr_chain::DestoryAll();

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
	auto ownership = sut.assign( &dummy );

	// Assert
	EXPECT_NE( ownership, nullptr );
}

TEST_F( TestBindHazardPtrList, CallAssingWithFull )
{
	// Arrange
	alpha::concurrent::internal::bind_hazard_ptr_list                        sut;
	char                                                                     dummy = 1;
	int                                                                      dummy_array[alpha::concurrent::internal::hazard_ptr_group::kArraySize];
	alpha::concurrent::internal::bind_hazard_ptr_list::hzrd_slot_ownership_t righofownership_array[alpha::concurrent::internal::hazard_ptr_group::kArraySize];
	for ( size_t i = 0; i < alpha::concurrent::internal::hazard_ptr_group::kArraySize; i++ ) {
		righofownership_array[i] = sut.assign( &dummy_array[i] );
		EXPECT_NE( righofownership_array[i], nullptr );
	}

	// Act
	auto righofownership = sut.assign( &dummy );

	// Assert
	EXPECT_NE( righofownership, nullptr );
}
