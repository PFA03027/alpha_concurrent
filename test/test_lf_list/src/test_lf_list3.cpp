/**
 * @file test_lf_list2.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-11-23
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "gtest/gtest.h"

#include "alconcurrent/lf_list.hpp"

using tut_list = alpha::concurrent::lockfree_list<int>;

TEST( Test_lockfree_list_Construct, DoConstruct_ThenDestruct )
{
	// Arrenge

	// Act
	ASSERT_NO_THROW( { tut_list sut; } );

	// Assert
}

// ==================================================================================
class Test_lockfree_list : public ::testing::Test {
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

	tut_list sut_;
};

TEST_F( Test_lockfree_list, Empty_DoFindIf )
{
	// Arrenge
	int count = 0;

	// Act
	sut_.find_if( [&count]( const tut_list::value_type& ) -> bool { count++; return false; } );

	// Assert
	EXPECT_EQ( count, 0 );
}

TEST_F( Test_lockfree_list, Empty_DoInsertToHead_ThenOneElement )
{
	// Arrenge
	int count = 0;

	// Act
	sut_.insert(
		[&count]( const int& v ) -> bool {
			count++;
			return true;
		},
		1 );

	// Assert
	EXPECT_EQ( count, 0 );

	count     = 0;
	int value = 0;
	sut_.find_if( [&count, &value]( const tut_list::value_type& v ) -> bool { count++; value = v; return false; } );
	EXPECT_EQ( count, 1 );
	EXPECT_EQ( value, 1 );
}

TEST_F( Test_lockfree_list, Empty_DoInsertToTail_ThenOneElement )
{
	// Arrenge
	int count = 0;

	// Act
	sut_.insert(
		[&count]( const int& v ) -> bool {
			count++;
			return false;
		},
		1 );

	// Assert
	EXPECT_EQ( count, 0 );

	count     = 0;
	int value = 0;
	sut_.find_if( [&count, &value]( const tut_list::value_type& v ) -> bool { count++; value = v; return false; } );
	EXPECT_EQ( count, 1 );
	EXPECT_EQ( value, 1 );
}

TEST_F( Test_lockfree_list, Empty_DoRemoveOneIf_ThenEmpty )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );

	// Act
	auto ret = sut_.remove_one_if( []( const tut_list::value_type& v ) -> bool { return v == 0; } );

	// Assert
	EXPECT_FALSE( std::get<0>( ret ) );
	EXPECT_EQ( sut_.get_size(), 0 );
}

TEST_F( Test_lockfree_list, OneElement_DoRemoveOneIf_ThenEmpty )
{
	// Arrenge
	sut_.insert( []( const int& v ) -> bool { return true; }, 1 );
	EXPECT_EQ( sut_.get_size(), 1 );

	// Act
	auto ret = sut_.remove_one_if( []( const tut_list::value_type& v ) -> bool { return v == 1; } );

	// Assert
	EXPECT_TRUE( std::get<0>( ret ) );
	EXPECT_EQ( sut_.get_size(), 0 );
}

TEST_F( Test_lockfree_list, TwoElement_DoRemoveOneIfFromHead_ThenOneElement )
{
	// Arrenge
	sut_.insert( []( const int& v ) -> bool { return true; }, 1 );   // 先頭に挿入
	sut_.insert( []( const int& v ) -> bool { return true; }, 2 );   // 先頭に挿入
	EXPECT_EQ( sut_.get_size(), 2 );

	// Act
	auto ret = sut_.remove_one_if( []( const tut_list::value_type& v ) -> bool { return v == 2; } );

	// Assert
	EXPECT_TRUE( std::get<0>( ret ) );
	EXPECT_EQ( sut_.get_size(), 1 );
}

TEST_F( Test_lockfree_list, TwoElement_DoRemoveOneIfFromTail_ThenOneElement )
{
	// Arrenge
	sut_.insert( []( const int& v ) -> bool { return true; }, 1 );   // 先頭に挿入
	sut_.insert( []( const int& v ) -> bool { return true; }, 2 );   // 先頭に挿入
	EXPECT_EQ( sut_.get_size(), 2 );

	// Act
	auto ret = sut_.remove_one_if( []( const tut_list::value_type& v ) -> bool { return v == 1; } );

	// Assert
	EXPECT_TRUE( std::get<0>( ret ) );
	EXPECT_EQ( sut_.get_size(), 1 );
}

TEST_F( Test_lockfree_list, ThreeElement_DoRemoveOneIfFromMid_ThenTwoElement )
{
	// Arrenge
	sut_.insert( []( const int& v ) -> bool { return true; }, 1 );   // 先頭に挿入
	sut_.insert( []( const int& v ) -> bool { return true; }, 2 );   // 先頭に挿入
	sut_.insert( []( const int& v ) -> bool { return true; }, 3 );   // 先頭に挿入
	EXPECT_EQ( sut_.get_size(), 3 );

	// Act
	auto ret = sut_.remove_one_if( []( const tut_list::value_type& v ) -> bool { return v == 2; } );

	// Assert
	EXPECT_TRUE( std::get<0>( ret ) );
	EXPECT_EQ( sut_.get_size(), 2 );
}

TEST_F( Test_lockfree_list, Empty_DoRemoveAllIf_ThenEmpty )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );

	// Act
	size_t ret = sut_.remove_all_if( []( const tut_list::value_type& v ) -> bool { return true; } );

	// Assert
	EXPECT_EQ( ret, 0 );
	EXPECT_EQ( sut_.get_size(), 0 );
}

TEST_F( Test_lockfree_list, OneElement_DoRemoveAllIf_ThenEmpty )
{
	// Arrenge
	sut_.insert( []( const int& v ) -> bool { return true; }, 1 );
	EXPECT_EQ( sut_.get_size(), 1 );

	// Act
	size_t ret = sut_.remove_all_if( []( const tut_list::value_type& v ) -> bool { return true; } );

	// Assert
	EXPECT_EQ( ret, 1 );
	EXPECT_EQ( sut_.get_size(), 0 );
}

TEST_F( Test_lockfree_list, OneElement_DoRemoveAllIf_ThenOneElement )
{
	// Arrenge
	sut_.insert( []( const int& v ) -> bool { return true; }, 1 );
	EXPECT_EQ( sut_.get_size(), 1 );

	// Act
	size_t ret = sut_.remove_all_if( []( const tut_list::value_type& v ) -> bool { return false; } );

	// Assert
	EXPECT_EQ( ret, 0 );
	EXPECT_EQ( sut_.get_size(), 1 );
}

TEST_F( Test_lockfree_list, TwoElement_DoRemoveAllIfFromHead_ThenOneElement )
{
	// Arrenge
	sut_.insert( []( const int& v ) -> bool { return true; }, 1 );   // 先頭に挿入
	sut_.insert( []( const int& v ) -> bool { return true; }, 2 );   // 先頭に挿入
	EXPECT_EQ( sut_.get_size(), 2 );

	// Act
	size_t ret = sut_.remove_all_if( []( const tut_list::value_type& v ) -> bool { return v == 2; } );

	// Assert
	EXPECT_EQ( ret, 1 );
	EXPECT_EQ( sut_.get_size(), 1 );
}

TEST_F( Test_lockfree_list, TwoElement_DoRemoveAllIfFromTail_ThenOneElement )
{
	// Arrenge
	sut_.insert( []( const int& v ) -> bool { return true; }, 1 );   // 先頭に挿入
	sut_.insert( []( const int& v ) -> bool { return true; }, 2 );   // 先頭に挿入
	EXPECT_EQ( sut_.get_size(), 2 );

	// Act
	size_t ret = sut_.remove_all_if( []( const tut_list::value_type& v ) -> bool { return v == 1; } );

	// Assert
	EXPECT_EQ( ret, 1 );
	EXPECT_EQ( sut_.get_size(), 1 );
}

TEST_F( Test_lockfree_list, TwoElement_DoRemoveAllIf_ThenEmpty )
{
	// Arrenge
	sut_.insert( []( const int& v ) -> bool { return true; }, 1 );   // 先頭に挿入
	sut_.insert( []( const int& v ) -> bool { return true; }, 2 );   // 先頭に挿入
	EXPECT_EQ( sut_.get_size(), 2 );

	// Act
	size_t ret = sut_.remove_all_if( []( const tut_list::value_type& v ) -> bool { return true; } );

	// Assert
	EXPECT_EQ( ret, 2 );
	EXPECT_EQ( sut_.get_size(), 0 );
}

TEST_F( Test_lockfree_list, TwoElement_DoRemoveAllIf_ThenTwoElement )
{
	// Arrenge
	sut_.insert( []( const int& v ) -> bool { return true; }, 1 );   // 先頭に挿入
	sut_.insert( []( const int& v ) -> bool { return true; }, 2 );   // 先頭に挿入
	EXPECT_EQ( sut_.get_size(), 2 );

	// Act
	size_t ret = sut_.remove_all_if( []( const tut_list::value_type& v ) -> bool { return false; } );

	// Assert
	EXPECT_EQ( ret, 0 );
	EXPECT_EQ( sut_.get_size(), 2 );
}

TEST_F( Test_lockfree_list, ThreeElement_DoRemoveAllIfFromMid_ThenTwoElement )
{
	// Arrenge
	sut_.insert( []( const int& v ) -> bool { return true; }, 1 );   // 先頭に挿入
	sut_.insert( []( const int& v ) -> bool { return true; }, 2 );   // 先頭に挿入
	sut_.insert( []( const int& v ) -> bool { return true; }, 3 );   // 先頭に挿入
	EXPECT_EQ( sut_.get_size(), 3 );

	// Act
	size_t ret = sut_.remove_all_if( []( const tut_list::value_type& v ) -> bool { return v == 2; } );

	// Assert
	EXPECT_EQ( ret, 1 );
	EXPECT_EQ( sut_.get_size(), 2 );
}

TEST_F( Test_lockfree_list, ThreeElement_DoRemoveAllIf_ThenEmpty )
{
	// Arrenge
	sut_.insert( []( const int& v ) -> bool { return true; }, 1 );   // 先頭に挿入
	sut_.insert( []( const int& v ) -> bool { return true; }, 2 );   // 先頭に挿入
	sut_.insert( []( const int& v ) -> bool { return true; }, 3 );   // 先頭に挿入
	EXPECT_EQ( sut_.get_size(), 3 );

	// Act
	size_t ret = sut_.remove_all_if( []( const tut_list::value_type& v ) -> bool { return true; } );

	// Assert
	EXPECT_EQ( ret, 3 );
	EXPECT_EQ( sut_.get_size(), 0 );
}

TEST_F( Test_lockfree_list, ThreeElement_DoRemoveAllIf_ThenThreeElement )
{
	// Arrenge
	sut_.insert( []( const int& v ) -> bool { return true; }, 1 );   // 先頭に挿入
	sut_.insert( []( const int& v ) -> bool { return true; }, 2 );   // 先頭に挿入
	sut_.insert( []( const int& v ) -> bool { return true; }, 3 );   // 先頭に挿入
	EXPECT_EQ( sut_.get_size(), 3 );

	// Act
	size_t ret = sut_.remove_all_if( []( const tut_list::value_type& v ) -> bool { return false; } );

	// Assert
	EXPECT_EQ( ret, 0 );
	EXPECT_EQ( sut_.get_size(), 3 );
}

TEST_F( Test_lockfree_list, Empty_DoForEach )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	int count = 0;

	// Act
	sut_.for_each( [&count]( const tut_list::value_type& v ) { count++; } );

	// Assert
	EXPECT_EQ( count, 0 );
	EXPECT_EQ( sut_.get_size(), 0 );
}

TEST_F( Test_lockfree_list, OneElement_DoForEach )
{
	// Arrenge
	sut_.insert( []( const int& v ) -> bool { return true; }, 1 );   // 先頭に挿入
	EXPECT_EQ( sut_.get_size(), 1 );
	int count = 0;

	// Act
	sut_.for_each( [&count]( const tut_list::value_type& v ) { count++; } );

	// Assert
	EXPECT_EQ( count, 1 );
	EXPECT_EQ( sut_.get_size(), 1 );
}

TEST_F( Test_lockfree_list, TwoElement_DoForEach )
{
	// Arrenge
	sut_.insert( []( const int& v ) -> bool { return true; }, 1 );   // 先頭に挿入
	sut_.insert( []( const int& v ) -> bool { return true; }, 2 );   // 先頭に挿入
	EXPECT_EQ( sut_.get_size(), 2 );
	int count = 0;

	// Act
	sut_.for_each( [&count]( const tut_list::value_type& v ) { count++; } );

	// Assert
	EXPECT_EQ( count, 2 );
	EXPECT_EQ( sut_.get_size(), 2 );
}

TEST_F( Test_lockfree_list, ThreeElement_DoForEach )
{
	// Arrenge
	sut_.insert( []( const int& v ) -> bool { return true; }, 1 );   // 先頭に挿入
	sut_.insert( []( const int& v ) -> bool { return true; }, 2 );   // 先頭に挿入
	sut_.insert( []( const int& v ) -> bool { return true; }, 3 );   // 先頭に挿入
	EXPECT_EQ( sut_.get_size(), 3 );
	int count = 0;

	// Act
	sut_.for_each( [&count]( const tut_list::value_type& v ) { count++; } );

	// Assert
	EXPECT_EQ( count, 3 );
	EXPECT_EQ( sut_.get_size(), 3 );
}
