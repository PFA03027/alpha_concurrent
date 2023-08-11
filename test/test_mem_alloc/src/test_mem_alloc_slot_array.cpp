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
	alpha::concurrent::internal::slot_array_mgr*                 p_sut = alpha::concurrent::internal::slot_array_mgr::make_instance( nullptr, static_cast<size_t>( 32 ), static_cast<size_t>( 32 ) );
	std::unique_ptr<alpha::concurrent::internal::slot_array_mgr> up_sut( p_sut );

	// Assert
	for ( int i = 0; i < 32; i++ ) {
		auto p_ret = p_sut->get_pointer_of_slot( i )->mh_.get_mgr_pointer<alpha::concurrent::internal::slot_array_mgr>();
		EXPECT_EQ( p_ret, p_sut );
	}
}

TEST( slot_array_mgr, Call_get_XXX_from_assignment_p )
{
	// Arrange
	alpha::concurrent::internal::slot_array_mgr*                 p_sut = alpha::concurrent::internal::slot_array_mgr::make_instance( nullptr, static_cast<size_t>( 32 ), static_cast<size_t>( 32 ) );
	std::unique_ptr<alpha::concurrent::internal::slot_array_mgr> up_sut( p_sut );
	void*                                                        p_mem = p_sut->allocate( 1, 32, 1 );
	ASSERT_NE( p_mem, nullptr );
	size_t ret_idx = 0;

	// Act
	ASSERT_NO_THROW( {
		auto p_slot = alpha::concurrent::internal::slot_array_mgr::get_pointer_of_slot_header_of_array_from_assignment_p( p_mem );
		ret_idx     = alpha::concurrent::internal::slot_array_mgr::get_slot_idx_from_slot_header_of_array( p_slot );
	} );

	// Assert
	EXPECT_EQ( ret_idx, 1 );
}

TEST( slot_array_mgr, Call_Allocate_Deallocate )
{
	// Arrange
	alpha::concurrent::internal::slot_array_mgr*                 p_sut = alpha::concurrent::internal::slot_array_mgr::make_instance( nullptr, static_cast<size_t>( 32 ), static_cast<size_t>( 32 ) );
	std::unique_ptr<alpha::concurrent::internal::slot_array_mgr> up_sut( p_sut );
	void*                                                        p_mem = p_sut->allocate( 32 );
	ASSERT_NE( p_mem, nullptr );

	// Act
	ASSERT_NO_THROW( p_sut->deallocate( p_mem ) );

	// Assert
}

TEST( slot_array_mgr, Call_OverAllocate )
{
	// Arrange
	alpha::concurrent::internal::slot_array_mgr*                 p_sut = alpha::concurrent::internal::slot_array_mgr::make_instance( nullptr, static_cast<size_t>( 1 ), static_cast<size_t>( 32 ) );
	std::unique_ptr<alpha::concurrent::internal::slot_array_mgr> up_sut( p_sut );
	void*                                                        p_mem = p_sut->allocate( 32 );   // 1つ目のスロットが割り当てられる。
	ASSERT_NE( p_mem, nullptr );

	// Act
	p_mem = p_sut->allocate( 32 );   // 2つ目のスロットを割り当て用とするが、存在しないため、割り当てに失敗するのをテストする。

	// Assert
	ASSERT_EQ( p_mem, nullptr );
}
