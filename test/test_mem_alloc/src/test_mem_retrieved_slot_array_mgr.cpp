/**
 * @file test_mem_retrieved_slot_array_mgr.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-26
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include <memory>

#include "gtest/gtest.h"

#include "mem_retrieved_slot_array_mgr.hpp"
#include "mem_small_memory_slot.hpp"

using tut1 = alpha::concurrent::internal::retrieved_slots_stack<alpha::concurrent::internal::slot_link_info>;

TEST( Test_RetrievedSlotsStack, CanConstruct )
{
	// Arrange

	// Act
	tut1 sut;

	// Assert
	EXPECT_EQ( 0, sut.count() );
	EXPECT_TRUE( sut.is_empty() );
}

TEST( Test_RetrievedSlotsStack, CanPushWithNullPtr )
{
	// Arrange
	tut1 sut;

	// Act
	alpha::concurrent::internal::slot_link_info* p = nullptr;
	sut.push( p );

	// Assert
	EXPECT_EQ( 0, sut.count() );
	EXPECT_TRUE( sut.is_empty() );
}

TEST( Test_RetrievedSlotsStack, CanPushOneElement )
{
	// Arrange
	tut1                                         sut;
	unsigned char                                buffer[1024];
	alpha::concurrent::internal::slot_link_info* p_sli = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer, nullptr );

	// Act
	sut.push( p_sli );

	// Assert
	EXPECT_EQ( 1, sut.count() );
	EXPECT_FALSE( sut.is_empty() );
}

TEST( Test_RetrievedSlotsStack, CanPushTwoElement )
{
	// Arrange
	tut1                                         sut;
	unsigned char                                buffer1[1024];
	alpha::concurrent::internal::slot_link_info* p_sli1 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer1, nullptr );
	unsigned char                                buffer2[1024];
	alpha::concurrent::internal::slot_link_info* p_sli2 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer2, nullptr );

	// Act
	sut.push( p_sli1 );
	sut.push( p_sli2 );

	// Assert
	EXPECT_EQ( 2, sut.count() );
}

TEST( Test_RetrievedSlotsStack, CanPopOneElement )
{
	// Arrange
	tut1                                         sut;
	unsigned char                                buffer[1024];
	alpha::concurrent::internal::slot_link_info* p_sli = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer, nullptr );
	sut.push( p_sli );
	EXPECT_EQ( 1, sut.count() );

	// Act
	auto p = sut.pop();

	// Assert
	EXPECT_EQ( 0, sut.count() );
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_EQ( p_sli, p );
}

TEST( Test_RetrievedSlotsStack, CanPopTwoElement )
{
	// Arrange
	tut1                                         sut;
	unsigned char                                buffer1[1024];
	alpha::concurrent::internal::slot_link_info* p_sli1 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer1, nullptr );
	unsigned char                                buffer2[1024];
	alpha::concurrent::internal::slot_link_info* p_sli2 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer2, nullptr );
	sut.push( p_sli1 );
	sut.push( p_sli2 );
	EXPECT_EQ( 2, sut.count() );

	// Act
	auto p1 = sut.pop();
	auto p2 = sut.pop();

	// Assert
	EXPECT_EQ( 0, sut.count() );
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_EQ( p_sli2, p1 );
	EXPECT_EQ( p_sli1, p2 );
}

TEST( Test_RetrievedSlotsStack, OneElement_CanPopTwoElement_Then_ValidAndNullPtr )
{
	// Arrange
	tut1                                         sut;
	unsigned char                                buffer1[1024];
	alpha::concurrent::internal::slot_link_info* p_sli1 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer1, nullptr );
	sut.push( p_sli1 );
	EXPECT_EQ( 1, sut.count() );

	// Act
	auto p1 = sut.pop();
	auto p2 = sut.pop();

	// Assert
	EXPECT_EQ( 0, sut.count() );
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_EQ( p1, p_sli1 );
	EXPECT_EQ( p2, nullptr );
}

TEST( Test_RetrievedSlotsStack, CanMergeTwoStacks )
{
	// Arrange
	tut1                                         sut1;
	unsigned char                                buffer1[1024];
	alpha::concurrent::internal::slot_link_info* p_sli1 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer1, nullptr );
	unsigned char                                buffer2[1024];
	alpha::concurrent::internal::slot_link_info* p_sli2 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer2, nullptr );
	sut1.push( p_sli1 );
	sut1.push( p_sli2 );
	EXPECT_EQ( 2, sut1.count() );

	tut1                                         sut2;
	unsigned char                                buffer3[1024];
	alpha::concurrent::internal::slot_link_info* p_sli3 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer3, nullptr );
	unsigned char                                buffer4[1024];
	alpha::concurrent::internal::slot_link_info* p_sli4 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer4, nullptr );
	sut2.push( p_sli3 );
	sut2.push( p_sli4 );
	EXPECT_EQ( 2, sut2.count() );

	// Act
	sut1.merge( std::move( sut2 ) );

	// Assert
	EXPECT_EQ( 4, sut1.count() );
	EXPECT_FALSE( sut1.is_empty() );
	EXPECT_EQ( 0, sut2.count() );
	EXPECT_TRUE( sut2.is_empty() );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
using tut2 = alpha::concurrent::internal::retrieved_slots_stack_lockable<alpha::concurrent::internal::slot_link_info>;

TEST( Test_RetrievedSlotsStackLockable, CanConstruct )
{
	// Arrange

	// Act
	tut2 sut;

	// Assert
	EXPECT_EQ( 0, sut.count() );
	EXPECT_TRUE( sut.is_empty() );
}

TEST( Test_RetrievedSlotsStackLockable, CanPushWithNullPtr )
{
	// Arrange
	tut2                                         sut;
	alpha::concurrent::internal::slot_link_info* p = nullptr;

	// Act
	sut.try_push( p );

	// Assert
	EXPECT_EQ( 0, sut.count() );
	EXPECT_TRUE( sut.is_empty() );
}

TEST( Test_RetrievedSlotsStackLockable, CanPushOneElement )
{
	// Arrange
	tut2                                         sut;
	unsigned char                                buffer[1024];
	alpha::concurrent::internal::slot_link_info* p_sli = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer, nullptr );

	// Act
	sut.try_push( p_sli );

	// Assert
	EXPECT_EQ( 1, sut.count() );
	EXPECT_FALSE( sut.is_empty() );
}

TEST( Test_RetrievedSlotsStackLockable, CanPushTwoElement )
{
	// Arrange
	tut2                                         sut;
	unsigned char                                buffer1[1024];
	alpha::concurrent::internal::slot_link_info* p_sli1 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer1, nullptr );
	unsigned char                                buffer2[1024];
	alpha::concurrent::internal::slot_link_info* p_sli2 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer2, nullptr );

	// Act
	sut.try_push( p_sli1 );
	sut.try_push( p_sli2 );

	// Assert
	EXPECT_EQ( 2, sut.count() );
}

TEST( Test_RetrievedSlotsStackLockable, Empty_CanPopOneElement )
{
	// Arrange
	tut2 sut;

	// Act
	auto p = sut.try_pop();

	// Assert
	EXPECT_EQ( 0, sut.count() );
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_EQ( nullptr, p );
}

TEST( Test_RetrievedSlotsStackLockable, OneElement_CanPopOneElement )
{
	// Arrange
	tut2                                         sut;
	unsigned char                                buffer[1024];
	alpha::concurrent::internal::slot_link_info* p_sli = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer, nullptr );
	sut.try_push( p_sli );
	EXPECT_EQ( 1, sut.count() );

	// Act
	auto p = sut.try_pop();

	// Assert
	EXPECT_EQ( 0, sut.count() );
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_EQ( p_sli, p );
}

TEST( Test_RetrievedSlotsStackLockable, TwoElement_CanPopTwoElement )
{
	// Arrange
	tut2                                         sut;
	unsigned char                                buffer1[1024];
	alpha::concurrent::internal::slot_link_info* p_sli1 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer1, nullptr );
	unsigned char                                buffer2[1024];
	alpha::concurrent::internal::slot_link_info* p_sli2 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer2, nullptr );
	sut.try_push( p_sli1 );
	sut.try_push( p_sli2 );
	EXPECT_EQ( 2, sut.count() );

	// Act
	auto p1 = sut.try_pop();
	auto p2 = sut.try_pop();

	// Assert
	EXPECT_EQ( 0, sut.count() );
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_EQ( p_sli2, p1 );
	EXPECT_EQ( p_sli1, p2 );
}

TEST( Test_RetrievedSlotsStackLockable, OneElement_CanPopTwoElement_Then_ValidAndNullPtr )
{
	// Arrange
	tut2                                         sut;
	unsigned char                                buffer1[1024];
	alpha::concurrent::internal::slot_link_info* p_sli1 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer1, nullptr );
	sut.try_push( p_sli1 );
	EXPECT_EQ( 1, sut.count() );

	// Act
	auto p1 = sut.try_pop();
	auto p2 = sut.try_pop();

	// Assert
	EXPECT_EQ( 0, sut.count() );
	EXPECT_TRUE( sut.is_empty() );
	EXPECT_EQ( p1, p_sli1 );
	EXPECT_EQ( p2, nullptr );
}

TEST( Test_RetrievedSlotsStackLockable, CanMergeTwoStacks )
{
	// Arrange
	tut2                                         sut1;
	unsigned char                                buffer1[1024];
	alpha::concurrent::internal::slot_link_info* p_sli1 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer1, nullptr );
	unsigned char                                buffer2[1024];
	alpha::concurrent::internal::slot_link_info* p_sli2 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer2, nullptr );
	sut1.try_push( p_sli1 );
	sut1.try_push( p_sli2 );
	EXPECT_EQ( 2, sut1.count() );

	tut1                                         sut2;
	unsigned char                                buffer3[1024];
	alpha::concurrent::internal::slot_link_info* p_sli3 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer3, nullptr );
	unsigned char                                buffer4[1024];
	alpha::concurrent::internal::slot_link_info* p_sli4 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer4, nullptr );
	sut2.push( p_sli3 );
	sut2.push( p_sli4 );
	EXPECT_EQ( 2, sut2.count() );

	// Act
	sut1.merge( std::move( sut2 ) );

	// Assert
	EXPECT_EQ( 4, sut1.count() );
	EXPECT_FALSE( sut1.is_empty() );
	EXPECT_EQ( 0, sut2.count() );
	EXPECT_TRUE( sut2.is_empty() );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
using tut3 = alpha::concurrent::internal::retrieved_slots_stack_lockfree<alpha::concurrent::internal::slot_link_info>;

TEST( Test_RetrievedSlotsStackLockfree, CanConstruct )
{
	// Arrange

	// Act
	tut3 sut;

	// Assert
}

TEST( Test_RetrievedSlotsStackLockfree, CanPushWithNullPtr )
{
	// Arrange
	tut3                                         sut;
	alpha::concurrent::internal::slot_link_info* p = nullptr;

	// Act
	auto p_ret = sut.try_push( p );

	// Assert
	EXPECT_EQ( nullptr, p_ret );
}

TEST( Test_RetrievedSlotsStackLockfree, CanPushOneElement )
{
	// Arrange
	tut3                                         sut;
	unsigned char                                buffer[1024];
	alpha::concurrent::internal::slot_link_info* p_sli = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer, nullptr );

	// Act
	auto p_ret = sut.try_push( p_sli );

	// Assert
	EXPECT_EQ( nullptr, p_ret );
}

TEST( Test_RetrievedSlotsStackLockfree, CanPushTwoElement )
{
	// Arrange
	tut3                                         sut;
	unsigned char                                buffer1[1024];
	alpha::concurrent::internal::slot_link_info* p_sli1 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer1, nullptr );
	unsigned char                                buffer2[1024];
	alpha::concurrent::internal::slot_link_info* p_sli2 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer2, nullptr );

	// Act
	auto p_ret1 = sut.try_push( p_sli1 );
	auto p_ret2 = sut.try_push( p_sli2 );

	// Assert
	EXPECT_EQ( nullptr, p_ret1 );
	EXPECT_EQ( nullptr, p_ret2 );
}

TEST( Test_RetrievedSlotsStackLockfree, Empty_CanPopOneElement )
{
	// Arrange
	tut3 sut;

	// Act
	auto p = sut.try_pop();

	// Assert
	EXPECT_EQ( nullptr, p );
}

TEST( Test_RetrievedSlotsStackLockfree, OneElement_CanPopOneElement )
{
	// Arrange
	tut3                                         sut;
	unsigned char                                buffer[1024];
	alpha::concurrent::internal::slot_link_info* p_sli = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer, nullptr );
	sut.try_push( p_sli );

	// Act
	auto p = sut.try_pop();

	// Assert
	EXPECT_EQ( p_sli, p );
}

TEST( Test_RetrievedSlotsStackLockfree, TwoElement_CanPopTwoElement )
{
	// Arrange
	tut3                                         sut;
	unsigned char                                buffer1[1024];
	alpha::concurrent::internal::slot_link_info* p_sli1 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer1, nullptr );
	unsigned char                                buffer2[1024];
	alpha::concurrent::internal::slot_link_info* p_sli2 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer2, nullptr );
	sut.try_push( p_sli1 );
	sut.try_push( p_sli2 );

	// Act
	auto p1 = sut.try_pop();
	auto p2 = sut.try_pop();

	// Assert
	EXPECT_EQ( p_sli2, p1 );
	EXPECT_EQ( p_sli1, p2 );
}

TEST( Test_RetrievedSlotsStackLockfree, OneElement_CanPopTwoElement_Then_ValidAndNullPtr )
{
	// Arrange
	tut3                                         sut;
	unsigned char                                buffer1[1024];
	alpha::concurrent::internal::slot_link_info* p_sli1 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer1, nullptr );
	sut.try_push( p_sli1 );

	// Act
	auto p1 = sut.try_pop();
	auto p2 = sut.try_pop();

	// Assert
	EXPECT_EQ( p_sli1, p1 );
	EXPECT_EQ( nullptr, p2 );
}

TEST( Test_RetrievedSlotsStackLockfree, CanMergeTwoStacks )
{
	// Arrange
	tut3                                         sut1;
	unsigned char                                buffer1[1024];
	alpha::concurrent::internal::slot_link_info* p_sli1 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer1, nullptr );
	unsigned char                                buffer2[1024];
	alpha::concurrent::internal::slot_link_info* p_sli2 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer2, nullptr );
	sut1.try_push( p_sli1 );
	sut1.try_push( p_sli2 );

	tut1                                         sut2;
	unsigned char                                buffer3[1024];
	alpha::concurrent::internal::slot_link_info* p_sli3 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer3, nullptr );
	unsigned char                                buffer4[1024];
	alpha::concurrent::internal::slot_link_info* p_sli4 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer4, nullptr );
	sut2.push( p_sli3 );
	sut2.push( p_sli4 );

	// Act
	sut1.merge( std::move( sut2 ) );

	// Assert
	auto p1 = sut1.try_pop();
	auto p2 = sut1.try_pop();
	auto p3 = sut1.try_pop();
	auto p4 = sut1.try_pop();
	auto p5 = sut1.try_pop();

	EXPECT_EQ( p1, p_sli3 );
	EXPECT_EQ( p2, p_sli4 );
	EXPECT_EQ( p3, p_sli2 );
	EXPECT_EQ( p4, p_sli1 );
	EXPECT_EQ( p5, nullptr );

	auto p6 = sut2.pop();
	EXPECT_EQ( nullptr, p6 );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
using tut4 = alpha::concurrent::internal::retrieved_slots_stack_array_mgr<alpha::concurrent::internal::slot_link_info>;

TEST( Test_RetrievedSlotsStackArrayMgr, CanPushWithNullPtr )
{
	// Arrange
	tut4::reset_for_test();

	// Act
	tut4::retrieve( 0, nullptr );

	// Assert
	auto p = tut4::request_reuse( 0 );
	EXPECT_EQ( p, nullptr );
}

TEST( Test_RetrievedSlotsStackArrayMgr, CanPush )
{
	// Arrange
	tut4::reset_for_test();
	unsigned char                                buffer1[1024];
	alpha::concurrent::internal::slot_link_info* p_sli1 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer1, nullptr );

	// Act
	tut4::retrieve( 0, p_sli1 );

	// Assert
	auto p = tut4::request_reuse( 0 );
	EXPECT_EQ( p_sli1, p );
}

TEST( Test_RetrievedSlotsStackArrayMgr, CanPushTwoElement )
{
	// Arrange
	tut4::reset_for_test();
	unsigned char                                buffer1[1024];
	alpha::concurrent::internal::slot_link_info* p_sli1 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer1, nullptr );
	unsigned char                                buffer2[1024];
	alpha::concurrent::internal::slot_link_info* p_sli2 = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer2, nullptr );

	// Act
	tut4::retrieve( 0, p_sli1 );
	tut4::retrieve( 0, p_sli2 );

	// Assert
	auto p1 = tut4::request_reuse( 0 );
	auto p2 = tut4::request_reuse( 0 );
	EXPECT_TRUE( ( ( p_sli2 == p1 ) && ( p_sli1 == p2 ) ) || ( ( p_sli2 == p2 ) && ( p_sli1 == p1 ) ) );
}

TEST( Test_RetrievedSlotsStackArrayMgr, Empty_CanPop_Then_ReturnNullPtr )
{
	// Arrange
	tut4::reset_for_test();

	// Act
	auto p = tut4::request_reuse( 0 );

	// Assert
	EXPECT_EQ( p, nullptr );
}
