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

TEST( SlotHeaderOfArray, CanCall_Constructor )
{
	// Arrange

	// Act
	alpha::concurrent::internal::slot_header_of_array sha( static_cast<uintptr_t>( 1 ) );

	// Assert
	EXPECT_NE( 0, sha.mh_.offset_to_mgr_.load() );
#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
	EXPECT_TRUE( sha.mh_.check_marker() );
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

TEST_P( SlotFunc_FixtureParam, slot_container_calc_slot_container_size )
{
	// Arrange
	auto cur_param = GetParam();

	// Act
	size_t ret_size = alpha::concurrent::internal::slot_container::calc_slot_container_size( cur_param.n_v_, cur_param.align_v_ );

	// Assert
	EXPECT_GE( ret_size, sizeof( alpha::concurrent::internal::slot_container ) + cur_param.n_v_ + cur_param.align_v_ );
}

TEST_P( SlotFunc_FixtureParam, slot_container_calc_slot_container_size2 )
{
	// Arrange
	auto                                              cur_param = GetParam();
	size_t                                            ret_size  = alpha::concurrent::internal::slot_container::calc_slot_container_size( cur_param.n_v_, cur_param.align_v_ );
	unsigned char*                                    p_tmp     = new unsigned char[ret_size];
	std::unique_ptr<unsigned char[]>                  up_tmp( p_tmp );
	alpha::concurrent::internal::slot_header_of_array sha( static_cast<uintptr_t>( 1 ) );
	EXPECT_NE( 0, sha.mh_.offset_to_mgr_.load() );

	// Act
	void* p_ret_mem = sha.allocate( reinterpret_cast<alpha::concurrent::internal::slot_container*>( p_tmp ), ret_size, cur_param.n_v_, cur_param.align_v_ );

	// Assert
	EXPECT_NE( p_ret_mem, nullptr );
	EXPECT_EQ( 0, reinterpret_cast<uintptr_t>( p_ret_mem ) % cur_param.align_v_ );
	EXPECT_GE( reinterpret_cast<uintptr_t>( p_ret_mem ), reinterpret_cast<uintptr_t>( p_tmp ) + sizeof( alpha::concurrent::internal::slot_container ) );
	EXPECT_GT( reinterpret_cast<uintptr_t>( p_tmp ) + ret_size, reinterpret_cast<uintptr_t>( p_ret_mem ) + cur_param.n_v_ );
}

TEST_P( SlotFunc_FixtureParam, slot_container_calc_slot_container_size3 )
{
	// Arrange
	auto                                              cur_param = GetParam();
	size_t                                            ret_size  = alpha::concurrent::internal::slot_container::calc_slot_container_size( cur_param.n_v_, cur_param.align_v_ );
	unsigned char*                                    p_tmp     = new unsigned char[ret_size];
	std::unique_ptr<unsigned char[]>                  up_tmp( p_tmp );
	alpha::concurrent::internal::slot_header_of_array sha( static_cast<uintptr_t>( 1 ) );
	void*                                             p_ret_mem = sha.allocate( reinterpret_cast<alpha::concurrent::internal::slot_container*>( p_tmp ), ret_size, cur_param.n_v_, cur_param.align_v_ );
	ASSERT_NE( p_ret_mem, nullptr );

	// Act
	alpha::concurrent::internal::unified_slot_header* p_ush = alpha::concurrent::internal::slot_container::get_slot_header_from_assignment_p( p_ret_mem );

	// Assert
	ASSERT_NE( p_ush, nullptr );
	EXPECT_EQ( &( p_ush->arrayh_ ), &sha );
}

#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_OVERRUN_WRITING
TEST_P( SlotFunc_FixtureParam, slot_header_of_array_CanDetect_Overrun_Writing )
{
	// Arrange
	// Arrange
	auto                                              cur_param = GetParam();
	size_t                                            ret_size  = alpha::concurrent::internal::slot_container::calc_slot_container_size( cur_param.n_v_, cur_param.align_v_ );
	unsigned char*                                    p_tmp     = new unsigned char[ret_size];
	std::unique_ptr<unsigned char[]>                  up_tmp( p_tmp );
	alpha::concurrent::internal::slot_header_of_array sha( static_cast<uintptr_t>( 1 ) );
	void*                                             p_ret_mem = sha.allocate( reinterpret_cast<alpha::concurrent::internal::slot_container*>( p_tmp ), ret_size, cur_param.n_v_, cur_param.align_v_ );
	ASSERT_NE( p_ret_mem, nullptr );
	unsigned char* p_tail_padding_area = reinterpret_cast<unsigned char*>( p_ret_mem ) + cur_param.n_v_;
	*p_tail_padding_area               = 2;

	// Act
	try {
		sha.deallocate();
		FAIL();
	} catch ( std::exception& e ) {
		// Assert
		std::string exception_log = e.what();

		auto ret = exception_log.find( "overrun" );
		EXPECT_NE( ret, std::string::npos );
	}

	// Assert
}
#endif

#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
TEST_P( SlotFunc_FixtureParam, slot_header_of_array_CanDetect_Double_Free )
{
	// Arrange
	// Arrange
	auto                                              cur_param = GetParam();
	size_t                                            ret_size  = alpha::concurrent::internal::slot_container::calc_slot_container_size( cur_param.n_v_, cur_param.align_v_ );
	unsigned char*                                    p_tmp     = new unsigned char[ret_size];
	std::unique_ptr<unsigned char[]>                  up_tmp( p_tmp );
	alpha::concurrent::internal::slot_header_of_array sha( static_cast<uintptr_t>( 1 ) );
	void*                                             p_ret_mem = sha.allocate( reinterpret_cast<alpha::concurrent::internal::slot_container*>( p_tmp ), ret_size, cur_param.n_v_, cur_param.align_v_ );
	ASSERT_NE( p_ret_mem, nullptr );
	EXPECT_NO_THROW( sha.deallocate() );

	// Act
	try {
		sha.deallocate();
		FAIL();
	} catch ( std::exception& e ) {
		// Assert
		std::string exception_log = e.what();

		auto ret = exception_log.find( "double" );
		EXPECT_NE( ret, std::string::npos );
	}
}
#endif

TEST_P( SlotFunc_FixtureParam, calc_total_slot_size_of_slot_header_of_slot_header_of_alloc )
{
	// Arrange
	auto cur_param = GetParam();

	// Act
	size_t ret_size = alpha::concurrent::internal::slot_header_of_alloc::calc_slot_header_and_container_size( cur_param.n_v_, cur_param.align_v_ );

	// Assert
	EXPECT_GE( ret_size, sizeof( alpha::concurrent::internal::slot_header_of_alloc ) + sizeof( alpha::concurrent::internal::slot_container ) + cur_param.n_v_ + cur_param.align_v_ );
}

TEST_P( SlotFunc_FixtureParam, calc_addr_info_of_slot_of_slot_header_of_alloc )
{
	// Arrange
	auto                             cur_param = GetParam();
	size_t                           ret_size  = alpha::concurrent::internal::slot_header_of_alloc::calc_slot_header_and_container_size( cur_param.n_v_, cur_param.align_v_ );
	unsigned char*                   p_tmp     = new unsigned char[ret_size];
	std::unique_ptr<unsigned char[]> up_tmp( p_tmp );

	// Act
	alpha::concurrent::internal::slot_header_of_alloc* p_sut = new ( p_tmp ) alpha::concurrent::internal::slot_header_of_alloc( ret_size );

	// Assert
	ASSERT_NE( p_sut, nullptr );
	EXPECT_EQ( 0, p_sut->mh_.offset_to_mgr_.load() );
}

TEST_P( SlotFunc_FixtureParam, Call_slot_header_of_alloc_allocate )
{
	// Arrange
	auto                                               cur_param = GetParam();
	size_t                                             ret_size  = alpha::concurrent::internal::slot_header_of_alloc::calc_slot_header_and_container_size( cur_param.n_v_, cur_param.align_v_ );
	unsigned char*                                     p_tmp     = new unsigned char[ret_size];
	std::unique_ptr<unsigned char[]>                   up_tmp( p_tmp );
	alpha::concurrent::internal::slot_header_of_alloc* p_sut = new ( p_tmp ) alpha::concurrent::internal::slot_header_of_alloc( ret_size );
	EXPECT_NE( p_sut, nullptr );

	// Act
	void* p_ret_mem = p_sut->allocate( cur_param.n_v_, cur_param.align_v_ );

	// Assert
	EXPECT_NE( p_ret_mem, nullptr );
	EXPECT_EQ( 0, reinterpret_cast<uintptr_t>( p_ret_mem ) % cur_param.align_v_ );
	EXPECT_GE( reinterpret_cast<uintptr_t>( p_ret_mem ), reinterpret_cast<uintptr_t>( p_tmp ) + sizeof( alpha::concurrent::internal::slot_header_of_alloc ) + sizeof( alpha::concurrent::internal::slot_container ) );
	EXPECT_GT( reinterpret_cast<uintptr_t>( p_tmp ) + ret_size, reinterpret_cast<uintptr_t>( p_ret_mem ) + cur_param.n_v_ );
}

TEST_P( SlotFunc_FixtureParam, Call_slot_header_of_alloc_allocate2 )
{
	// Arrange
	auto                                               cur_param = GetParam();
	size_t                                             ret_size  = alpha::concurrent::internal::slot_header_of_alloc::calc_slot_header_and_container_size( cur_param.n_v_, cur_param.align_v_ );
	unsigned char*                                     p_tmp     = new unsigned char[ret_size];
	std::unique_ptr<unsigned char[]>                   up_tmp( p_tmp );
	alpha::concurrent::internal::slot_header_of_alloc* p_sut = new ( p_tmp ) alpha::concurrent::internal::slot_header_of_alloc( ret_size );
	EXPECT_NE( p_sut, nullptr );
	void* p_ret = p_sut->allocate( cur_param.n_v_, cur_param.align_v_ );
	EXPECT_NE( p_ret, nullptr );

	// Act
	alpha::concurrent::internal::unified_slot_header* p_ush = alpha::concurrent::internal::slot_container::get_slot_header_from_assignment_p( p_ret );

	// Assert
	ASSERT_NE( p_ush, nullptr );
	EXPECT_EQ( &( p_ush->alloch_ ), p_sut );
}

#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_OVERRUN_WRITING
TEST_P( SlotFunc_FixtureParam, slot_header_of_alloc_CanDetect_Overrun_Writing )
{
	// Arrange
	auto                                               cur_param = GetParam();
	size_t                                             ret_size  = alpha::concurrent::internal::slot_header_of_alloc::calc_slot_header_and_container_size( cur_param.n_v_, cur_param.align_v_ );
	unsigned char*                                     p_tmp     = new unsigned char[ret_size];
	std::unique_ptr<unsigned char[]>                   up_tmp( p_tmp );
	alpha::concurrent::internal::slot_header_of_alloc* p_sut = new ( p_tmp ) alpha::concurrent::internal::slot_header_of_alloc( ret_size );
	ASSERT_NE( p_sut, nullptr );
	void* p_ret = p_sut->allocate( cur_param.n_v_, cur_param.align_v_ );
	ASSERT_NE( p_ret, nullptr );
	unsigned char* p_tail_padding_area = reinterpret_cast<unsigned char*>( p_ret ) + cur_param.n_v_;
	*p_tail_padding_area               = 2;

	// Act
	try {
		p_sut->deallocate();
		FAIL();
	} catch ( std::exception& e ) {
		// Assert
		std::string exception_log = e.what();

		auto ret = exception_log.find( "overrun" );
		EXPECT_NE( ret, std::string::npos );
	}
}
#endif

#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
TEST_P( SlotFunc_FixtureParam, slot_header_of_alloc_CanDetect_Double_Free )
{
	// Arrange
	auto                                               cur_param = GetParam();
	size_t                                             ret_size  = alpha::concurrent::internal::slot_header_of_alloc::calc_slot_header_and_container_size( cur_param.n_v_, cur_param.align_v_ );
	unsigned char*                                     p_tmp     = new unsigned char[ret_size];
	std::unique_ptr<unsigned char[]>                   up_tmp( p_tmp );
	alpha::concurrent::internal::slot_header_of_alloc* p_sut = new ( p_tmp ) alpha::concurrent::internal::slot_header_of_alloc( ret_size );
	ASSERT_NE( p_sut, nullptr );
	void* p_ret = p_sut->allocate( cur_param.n_v_, cur_param.align_v_ );
	ASSERT_NE( p_ret, nullptr );
	EXPECT_NO_THROW( p_sut->deallocate() );

	// Act
	try {
		p_sut->deallocate();
		FAIL();
	} catch ( std::exception& e ) {
		// Assert
		std::string exception_log = e.what();

		auto ret = exception_log.find( "double" );
		EXPECT_NE( ret, std::string::npos );
	}
}
#endif

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
