/**
 * @file test_mem_alloc_slot_array.cpp
 * @author Teruaki Ata <PFA03027@nifty.com>
 * @brief test for slot array implementation
 * @version 0.1
 * @date 2023-08-06
 *
 * @copyright Copyright (c) 2023 Teruaki Ata <PFA03027@nifty.com>
 *
 */

#include "gtest/gtest.h"

#include "../src/lf_mem_alloc_slot_array.hpp"

TEST( slot_array_mgr, construct_destruct )
{
	// Arrange

	// Act
	alpha::concurrent::internal::slot_array_mgr*                 p_sut = new ( static_cast<size_t>( 32 ), static_cast<size_t>( 32 ) ) alpha::concurrent::internal::slot_array_mgr( nullptr, static_cast<size_t>( 32 ), static_cast<size_t>( 32 ) );
	std::unique_ptr<alpha::concurrent::internal::slot_array_mgr> up_sut( p_sut );

	// Assert
	for ( int i = 0; i < 32; i++ ) {
		auto p_ret = p_sut->get_pointer_of_slot( i )->mh_.get_mgr_pointer<alpha::concurrent::internal::slot_array_mgr>();
		EXPECT_EQ( p_ret, p_sut );
	}
}

TEST( slot_array_mgr, Call_get_slot_idx_from_assignment_p )
{
	// Arrange
	alpha::concurrent::internal::slot_array_mgr*                 p_sut = new ( static_cast<size_t>( 32 ), static_cast<size_t>( 32 ) ) alpha::concurrent::internal::slot_array_mgr( nullptr, static_cast<size_t>( 32 ), static_cast<size_t>( 32 ) );
	std::unique_ptr<alpha::concurrent::internal::slot_array_mgr> up_sut( p_sut );
	void*                                                        p_mem   = p_sut->allocate( 1, 32, 1 );
	size_t                                                       ret_idx = 0;

	// Act
	ASSERT_NO_THROW( ret_idx = alpha::concurrent::internal::slot_array_mgr::get_slot_idx_from_assignment_p( p_mem ) );

	// Assert
	EXPECT_EQ( ret_idx, 1 );
}
