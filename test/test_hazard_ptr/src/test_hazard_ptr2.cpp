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
#include "hazard_ptr_impl.hpp"

class TestHazardPtrGroup : public ::testing::Test {
protected:
	void SetUp() override
	{
		alpha::concurrent::GetErrorWarningLogCountAndReset( nullptr, nullptr );
	}

	void TearDown() override
	{
		int cw, ce;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
		EXPECT_EQ( ce, 0 );
		EXPECT_EQ( cw, 0 );
	}
};

TEST_F( TestHazardPtrGroup, CanDefaultConstruct )
{
	// Arrange

	// Act
	alpha::concurrent::internal::hazard_ptr_group sut;

	// Assert
}

TEST_F( TestHazardPtrGroup, CanTryAssing )
{
	// Arrange
	alpha::concurrent::internal::hazard_ptr_group sut;
	char                                          dummy = 1;

	// Act
	auto righofownership = sut.try_assign( &dummy );

	// Assert
	EXPECT_NE( righofownership, nullptr );
}

TEST_F( TestHazardPtrGroup, CanTryAssingForNullptr )
{
	// Arrange
	alpha::concurrent::internal::hazard_ptr_group sut;

	// Act
	auto righofownership = sut.try_assign( nullptr );

	// Assert
	EXPECT_EQ( righofownership, nullptr );
}

TEST_F( TestHazardPtrGroup, CanTryAssingWithFull )
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

TEST_F( TestHazardPtrGroup, CallChkHazardNullptr )
{
	// Arrange
	alpha::concurrent::internal::hazard_ptr_group sut;

	// Act
	bool ret = sut.check_pointer_is_hazard_pointer( nullptr );

	// Assert
	EXPECT_FALSE( ret );
}

TEST_F( TestHazardPtrGroup, CallChkHazardPtr1 )
{
	// Arrange
	alpha::concurrent::internal::hazard_ptr_group sut;
	char                                          dummy = 1;

	// Act
	bool ret = sut.check_pointer_is_hazard_pointer( &dummy );

	// Assert
	EXPECT_FALSE( ret );
}

TEST_F( TestHazardPtrGroup, CallChkHazardPtr2 )
{
	// Arrange
	alpha::concurrent::internal::hazard_ptr_group sut;
	char                                          dummy           = 1;
	auto                                          righofownership = sut.try_assign( &dummy );
	EXPECT_NE( righofownership, nullptr );

	// Act
	bool ret = sut.check_pointer_is_hazard_pointer( &dummy );

	// Assert
	EXPECT_TRUE( ret );
}

TEST_F( TestHazardPtrGroup, CallChkHazardPtrFull )
{
	// Arrange
	alpha::concurrent::internal::hazard_ptr_group sut;
	char                                          dummy1          = 1;
	char                                          dummy2          = 2;
	auto                                          righofownership = sut.try_assign( &dummy1 );
	EXPECT_NE( righofownership, nullptr );

	// Act
	bool ret = sut.check_pointer_is_hazard_pointer( &dummy2 );

	// Assert
	EXPECT_FALSE( ret );
}

TEST_F( TestHazardPtrGroup, CallChkHazardPtr4 )
{
	alpha::concurrent::internal::hazard_ptr_group      sut;
	char                                               dummy = 1;
	int                                                dummy_array[alpha::concurrent::internal::hazard_ptr_group::kArraySize];
	alpha::concurrent::internal::hzrd_slot_ownership_t righofownership_array[alpha::concurrent::internal::hazard_ptr_group::kArraySize];
	for ( size_t i = 0; i < alpha::concurrent::internal::hazard_ptr_group::kArraySize; i++ ) {
		righofownership_array[i] = sut.try_assign( &dummy_array[i] );
		EXPECT_NE( righofownership_array[i], nullptr );
	}

	for ( size_t i = 0; i < alpha::concurrent::internal::hazard_ptr_group::kArraySize; i++ ) {

		// Act
		bool ret = sut.check_pointer_is_hazard_pointer( &dummy_array[i] );

		// Assert
		EXPECT_TRUE( ret );
	}

	// Act
	bool ret = sut.check_pointer_is_hazard_pointer( &dummy );

	// Assert
	EXPECT_FALSE( ret );
}

/////////////////////////////////////////////////////////////////////////////////
class TestGlobalScopeHazardPtrChain : public ::testing::Test {
public:
	TestGlobalScopeHazardPtrChain( void )
	  : ::testing::Test()
	  , sut_()
	{
	}

protected:
	void SetUp() override
	{
		alpha::concurrent::GetErrorWarningLogCountAndReset( nullptr, nullptr );
	}

	void TearDown() override
	{
		alpha::concurrent::internal::retire_mgr::stop_prune_thread();

		int cw, ce;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
		EXPECT_EQ( ce, 0 );
		EXPECT_EQ( cw, 0 );
	}

	alpha::concurrent::internal::global_scope_hazard_ptr_chain sut_;
};

TEST_F( TestGlobalScopeHazardPtrChain, CallRemoveAll )
{
	// Arrange

	// Act
	sut_.remove_all();

	// Assert
	EXPECT_TRUE( sut_.is_empty() );
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

/////////////////////////////////////////////////////////////////////////////////
class TestHazardPtr : public ::testing::Test {
protected:
	using test_type = int;

	void SetUp() override
	{
		alpha::concurrent::GetErrorWarningLogCountAndReset( nullptr, nullptr );

		dummy_ = 1;
		hph_.store( &dummy_ );
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

	test_type                                        dummy_;
	alpha::concurrent::hazard_ptr_handler<test_type> hph_;
};

TEST_F( TestHazardPtr, Call_GetHazardPtr1 )
{
	// Arrange
	auto sut = hph_.get();

	// Act
	test_type* p_ret = sut.get();

	// Assert
	EXPECT_NE( p_ret, nullptr );
	EXPECT_TRUE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( p_ret ) );
}

TEST_F( TestHazardPtr, Call_GetHazardPtr2 )
{
	// Arrange
	auto sut1 = hph_.get();
	auto sut2 = hph_.get();

	// Act
	test_type* p_ret1 = sut1.get();
	test_type* p_ret2 = sut2.get();

	// Assert
	EXPECT_NE( p_ret1, nullptr );
	EXPECT_TRUE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( p_ret1 ) );
	EXPECT_NE( p_ret2, nullptr );
	EXPECT_TRUE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( p_ret2 ) );
}

TEST_F( TestHazardPtr, Call_ReleaseHazardPtr_by_destructor1 )
{
	// Arrange
	test_type* p_ret;
	{
		auto sut = hph_.get();
		p_ret    = sut.get();
		EXPECT_NE( p_ret, nullptr );
		EXPECT_TRUE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( p_ret ) );
	}

	// Act
	bool ret = alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( p_ret );

	// Assert
	EXPECT_FALSE( ret );
}

TEST_F( TestHazardPtr, Call_ReleaseHazardPtr_by_destructor2 )
{
	// Arrange
	test_type* p_ret1;
	test_type* p_ret2;
	auto       sut1 = hph_.get();
	{
		auto sut2 = hph_.get();

		// Act
		p_ret1 = sut1.get();
		p_ret2 = sut2.get();
	}

	// Assert
	EXPECT_NE( p_ret1, nullptr );
	EXPECT_TRUE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( p_ret1 ) );
	EXPECT_NE( p_ret2, nullptr );
	EXPECT_TRUE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( p_ret2 ) );
}

TEST_F( TestHazardPtr, Call_ReleaseHazardPtr_by_destructor3 )
{
	// Arrange
	test_type* p_ret1;
	test_type* p_ret2;
	{
		auto sut1 = hph_.get();
		auto sut2 = hph_.get();

		// Act
		p_ret1 = sut1.get();
		p_ret2 = sut2.get();
	}

	// Assert
	EXPECT_NE( p_ret1, nullptr );
	EXPECT_FALSE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( p_ret1 ) );
	EXPECT_NE( p_ret2, nullptr );
	EXPECT_FALSE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( p_ret2 ) );
}

TEST_F( TestHazardPtr, Call_ReleaseHazardPtr_by_assignment )
{
	// Arrange
	test_type* p_ret1;
	test_type* p_ret2;
	auto       sut = hph_.get();
	p_ret1         = sut.get();
	EXPECT_NE( p_ret1, nullptr );
	EXPECT_TRUE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( p_ret1 ) );
	int dummy2 = 2;
	hph_.store( &dummy2 );

	// Act
	sut = hph_.get();

	// Assert
	EXPECT_FALSE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( p_ret1 ) );
	p_ret2 = sut.get();
	EXPECT_TRUE( alpha::concurrent::internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( p_ret2 ) );
}
