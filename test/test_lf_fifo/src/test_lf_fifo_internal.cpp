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
	ASSERT_TRUE( ret.has_value() );
	EXPECT_EQ( ret.value(), 1 );
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
	ASSERT_TRUE( ret1.has_value() );
	EXPECT_EQ( ret1.value(), 1 );
	ASSERT_TRUE( ret2.has_value() );
	EXPECT_EQ( ret2.value(), 2 );
}

TEST_F( Test_x_lockfree_fifo, PushHeadValue_DoPopValue_Then_SameValue )
{
	// Arrange
	p_sut_->push_head( 1 );

	// Act
	auto ret = p_sut_->pop();

	// Assert
	ASSERT_TRUE( ret.has_value() );
	EXPECT_EQ( ret.value(), 1 );
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
	ASSERT_TRUE( ret1.has_value() );
	EXPECT_EQ( ret1.value(), 2 );
	ASSERT_TRUE( ret2.has_value() );
	EXPECT_EQ( ret2.value(), 1 );
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
	ASSERT_TRUE( ret1.has_value() );
	EXPECT_EQ( ret1.value(), 2 );
	ASSERT_TRUE( ret2.has_value() );
	EXPECT_EQ( ret2.value(), 1 );
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
	ASSERT_TRUE( ret1.has_value() );
	EXPECT_EQ( ret1.value(), 1 );
	ASSERT_TRUE( ret2.has_value() );
	EXPECT_EQ( ret2.value(), 2 );
}

TEST_F( Test_x_lockfree_fifo, DoEmplace_Head )
{
	// Arrange
	p_sut_->push( 10 );
	p_sut_->emplace_head( 20 );

	// Act
	auto ret1 = p_sut_->pop();
	auto ret2 = p_sut_->pop();

	// Assert
	ASSERT_TRUE( ret1.has_value() );
	EXPECT_EQ( ret1.value(), 20 );
	ASSERT_TRUE( ret2.has_value() );
	EXPECT_EQ( ret2.value(), 10 );
}

TEST_F( Test_x_lockfree_fifo, DoEmplace )
{
	// Arrange
	p_sut_->emplace( 10 );

	// Act
	auto ret1 = p_sut_->pop();

	// Assert
	ASSERT_TRUE( ret1.has_value() );
	EXPECT_EQ( ret1.value(), 10 );
}
