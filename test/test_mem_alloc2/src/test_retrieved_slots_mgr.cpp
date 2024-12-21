/**
 * @file test_retrieved_slots_mgr.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-21
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "gtest/gtest.h"

#include "mem_small_memory_slot.hpp"

using tut = alpha::concurrent::internal::retrieved_slots_mgr;

TEST( Test_RetrievedSlotsMgr, CanConstruct )
{
	// Arrange

	// Act
	tut sut;

	// Assert
}

TEST( Test_RetrievedSlotsMgr, DoRetrieve )
{
	// Arrange
	tut                                          sut;
	unsigned char                                buffer[1024];
	alpha::concurrent::internal::slot_link_info* p_sli = alpha::concurrent::internal::slot_link_info::emplace_on_mem( buffer, nullptr );

	// Act
	sut.retrieve( p_sli );

	// Assert
}

TEST( Test_RetrievedSlotsMgr, Empty_DoRequestReuse_Then_ReturnNullptr )
{
	// Arrange
	tut sut;

	// Act
	auto p = sut.request_reuse();

	// Assert
	EXPECT_EQ( p, nullptr );
}

TEST( Test_RetrievedSlotsMgr, OneElement_DoRequestReuse_Then_ReturnElement )
{
	// Arrange
	tut sut;

	unsigned char buff_memory_slot_group[1024 * 4];
	auto          p_ret = alpha::concurrent::internal::memory_slot_group::emplace_on_mem( buff_memory_slot_group, nullptr, 1024 * 4, 15 );
	EXPECT_LE( 1, p_ret->num_slots_ );
	alpha::concurrent::internal::slot_link_info* p_sli = p_ret->assign_new_slot();
	EXPECT_NE( p_sli, nullptr );

	sut.retrieve( p_sli );

	// Act
	auto p = sut.request_reuse();

	// Assert
	EXPECT_EQ( p, p_sli );
}

TEST( Test_RetrievedSlotsMgr, TwoElement_DoRequestReuse_Then_ReturnElement )
{
	// Arrange
	tut sut;

	unsigned char buff_memory_slot_group[1024 * 8];
	auto          p_ret = alpha::concurrent::internal::memory_slot_group::emplace_on_mem( buff_memory_slot_group, nullptr, 1024 * 8, 15 );
	EXPECT_LE( 2, p_ret->num_slots_ );
	alpha::concurrent::internal::slot_link_info* p_sli1 = p_ret->assign_new_slot();
	EXPECT_NE( p_sli1, nullptr );
	alpha::concurrent::internal::slot_link_info* p_sli2 = p_ret->assign_new_slot();
	EXPECT_NE( p_sli2, nullptr );

	sut.retrieve( p_sli1 );
	sut.retrieve( p_sli2 );

	// Act
	auto p1 = sut.request_reuse();
	auto p2 = sut.request_reuse();

	// Assert
	EXPECT_EQ( p1, p_sli2 );
	EXPECT_EQ( p2, p_sli1 );
}

TEST( Test_RetrievedSlotsMgr, TwoElement_DoRequestReuseTwice_Then_ReturnNullptr )
{
	// Arrange
	tut sut;

	unsigned char buff_memory_slot_group[1024 * 8];
	auto          p_ret = alpha::concurrent::internal::memory_slot_group::emplace_on_mem( buff_memory_slot_group, nullptr, 1024 * 8, 15 );
	EXPECT_LE( 2, p_ret->num_slots_ );
	alpha::concurrent::internal::slot_link_info* p_sli1 = p_ret->assign_new_slot();
	EXPECT_NE( p_sli1, nullptr );
	alpha::concurrent::internal::slot_link_info* p_sli2 = p_ret->assign_new_slot();
	EXPECT_NE( p_sli2, nullptr );

	sut.retrieve( p_sli1 );
	sut.retrieve( p_sli2 );

	// Act
	auto p1 = sut.request_reuse();
	auto p2 = sut.request_reuse();
	auto p3 = sut.request_reuse();
	auto p4 = sut.request_reuse();

	// Assert
	EXPECT_EQ( p1, p_sli2 );
	EXPECT_EQ( p2, p_sli1 );
	EXPECT_EQ( p3, nullptr );
	EXPECT_EQ( p4, nullptr );
}

TEST( Test_RetrievedSlotsMgr, OneElementUsableOneElementInHazard_DoRequestReuse_Then_ReturnOneElementAvailable )
{
	// Arrange
	tut sut;

	unsigned char buff_memory_slot_group[1024 * 8];
	auto          p_ret = alpha::concurrent::internal::memory_slot_group::emplace_on_mem( buff_memory_slot_group, nullptr, 1024 * 8, 15 );
	EXPECT_LE( 2, p_ret->num_slots_ );
	alpha::concurrent::internal::slot_link_info* p_sli1 = p_ret->assign_new_slot();
	EXPECT_NE( p_sli1, nullptr );
	alpha::concurrent::internal::slot_link_info* p_sli2 = p_ret->assign_new_slot();
	EXPECT_NE( p_sli2, nullptr );

	alpha::concurrent::hazard_ptr_handler<alpha::concurrent::internal::slot_link_info> hph_sli2( p_sli2 );
	auto                                                                               hp_sli2 = hph_sli2.get_to_verify_exchange();

	sut.retrieve( p_sli1 );
	sut.retrieve( p_sli2 );

	// Act
	auto p1 = sut.request_reuse();
	auto p2 = sut.request_reuse();

	// Assert
	EXPECT_EQ( p1, p_sli1 );
	EXPECT_EQ( p2, nullptr );
}

TEST( Test_RetrievedSlotsMgr, OneElementUsableOneElementInHazard_DoRequestReuse_Then_ReturnOneElementAvailable2 )
{
	// Arrange
	tut sut;

	unsigned char buff_memory_slot_group[1024 * 8];
	auto          p_ret = alpha::concurrent::internal::memory_slot_group::emplace_on_mem( buff_memory_slot_group, nullptr, 1024 * 8, 15 );
	EXPECT_LE( 2, p_ret->num_slots_ );
	alpha::concurrent::internal::slot_link_info* p_sli1 = p_ret->assign_new_slot();
	EXPECT_NE( p_sli1, nullptr );
	alpha::concurrent::internal::slot_link_info* p_sli2 = p_ret->assign_new_slot();
	EXPECT_NE( p_sli2, nullptr );

	{
		alpha::concurrent::hazard_ptr_handler<alpha::concurrent::internal::slot_link_info> hph_sli2( p_sli2 );
		auto                                                                               hp_sli2 = hph_sli2.get_to_verify_exchange();

		sut.retrieve( p_sli1 );
		sut.retrieve( p_sli2 );
	}

	// Act
	auto p1 = sut.request_reuse();
	auto p2 = sut.request_reuse();

	// Assert
	EXPECT_EQ( p1, p_sli1 );
	EXPECT_EQ( p2, p_sli2 );
}
