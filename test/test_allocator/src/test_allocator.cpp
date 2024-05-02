/**
 * @file test_conf.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief Test for conf_logger.hpp
 * @version 0.1
 * @date 2022-12-30
 *
 * @copyright Copyright (c) 2022, Teruaki Ata <PFA03027@nifty.com>
 *
 */

#include "alconcurrent/internal/alloc_only_allocator.hpp"
#include "mmap_allocator.hpp"

#include "gtest/gtest.h"

constexpr size_t REQ_ALLOC_SIZE          = 1024;
constexpr size_t test_conf_pre_mmap_size = 1024 * 1024;

TEST( MMAP_Alocator, DO_max_size )
{
	if ( alpha::concurrent::internal::conf_max_mmap_alloc_size > ( 1024UL * 1024UL * 1024UL * 32UL ) ) {
		return;   // 32Gを超えるようなら、多分テスト不可能なので、テストをスキップする。
	}
	// Arrange

	// Act
	auto mmap_alloc_ret = alpha::concurrent::internal::allocate_by_mmap( alpha::concurrent::internal::conf_max_mmap_alloc_size, 0 );

	// Assert
	ASSERT_NE( nullptr, mmap_alloc_ret.p_allocated_addr_ );
	EXPECT_GE( alpha::concurrent::internal::conf_max_mmap_alloc_size, mmap_alloc_ret.allocated_size_ );

	// Cleanup
	auto ret_unmap = alpha::concurrent::internal::deallocate_by_munmap( mmap_alloc_ret.p_allocated_addr_, mmap_alloc_ret.allocated_size_ );
	EXPECT_EQ( 0, ret_unmap );
}

TEST( MMAP_Alocator, DO_max_size_plus_one )
{
	if ( alpha::concurrent::internal::conf_max_mmap_alloc_size > ( 1024UL * 1024UL * 1024UL * 32UL ) ) {
		return;   // 32Gを超えるようなら、多分テスト不可能なので、テストをスキップする。
	}
	// Arrange

	// Act
	auto mmap_alloc_ret = alpha::concurrent::internal::allocate_by_mmap( alpha::concurrent::internal::conf_max_mmap_alloc_size + 1, 0 );

	// Assert
	EXPECT_EQ( nullptr, mmap_alloc_ret.p_allocated_addr_ );
	EXPECT_EQ( 0, mmap_alloc_ret.allocated_size_ );
}

TEST( Alloc_only_class, Call_push )
{
	auto pre_status = alpha::concurrent::internal::get_alloc_mmap_status();

	{
		// Arrange
		alpha::concurrent::internal::alloc_only_chamber sut( true, 128 );
		void*                                           p_mem = sut.allocate<8>( 55 );
		EXPECT_NE( p_mem, nullptr );
	}

	// Act
	auto post_status = alpha::concurrent::internal::get_alloc_mmap_status();

	// Assert
	EXPECT_EQ( pre_status.active_size_, post_status.active_size_ );
}

TEST( Alloc_only_class, Call_dump )
{
	auto pre_status = alpha::concurrent::internal::get_alloc_mmap_status();

	{
		// Arrange
		alpha::concurrent::internal::alloc_only_chamber sut( true, 128 );
		void*                                           p_mem = sut.allocate( REQ_ALLOC_SIZE );
		EXPECT_NE( p_mem, nullptr );

		// Act
		sut.dump_to_log( alpha::concurrent::log_type::TEST, 't', 2 );
	}

	// Assert
	auto post_status = alpha::concurrent::internal::get_alloc_mmap_status();
	EXPECT_EQ( pre_status.active_size_, post_status.active_size_ );
}

TEST( Alloc_only_class, Call_allocating_only )
{
	auto pre_status = alpha::concurrent::internal::get_alloc_mmap_status();

	{
		// Arrange
		alpha::concurrent::internal::alloc_only_chamber sut( true, 128 );

		// Act
		void* p_mem = sut.allocate( REQ_ALLOC_SIZE );

		// Assert
		EXPECT_NE( p_mem, nullptr );
	}
	auto post_status = alpha::concurrent::internal::get_alloc_mmap_status();
	EXPECT_EQ( pre_status.active_size_, post_status.active_size_ );
}

TEST( Alloc_only_class, Do_append_allocation )
{
	auto pre_status = alpha::concurrent::internal::get_alloc_mmap_status();

	{
		// Arrange
		alpha::concurrent::internal::alloc_only_chamber sut( true, test_conf_pre_mmap_size );
		void*                                           p_mem = sut.allocate( test_conf_pre_mmap_size / 2 );
		EXPECT_NE( p_mem, nullptr );

		// Act
		p_mem = sut.allocate( test_conf_pre_mmap_size / 2 );

		// Assert
		EXPECT_NE( p_mem, nullptr );
	}
	auto post_status = alpha::concurrent::internal::get_alloc_mmap_status();
	EXPECT_EQ( pre_status.active_size_, post_status.active_size_ );
}

TEST( Alloc_only_class, Do_allocation_over_pre_mmap_size )
{
	auto pre_status = alpha::concurrent::internal::get_alloc_mmap_status();

	{
		// Arrange
		alpha::concurrent::internal::alloc_only_chamber sut( true, test_conf_pre_mmap_size );

		// Act
		void* p_mem = sut.allocate( test_conf_pre_mmap_size * 2 );

		// Assert
		EXPECT_NE( p_mem, nullptr );
	}
	auto post_status = alpha::concurrent::internal::get_alloc_mmap_status();
	EXPECT_EQ( pre_status.active_size_, post_status.active_size_ );
}

TEST( Alloc_only_class, CanCall_VerifyValidity1 )
{
	// Arrange

	// Act
	auto cr = alpha::concurrent::internal::alloc_only_chamber::verify_validity( nullptr );

	// Assert
	EXPECT_EQ( cr, alpha::concurrent::internal::alloc_only_chamber::validity_status::kInvalid );
}

#if 0
/* セグメンテーションフォルト無しで安定してテストすることができないため、無効化する。 */
TEST( Alloc_only_class, CanCall_VerifyValidity2 )
{
	// Arrange
	uintptr_t test_buff[1000];

	// Act
	auto cr = alpha::concurrent::internal::alloc_only_chamber::verify_validity( &( test_buff[500] ) );

	// Assert
	EXPECT_EQ( cr, alpha::concurrent::internal::alloc_only_chamber::validity_status::kInvalid );
}
#endif

TEST( Alloc_only_class, CanCall_VerifyValidity3 )
{
	// Arrange
	alpha::concurrent::internal::alloc_only_chamber sut( true, 128 );
	void*                                           p_mem = sut.allocate( REQ_ALLOC_SIZE );
	EXPECT_NE( p_mem, nullptr );

	// Act
	auto cr = alpha::concurrent::internal::alloc_only_chamber::verify_validity( p_mem );

	// Assert
	EXPECT_EQ( cr, alpha::concurrent::internal::alloc_only_chamber::validity_status::kUsed );
}

TEST( Alloc_only_class, CanCall_Deallocate )
{
	// Arrange
	alpha::concurrent::internal::alloc_only_chamber sut( true, 128 );
	void*                                           p_mem = sut.allocate( REQ_ALLOC_SIZE );
	EXPECT_NE( p_mem, nullptr );

	// Act
	alpha::concurrent::internal::alloc_only_chamber::deallocate( p_mem );

	// Assert
	auto cr = alpha::concurrent::internal::alloc_only_chamber::verify_validity( p_mem );
	EXPECT_EQ( cr, alpha::concurrent::internal::alloc_only_chamber::validity_status::kReleased );
}

TEST( Alloc_only_class, CanCall_IsBelongToThis1 )
{
	// Arrange
	alpha::concurrent::internal::alloc_only_chamber sut( true, 128 );
	void*                                           p_mem = sut.allocate( REQ_ALLOC_SIZE );
	EXPECT_NE( p_mem, nullptr );

	// Act
	auto ret = sut.is_belong_to_this( p_mem );

	// Assert
	EXPECT_TRUE( ret );
}

TEST( Alloc_only_class, CanCall_IsBelongToThis2 )
{
	// Arrange
	alpha::concurrent::internal::alloc_only_chamber sut( true, 128 );
	alpha::concurrent::internal::alloc_only_chamber other( true, 128 );
	void*                                           p_mem = other.allocate( REQ_ALLOC_SIZE );
	EXPECT_NE( p_mem, nullptr );

	// Act
	auto ret = sut.is_belong_to_this( p_mem );

	// Assert
	EXPECT_FALSE( ret );
}

TEST( Alloc_only_class, CanCall_IsBelongToThis_With_Nullptr1 )
{
	// Arrange
	alpha::concurrent::internal::alloc_only_chamber sut( true, 128 );

	// Act
	auto ret = sut.is_belong_to_this( nullptr );

	// Assert
	EXPECT_FALSE( ret );
}

TEST( Alloc_only_class, CanCall_IsBelongToThis_With_Nullptr2 )
{
	// Arrange
	alpha::concurrent::internal::alloc_only_chamber sut( true, 128 );
	void*                                           p_mem = sut.allocate( REQ_ALLOC_SIZE );
	EXPECT_NE( p_mem, nullptr );

	// Act
	auto ret = sut.is_belong_to_this( nullptr );

	// Assert
	EXPECT_FALSE( ret );
}

TEST( Alloc_only_class, CanCall_inspect_using_memory1 )
{
	// Arrange
	alpha::concurrent::internal::alloc_only_chamber sut( true, 128 );

	// Act
	auto ret = sut.inspect_using_memory();

	// Assert
	EXPECT_EQ( ret, 0 );
}

TEST( Alloc_only_class, CanCall_inspect_using_memory2 )
{
	// Arrange
	alpha::concurrent::internal::alloc_only_chamber sut( true, 128 );
	void*                                           p_mem = sut.allocate( REQ_ALLOC_SIZE );
	EXPECT_NE( p_mem, nullptr );

	// Act
	auto ret = sut.inspect_using_memory( true, alpha::concurrent::log_type::ERR );

	// Assert
	EXPECT_EQ( ret, 1 );
}
