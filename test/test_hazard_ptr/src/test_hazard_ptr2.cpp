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

TEST_F( TestHazardPtrGroup, CallPushFrontToValidChain )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut;

	// Act
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_EQ( aaddr_valid_chain_next_.load(), reinterpret_cast<std::uintptr_t>( &sut ) );
}

TEST_F( TestHazardPtrGroup, CallSearchWithNullChain )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut;

	// Act
	bool ret = alpha::concurrent::internal::hazard_ptr_group::is_hazard_ptr_group_in_valid_chain( &sut, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_FALSE( ret );
}

TEST_F( TestHazardPtrGroup, CallSearchTop1 )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut;
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut, &aaddr_valid_chain_next_ );

	// Act
	bool ret = alpha::concurrent::internal::hazard_ptr_group::is_hazard_ptr_group_in_valid_chain( &sut, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_TRUE( ret );
}

TEST_F( TestHazardPtrGroup, CallSearchTop2 )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut1;
	alpha::concurrent::internal::hazard_ptr_group sut2;
	alpha::concurrent::internal::hazard_ptr_group sut3;
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut1, &aaddr_valid_chain_next_ );

	// Act
	bool ret = alpha::concurrent::internal::hazard_ptr_group::is_hazard_ptr_group_in_valid_chain( &sut1, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_TRUE( ret );
}

TEST_F( TestHazardPtrGroup, CallSearchSecond )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut1;
	alpha::concurrent::internal::hazard_ptr_group sut2;
	alpha::concurrent::internal::hazard_ptr_group sut3;
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut1, &aaddr_valid_chain_next_ );

	// Act
	bool ret = alpha::concurrent::internal::hazard_ptr_group::is_hazard_ptr_group_in_valid_chain( &sut2, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_TRUE( ret );
}

TEST_F( TestHazardPtrGroup, CallSearchThird )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut1;
	alpha::concurrent::internal::hazard_ptr_group sut2;
	alpha::concurrent::internal::hazard_ptr_group sut3;
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut1, &aaddr_valid_chain_next_ );

	// Act
	bool ret = alpha::concurrent::internal::hazard_ptr_group::is_hazard_ptr_group_in_valid_chain( &sut3, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_TRUE( ret );
}

TEST_F( TestHazardPtrGroup, CallSearchNotExist )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut1;
	alpha::concurrent::internal::hazard_ptr_group sut2;
	alpha::concurrent::internal::hazard_ptr_group sut3;
	alpha::concurrent::internal::hazard_ptr_group sut4;
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut1, &aaddr_valid_chain_next_ );

	// Act
	bool ret = alpha::concurrent::internal::hazard_ptr_group::is_hazard_ptr_group_in_valid_chain( &sut4, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_FALSE( ret );
}

TEST_F( TestHazardPtrGroup, CallSearchNullptr )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut1;
	alpha::concurrent::internal::hazard_ptr_group sut2;
	alpha::concurrent::internal::hazard_ptr_group sut3;
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut1, &aaddr_valid_chain_next_ );

	// Act
	bool ret = alpha::concurrent::internal::hazard_ptr_group::is_hazard_ptr_group_in_valid_chain( nullptr, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_FALSE( ret );
}

TEST_F( TestHazardPtrGroup, CallRemoveTopFromOneValidChain )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut;
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut, &aaddr_valid_chain_next_ );

	// Act
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_EQ( aaddr_valid_chain_next_.load(), 0 );
	bool ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
}

TEST_F( TestHazardPtrGroup, CallRemoveTopFromValidChain )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut1;
	alpha::concurrent::internal::hazard_ptr_group sut2;
	alpha::concurrent::internal::hazard_ptr_group sut3;
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut1, &aaddr_valid_chain_next_ );

	// Act
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut1, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_EQ( aaddr_valid_chain_next_.load(), reinterpret_cast<std::uintptr_t>( &sut2 ) );
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), reinterpret_cast<std::uintptr_t>( &sut3 ) );
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), 0 );
	bool ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut1.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut2.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut3.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
}

TEST_F( TestHazardPtrGroup, CallRemoveMidFromValidChain )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut1;
	alpha::concurrent::internal::hazard_ptr_group sut2;
	alpha::concurrent::internal::hazard_ptr_group sut3;
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut1, &aaddr_valid_chain_next_ );

	// Act
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut2, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_EQ( aaddr_valid_chain_next_.load(), reinterpret_cast<std::uintptr_t>( &sut1 ) );
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut1, &aaddr_valid_chain_next_ );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), reinterpret_cast<std::uintptr_t>( &sut3 ) );
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), 0 );
	bool ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut1.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut2.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut3.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
}

TEST_F( TestHazardPtrGroup, CallRemoveLastFromValidChain )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut1;
	alpha::concurrent::internal::hazard_ptr_group sut2;
	alpha::concurrent::internal::hazard_ptr_group sut3;
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut1, &aaddr_valid_chain_next_ );

	// Act
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut3, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_EQ( aaddr_valid_chain_next_.load(), reinterpret_cast<std::uintptr_t>( &sut1 ) );
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut1, &aaddr_valid_chain_next_ );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), reinterpret_cast<std::uintptr_t>( &sut2 ) );
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), 0 );
	bool ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut1.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut2.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut3.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
}

TEST_F( TestHazardPtrGroup, CallSearchWithDelMarkAt1st )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut1;
	alpha::concurrent::internal::hazard_ptr_group sut2;
	alpha::concurrent::internal::hazard_ptr_group sut3;
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut1, &aaddr_valid_chain_next_ );
	sut1.get_valid_chain_next_writer_accesser().set_del_mark();

	// Act
	bool ret = alpha::concurrent::internal::hazard_ptr_group::is_hazard_ptr_group_in_valid_chain( &sut1, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_FALSE( ret );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), reinterpret_cast<std::uintptr_t>( &sut2 ) );
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), reinterpret_cast<std::uintptr_t>( &sut3 ) );
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), 0 );
	bool ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut1.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut2.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut3.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
}

TEST_F( TestHazardPtrGroup, CallSearchWithDelMarkAt2nd )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut1;
	alpha::concurrent::internal::hazard_ptr_group sut2;
	alpha::concurrent::internal::hazard_ptr_group sut3;
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut1, &aaddr_valid_chain_next_ );
	sut2.get_valid_chain_next_writer_accesser().set_del_mark();

	// Act
	bool ret = alpha::concurrent::internal::hazard_ptr_group::is_hazard_ptr_group_in_valid_chain( &sut2, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_FALSE( ret );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), reinterpret_cast<std::uintptr_t>( &sut1 ) );
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut1, &aaddr_valid_chain_next_ );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), reinterpret_cast<std::uintptr_t>( &sut3 ) );
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), 0 );
	bool ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut1.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut2.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut3.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
}

TEST_F( TestHazardPtrGroup, CallSearchWithDelMarkAt3rd )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut1;
	alpha::concurrent::internal::hazard_ptr_group sut2;
	alpha::concurrent::internal::hazard_ptr_group sut3;
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut1, &aaddr_valid_chain_next_ );
	sut3.get_valid_chain_next_writer_accesser().set_del_mark();

	// Act
	bool ret = alpha::concurrent::internal::hazard_ptr_group::is_hazard_ptr_group_in_valid_chain( &sut3, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_FALSE( ret );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), reinterpret_cast<std::uintptr_t>( &sut1 ) );
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut1, &aaddr_valid_chain_next_ );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), reinterpret_cast<std::uintptr_t>( &sut2 ) );
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), 0 );
	bool ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut1.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut2.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut3.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
}

TEST_F( TestHazardPtrGroup, CallSearchWithDelMarkAt1st3rd )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut1;
	alpha::concurrent::internal::hazard_ptr_group sut2;
	alpha::concurrent::internal::hazard_ptr_group sut3;
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut1, &aaddr_valid_chain_next_ );
	sut1.get_valid_chain_next_writer_accesser().set_del_mark();
	sut3.get_valid_chain_next_writer_accesser().set_del_mark();

	// Act
	bool ret = alpha::concurrent::internal::hazard_ptr_group::is_hazard_ptr_group_in_valid_chain( &sut1, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_FALSE( ret );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), reinterpret_cast<std::uintptr_t>( &sut2 ) );
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), 0 );
	bool ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut1.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut2.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut3.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
}

TEST_F( TestHazardPtrGroup, CallSearchWithDelMarkAt1st2nd3rd )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut1;
	alpha::concurrent::internal::hazard_ptr_group sut2;
	alpha::concurrent::internal::hazard_ptr_group sut3;
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut1, &aaddr_valid_chain_next_ );
	sut1.get_valid_chain_next_writer_accesser().set_del_mark();
	sut2.get_valid_chain_next_writer_accesser().set_del_mark();
	sut3.get_valid_chain_next_writer_accesser().set_del_mark();

	// Act
	bool ret = alpha::concurrent::internal::hazard_ptr_group::is_hazard_ptr_group_in_valid_chain( &sut1, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_FALSE( ret );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), 0 );
	bool ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut1.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut2.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut3.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
}

TEST_F( TestHazardPtrGroup, CallPush1stWithDelMarkAt2nd3rd )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut1;
	alpha::concurrent::internal::hazard_ptr_group sut2;
	alpha::concurrent::internal::hazard_ptr_group sut3;
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	sut2.get_valid_chain_next_writer_accesser().set_del_mark();
	sut3.get_valid_chain_next_writer_accesser().set_del_mark();

	// Act
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut1, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_EQ( aaddr_valid_chain_next_.load(), reinterpret_cast<std::uintptr_t>( &sut1 ) );
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut1, &aaddr_valid_chain_next_ );
	bool ret = alpha::concurrent::internal::hazard_ptr_group::is_hazard_ptr_group_in_valid_chain( &sut1, &aaddr_valid_chain_next_ );
	EXPECT_FALSE( ret );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), 0 );
	bool ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut1.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut2.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut3.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
}

TEST_F( TestHazardPtrGroup, CallSearchWithDelMarkAt2nd3rd )
{
	// Arrange
	std::atomic<std::uintptr_t>                   aaddr_valid_chain_next_( 0 );
	alpha::concurrent::internal::hazard_ptr_group sut1;
	alpha::concurrent::internal::hazard_ptr_group sut2;
	alpha::concurrent::internal::hazard_ptr_group sut3;
	alpha::concurrent::internal::hazard_ptr_group sut4;
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut4, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut3, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut2, &aaddr_valid_chain_next_ );
	alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( &sut1, &aaddr_valid_chain_next_ );
	sut2.get_valid_chain_next_writer_accesser().set_del_mark();
	sut3.get_valid_chain_next_writer_accesser().set_del_mark();

	// Act
	bool ret = alpha::concurrent::internal::hazard_ptr_group::is_hazard_ptr_group_in_valid_chain( &sut2, &aaddr_valid_chain_next_ );

	// Assert
	EXPECT_FALSE( ret );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), reinterpret_cast<std::uintptr_t>( &sut1 ) );
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut1, &aaddr_valid_chain_next_ );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), reinterpret_cast<std::uintptr_t>( &sut4 ) );
	alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( &sut4, &aaddr_valid_chain_next_ );
	EXPECT_EQ( aaddr_valid_chain_next_.load(), 0 );
	bool ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut1.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut2.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut3.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
	ret_is_del_marked = alpha::concurrent::internal::is_del_marked( sut4.get_valid_chain_next_reader_accesser().load_address() );
	EXPECT_TRUE( ret_is_del_marked );
}

#if 1
TEST_F( TestHazardPtrGroup, HighLoadValidChain )
{
	// Arrange
	constexpr int                                 array_size = 10;
	constexpr int                                 th_num     = 8;
	std::atomic<std::uintptr_t>                   sut( 0 );
	alpha::concurrent::internal::hazard_ptr_group hpg[array_size * th_num];
	std::thread                                   test_load_th_obj[th_num];
	pthread_barrier_t                             barrier2;
	pthread_barrier_init( &barrier2, NULL, th_num + 1 );

	struct load_object {
		void operator()(
			int                                                  my_tid,
			pthread_barrier_t* const                             p_barrier,
			std::atomic<std::uintptr_t>* const                   p_addr_top,
			alpha::concurrent::internal::hazard_ptr_group* const p_begin,
			alpha::concurrent::internal::hazard_ptr_group* const p_end )
		{
			constexpr int loop_num = 100000;

			pthread_barrier_wait( p_barrier );
			for ( int i = 0; i < loop_num; i++ ) {
				int                                            j = 0;
				alpha::concurrent::internal::hazard_ptr_group* p_cur;
				for ( p_cur = p_begin; p_cur != p_end; p_cur++ ) {
					alpha::concurrent::internal::hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( p_cur, p_addr_top );
					if ( alpha::concurrent::internal::is_del_marked( p_cur->get_valid_chain_next_reader_accesser().load_address() ) ) {
						std::cout << std::string( "should not be del marked1" ) << std::endl;
						throw std::logic_error( "should not be del marked" );
					}
				}
				j = 0;
				for ( p_cur = p_begin; p_cur != p_end; p_cur++ ) {
					std::uintptr_t tmp_addr1 = p_cur->get_valid_chain_next_reader_accesser().load_address();
					if ( alpha::concurrent::internal::is_del_marked( tmp_addr1 ) ) {
						std::cout << std::string( "should not be del marked2" ) << std::endl;
						throw std::logic_error( "should not be del marked" );
					}
					alpha::concurrent::internal::hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( p_cur, p_addr_top );
					std::uintptr_t tmp_addr2 = p_cur->get_valid_chain_next_reader_accesser().load_address();
					if ( !alpha::concurrent::internal::is_del_marked( tmp_addr2 ) ) {
						// if ( my_tid == 0 ) {
						std::cout << std::string( "should be del marked  " ) + std::to_string( j ) + std::string( "  " ) +
										 std::to_string( tmp_addr1 ) + std::string( "  " ) + std::to_string( tmp_addr2 )
								  << std::endl;
						throw std::logic_error( std::string( "should be del marked  " ) + std::to_string( tmp_addr1 ) + std::string( "  " ) + std::to_string( tmp_addr2 ) );
						// }
					}
					j++;
				}
				if ( alpha::concurrent::internal::is_del_marked( p_addr_top->load( std::memory_order_acquire ) ) ) {
					throw std::logic_error( "p_addr_top should not have del marked" );
				}
			}
		}
	};
	for ( int i = 0; i < th_num; i++ ) {
		test_load_th_obj[i] = std::thread( load_object {}, i, &barrier2, &sut, &( hpg[i * array_size] ), &( hpg[( i + 1 ) * array_size] ) );
	}

	// Act
	pthread_barrier_wait( &barrier2 );
	for ( int i = 0; i < th_num; i++ ) {
		if ( test_load_th_obj[i].joinable() ) {
			test_load_th_obj[i].join();
		}
	}

	// Assert
	EXPECT_EQ( sut.load(), 0 );
}
#endif

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
