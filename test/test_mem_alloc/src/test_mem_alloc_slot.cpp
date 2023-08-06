/**
 * @file test_mem_alloc_slot.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-08-05
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "gtest/gtest.h"

#include "../src/lf_mem_alloc_slot.hpp"

//////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST( slot_mheader, do_construct )
{
	// Arrange
	// Act
	alpha::concurrent::internal::slot_mheader sut( static_cast<std::uintptr_t>( 0 ) );

	// Assert
	EXPECT_EQ( 0, sut.offset_to_mgr_.load() );
#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
	EXPECT_TRUE( sut.check_marker() );
#endif
}

TEST( slot_mheader, do_construct_offset )
{
	// Arrange
	int a = 0;

	// Act
	alpha::concurrent::internal::slot_mheader sut( reinterpret_cast<void*>( &a ) );

	// Assert
	EXPECT_EQ( &a, sut.get_mgr_pointer<int>() );
#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
	EXPECT_TRUE( sut.check_marker() );
#endif
}

TEST( unified_slot_header, same_address )
{
	// Arrange
	// Act
	alpha::concurrent::internal::unified_slot_header sut;

	// Assert
	EXPECT_EQ( &( sut.mh_ ), &( sut.alloch_.mh_ ) );
	EXPECT_EQ( &( sut.mh_ ), &( sut.arrayh_.mh_ ) );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
struct size_n_and_align {
	size_t n_v_;       // allocating request size
	size_t align_v_;   // alignment request size
};
class SlotFunc_FixtureParam : public testing::TestWithParam<size_n_and_align> {
};

TEST_P( SlotFunc_FixtureParam, calc_total_slot_size_of_slot_header_of_slot_header_of_array )
{
	// Arrange
	auto cur_param = GetParam();

	// Act
	size_t ret_size = alpha::concurrent::internal::calc_total_slot_size_of_slot_header_of<
		alpha::concurrent::internal::slot_header_of_array>(
		cur_param.n_v_, cur_param.align_v_ );

	// Assert
	EXPECT_GE( ret_size, sizeof( alpha::concurrent::internal::slot_header_of_array ) + sizeof( std::uintptr_t ) + cur_param.n_v_ + 1 );
}

TEST_P( SlotFunc_FixtureParam, calc_addr_info_of_slot_of_slot_header_of_array )
{
	// Arrange
	auto   cur_param = GetParam();
	size_t ret_size  = alpha::concurrent::internal::calc_total_slot_size_of_slot_header_of<
        alpha::concurrent::internal::slot_header_of_array>(
		cur_param.n_v_, cur_param.align_v_ );
	unsigned char*                   p_tmp = new unsigned char[ret_size];
	std::unique_ptr<unsigned char[]> up_tmp( p_tmp );

	// Act
	alpha::concurrent::internal::addr_info_of_slot ret = alpha::concurrent::internal::calc_addr_info_of_slot_of<
		alpha::concurrent::internal::slot_header_of_array>(
		p_tmp, ret_size, cur_param.n_v_, cur_param.align_v_ );

	// Assert
	EXPECT_TRUE( ret.is_success_ );
	uintptr_t chk_offset = reinterpret_cast<uintptr_t>( ret.p_back_offset_ ) - reinterpret_cast<uintptr_t>( p_tmp );
	EXPECT_GE( static_cast<size_t>( chk_offset ), sizeof( alpha::concurrent::internal::slot_header_of_array ) );
	EXPECT_EQ( reinterpret_cast<uintptr_t>( p_tmp ), reinterpret_cast<uintptr_t>( ret.p_back_offset_ ) + ret.value_of_back_offset_ );
	chk_offset = reinterpret_cast<uintptr_t>( ret.p_assignment_area_ ) - reinterpret_cast<uintptr_t>( ret.p_back_offset_ );
	EXPECT_EQ( static_cast<size_t>( chk_offset ), sizeof( uintptr_t ) );
	EXPECT_EQ( reinterpret_cast<uintptr_t>( ret.p_tail_padding_ ), reinterpret_cast<uintptr_t>( p_tmp ) + ret.value_of_offset_to_tail_padding_ );
	chk_offset = reinterpret_cast<uintptr_t>( ret.p_tail_padding_ ) - reinterpret_cast<uintptr_t>( ret.p_assignment_area_ );
	EXPECT_EQ( static_cast<size_t>( chk_offset ), cur_param.n_v_ );
	EXPECT_GE( ret.tail_padding_size_, 1 );
	EXPECT_GE( cur_param.align_v_ + alpha::concurrent::internal::default_slot_alignsize, ret.tail_padding_size_ );
}

TEST_P( SlotFunc_FixtureParam, Call_slot_header_of_array_allocate )
{
	// Arrange
	auto   cur_param = GetParam();
	size_t ret_size  = alpha::concurrent::internal::calc_total_slot_size_of_slot_header_of<
        alpha::concurrent::internal::slot_header_of_array>(
		cur_param.n_v_, cur_param.align_v_ );
	unsigned char*                                     p_tmp = new unsigned char[ret_size];
	std::unique_ptr<unsigned char[]>                   up_tmp( p_tmp );
	alpha::concurrent::internal::slot_header_of_array* p_sut = new ( p_tmp ) alpha::concurrent::internal::slot_header_of_array( static_cast<std::uintptr_t>( 0 ) );

	// Act
	void* p_ret = p_sut->allocate( ret_size, cur_param.n_v_, cur_param.align_v_ );

	// Assert
	EXPECT_GT( reinterpret_cast<uintptr_t>( p_ret ), reinterpret_cast<uintptr_t>( p_sut ) );
	EXPECT_GT( reinterpret_cast<uintptr_t>( p_sut ) + ret_size, reinterpret_cast<uintptr_t>( p_ret ) );
}

TEST_P( SlotFunc_FixtureParam, calc_total_slot_size_of_slot_header_of_slot_header_of_alloc )
{
	// Arrange
	auto cur_param = GetParam();

	// Act
	size_t ret_size = alpha::concurrent::internal::calc_total_slot_size_of_slot_header_of<
		alpha::concurrent::internal::slot_header_of_alloc>(
		cur_param.n_v_, cur_param.align_v_ );

	// Assert
	EXPECT_GE( ret_size, sizeof( alpha::concurrent::internal::slot_header_of_alloc ) + sizeof( std::uintptr_t ) + cur_param.n_v_ + cur_param.align_v_ );
}

TEST_P( SlotFunc_FixtureParam, calc_addr_info_of_slot_of_slot_header_of_alloc )
{
	// Arrange
	auto   cur_param = GetParam();
	size_t ret_size  = alpha::concurrent::internal::calc_total_slot_size_of_slot_header_of<
        alpha::concurrent::internal::slot_header_of_alloc>(
		cur_param.n_v_, cur_param.align_v_ );
	unsigned char*                   p_tmp = new unsigned char[ret_size];
	std::unique_ptr<unsigned char[]> up_tmp( p_tmp );

	// Act
	alpha::concurrent::internal::addr_info_of_slot ret = alpha::concurrent::internal::calc_addr_info_of_slot_of<
		alpha::concurrent::internal::slot_header_of_alloc>(
		p_tmp, ret_size, cur_param.n_v_, cur_param.align_v_ );

	// Assert
	EXPECT_TRUE( ret.is_success_ );
	uintptr_t chk_offset = reinterpret_cast<uintptr_t>( ret.p_back_offset_ ) - reinterpret_cast<uintptr_t>( p_tmp );
	EXPECT_GE( static_cast<size_t>( chk_offset ), sizeof( alpha::concurrent::internal::slot_header_of_alloc ) );
	EXPECT_EQ( reinterpret_cast<uintptr_t>( p_tmp ), reinterpret_cast<uintptr_t>( ret.p_back_offset_ ) + ret.value_of_back_offset_ );
	chk_offset = reinterpret_cast<uintptr_t>( ret.p_assignment_area_ ) - reinterpret_cast<uintptr_t>( ret.p_back_offset_ );
	EXPECT_EQ( static_cast<size_t>( chk_offset ), sizeof( uintptr_t ) );
	EXPECT_EQ( reinterpret_cast<uintptr_t>( ret.p_tail_padding_ ), reinterpret_cast<uintptr_t>( p_tmp ) + ret.value_of_offset_to_tail_padding_ );
	chk_offset = reinterpret_cast<uintptr_t>( ret.p_tail_padding_ ) - reinterpret_cast<uintptr_t>( ret.p_assignment_area_ );
	EXPECT_EQ( static_cast<size_t>( chk_offset ), cur_param.n_v_ );
	EXPECT_GE( ret.tail_padding_size_, 1 );
	EXPECT_GE( cur_param.align_v_ + alpha::concurrent::internal::default_slot_alignsize, ret.tail_padding_size_ );
}

TEST_P( SlotFunc_FixtureParam, Call_slot_header_of_alloc_allocate )
{
	// Arrange
	auto   cur_param = GetParam();
	size_t ret_size  = alpha::concurrent::internal::calc_total_slot_size_of_slot_header_of<
        alpha::concurrent::internal::slot_header_of_alloc>(
		cur_param.n_v_, cur_param.align_v_ );
	unsigned char*                                     p_tmp = new unsigned char[ret_size];
	std::unique_ptr<unsigned char[]>                   up_tmp( p_tmp );
	alpha::concurrent::internal::slot_header_of_alloc* p_sut = new ( p_tmp ) alpha::concurrent::internal::slot_header_of_alloc( ret_size );

	// Act
	void* p_ret = p_sut->allocate( ret_size, cur_param.n_v_, cur_param.align_v_ );

	// Assert
	EXPECT_GT( reinterpret_cast<uintptr_t>( p_ret ), reinterpret_cast<uintptr_t>( p_sut ) );
	EXPECT_GT( reinterpret_cast<uintptr_t>( p_sut ) + ret_size, reinterpret_cast<uintptr_t>( p_ret ) );
}

INSTANTIATE_TEST_SUITE_P(
	alloc_align_comb,
	SlotFunc_FixtureParam,
	testing::Values(
		size_n_and_align { 1, 1 },
		size_n_and_align { 1, 2 },
		size_n_and_align { 1, 4 },
		size_n_and_align { 1, 8 },
		size_n_and_align { 1, 16 },
		size_n_and_align { 1, 32 },
		size_n_and_align { 1, 64 },

		size_n_and_align { 2, 1 },
		size_n_and_align { 2, 2 },
		size_n_and_align { 2, 4 },
		size_n_and_align { 2, 8 },
		size_n_and_align { 2, 16 },
		size_n_and_align { 2, 32 },
		size_n_and_align { 2, 64 },

		size_n_and_align { 3, 1 },
		size_n_and_align { 3, 2 },
		size_n_and_align { 3, 4 },
		size_n_and_align { 3, 8 },
		size_n_and_align { 3, 16 },
		size_n_and_align { 3, 32 },
		size_n_and_align { 3, 64 },

		size_n_and_align { 4, 1 },
		size_n_and_align { 4, 2 },
		size_n_and_align { 4, 4 },
		size_n_and_align { 4, 8 },
		size_n_and_align { 4, 16 },
		size_n_and_align { 4, 32 },
		size_n_and_align { 4, 64 },

		size_n_and_align { 5, 1 },
		size_n_and_align { 5, 2 },
		size_n_and_align { 5, 4 },
		size_n_and_align { 5, 8 },
		size_n_and_align { 5, 16 },
		size_n_and_align { 5, 32 },
		size_n_and_align { 5, 64 },

		size_n_and_align { 6, 1 },
		size_n_and_align { 6, 2 },
		size_n_and_align { 6, 4 },
		size_n_and_align { 6, 8 },
		size_n_and_align { 6, 16 },
		size_n_and_align { 6, 32 },
		size_n_and_align { 6, 64 },

		size_n_and_align { 7, 1 },
		size_n_and_align { 7, 2 },
		size_n_and_align { 7, 4 },
		size_n_and_align { 7, 8 },
		size_n_and_align { 7, 16 },
		size_n_and_align { 7, 32 },
		size_n_and_align { 7, 64 },

		size_n_and_align { 8, 1 },
		size_n_and_align { 8, 2 },
		size_n_and_align { 8, 4 },
		size_n_and_align { 8, 8 },
		size_n_and_align { 8, 16 },
		size_n_and_align { 8, 32 },
		size_n_and_align { 8, 64 },

		size_n_and_align { 9, 1 },
		size_n_and_align { 9, 2 },
		size_n_and_align { 9, 4 },
		size_n_and_align { 9, 8 },
		size_n_and_align { 9, 16 },
		size_n_and_align { 9, 32 },
		size_n_and_align { 9, 64 },

		size_n_and_align { 15, 1 },
		size_n_and_align { 15, 2 },
		size_n_and_align { 15, 4 },
		size_n_and_align { 15, 8 },
		size_n_and_align { 15, 16 },
		size_n_and_align { 15, 32 },
		size_n_and_align { 15, 64 },

		size_n_and_align { 16, 1 },
		size_n_and_align { 16, 2 },
		size_n_and_align { 16, 4 },
		size_n_and_align { 16, 8 },
		size_n_and_align { 16, 16 },
		size_n_and_align { 16, 32 },
		size_n_and_align { 16, 64 },

		size_n_and_align { 17, 1 },
		size_n_and_align { 17, 2 },
		size_n_and_align { 17, 4 },
		size_n_and_align { 17, 8 },
		size_n_and_align { 17, 16 },
		size_n_and_align { 17, 32 },
		size_n_and_align { 17, 64 },

		size_n_and_align { 128, 1 },
		size_n_and_align { 128, 2 },
		size_n_and_align { 128, 4 },
		size_n_and_align { 128, 8 },
		size_n_and_align { 128, 16 },
		size_n_and_align { 128, 32 },
		size_n_and_align { 128, 64 }
		//
		) );

//////////////////////////////////////////////////////////////////////////////////////////////////////////
