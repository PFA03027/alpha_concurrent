/**
 * @file test_lf_fifo_internal.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-10-14
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "gtest/gtest.h"

#include "alconcurrent/lf_fifo.hpp"

using test_fifo_type = alpha::concurrent::internal::x_lockfree_fifo<size_t>;

class Test_x_lockfree_fifo : public ::testing::Test {
protected:
	void SetUp() override
	{
		alpha::concurrent::GetErrorWarningLogCountAndReset( nullptr, nullptr );

		p_sut_ = new test_fifo_type;
	}

	void TearDown() override
	{
		delete p_sut_;

		int cw, ce;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
		EXPECT_EQ( ce, 0 );
		EXPECT_EQ( cw, 0 );
	}

	test_fifo_type* p_sut_;
};

TEST_F( Test_x_lockfree_fifo, PushValue_DoPopValue_Then_SameValue )
{
	// Arrange
	p_sut_->push( 1 );

	// Act
	auto ret = p_sut_->pop();

	// Assert
	EXPECT_TRUE( std::get<0>( ret ) );
	EXPECT_EQ( std::get<1>( ret ), 1 );
}

TEST_F( Test_x_lockfree_fifo, PushValueTwice_DoPopValueTwice_Then_OrderIsCorrect )
{
	// Arrange
	p_sut_->push( 1 );
	p_sut_->push( 2 );

	// Act
	auto ret1 = p_sut_->pop();
	auto ret2 = p_sut_->pop();

	// Assert
	EXPECT_TRUE( std::get<0>( ret1 ) );
	EXPECT_EQ( std::get<1>( ret1 ), 1 );
	EXPECT_TRUE( std::get<0>( ret2 ) );
	EXPECT_EQ( std::get<1>( ret2 ), 2 );
}

TEST_F( Test_x_lockfree_fifo, PushHeadValue_DoPopValue_Then_SameValue )
{
	// Arrange
	p_sut_->push_head( 1 );

	// Act
	auto ret = p_sut_->pop();

	// Assert
	EXPECT_TRUE( std::get<0>( ret ) );
	EXPECT_EQ( std::get<1>( ret ), 1 );
}

TEST_F( Test_x_lockfree_fifo, PushHeadValueTwice_DoPopValueTwice_Then_OrderIsCorrect )
{
	// Arrange
	p_sut_->push_head( 1 );
	p_sut_->push_head( 2 );

	// Act
	auto ret1 = p_sut_->pop();
	auto ret2 = p_sut_->pop();

	// Assert
	EXPECT_TRUE( std::get<0>( ret1 ) );
	EXPECT_EQ( std::get<1>( ret1 ), 2 );
	EXPECT_TRUE( std::get<0>( ret2 ) );
	EXPECT_EQ( std::get<1>( ret2 ), 1 );
}

TEST_F( Test_x_lockfree_fifo, PushValuePushHeadValue_DoPopValueTwice_Then_OrderIsCorrect )
{
	// Arrange
	p_sut_->push( 1 );
	p_sut_->push_head( 2 );

	// Act
	auto ret1 = p_sut_->pop();
	auto ret2 = p_sut_->pop();

	// Assert
	EXPECT_TRUE( std::get<0>( ret1 ) );
	EXPECT_EQ( std::get<1>( ret1 ), 2 );
	EXPECT_TRUE( std::get<0>( ret2 ) );
	EXPECT_EQ( std::get<1>( ret2 ), 1 );
}

TEST_F( Test_x_lockfree_fifo, PushHeadValuePushValue_DoPopValueTwice_Then_OrderIsCorrect )
{
	// Arrange
	p_sut_->push_head( 1 );
	p_sut_->push( 2 );

	// Act
	auto ret1 = p_sut_->pop();
	auto ret2 = p_sut_->pop();

	// Assert
	EXPECT_TRUE( std::get<0>( ret1 ) );
	EXPECT_EQ( std::get<1>( ret1 ), 1 );
	EXPECT_TRUE( std::get<0>( ret2 ) );
	EXPECT_EQ( std::get<1>( ret2 ), 2 );
}
