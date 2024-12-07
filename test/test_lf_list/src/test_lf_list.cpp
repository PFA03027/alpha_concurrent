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

#include "alconcurrent/conf_logger.hpp"
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
	int val_array[3];
	int idx = 0;

	// Act
	sut_.for_each( [&count, &idx, &val_array]( const tut_list::value_type& v ) {
		count++;
		if ( idx < 3 ) {
			val_array[idx] = v;
			idx++;
		}
	} );

	// Assert
	EXPECT_EQ( count, 3 );
	EXPECT_EQ( sut_.get_size(), 3 );
	EXPECT_EQ( val_array[0], 3 );
	EXPECT_EQ( val_array[1], 2 );
	EXPECT_EQ( val_array[2], 1 );
}

////

TEST_F( Test_lockfree_list, Empty_DoPushFront_ThenOneElement )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );

	// Act
	sut_.push_front( 1 );

	// Assert
	EXPECT_EQ( sut_.get_size(), 1 );
}

TEST_F( Test_lockfree_list, Empty_DoPushFrontTwice_ThenTwoElement )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	sut_.push_front( 1 );

	// Act
	sut_.push_front( 2 );

	// Assert
	EXPECT_EQ( sut_.get_size(), 2 );
}

TEST_F( Test_lockfree_list, Empty_DoPushFrontThree_ThenThreeElement )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	sut_.push_front( 1 );
	sut_.push_front( 2 );

	// Act
	sut_.push_front( 3 );

	// Assert
	int count = 0;
	int val_array[3];
	int idx = 0;
	sut_.for_each( [&count, &idx, &val_array]( const tut_list::value_type& v ) {
		count++;
		if ( idx < 3 ) {
			val_array[idx] = v;
			idx++;
		}
	} );
	EXPECT_EQ( count, 3 );
	EXPECT_EQ( sut_.get_size(), 3 );
	EXPECT_EQ( val_array[0], 3 );
	EXPECT_EQ( val_array[1], 2 );
	EXPECT_EQ( val_array[2], 1 );
}

TEST_F( Test_lockfree_list, Empty_DoPopFront_ThenEmpty )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );

	// Act
	auto ret = sut_.pop_front();

	// Assert
	EXPECT_FALSE( std::get<0>( ret ) );
	EXPECT_EQ( sut_.get_size(), 0 );
}

TEST_F( Test_lockfree_list, Empty_DoPushFrontPopFront_ThenEmpty )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	sut_.push_front( 1 );

	// Act
	auto ret = sut_.pop_front();

	// Assert
	EXPECT_TRUE( std::get<0>( ret ) );
	EXPECT_EQ( std::get<1>( ret ), 1 );
	EXPECT_EQ( sut_.get_size(), 0 );
}

TEST_F( Test_lockfree_list, Empty_DoPushFrontPopFrontTwice_ThenEmpty )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	sut_.push_front( 1 );
	sut_.pop_front();

	// Act
	auto ret = sut_.pop_front();

	// Assert
	EXPECT_FALSE( std::get<0>( ret ) );
	EXPECT_EQ( sut_.get_size(), 0 );
}

TEST_F( Test_lockfree_list, Empty_DoPushFrontTwicePopFront_ThenOneElement )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	sut_.push_front( 1 );
	sut_.push_front( 2 );

	// Act
	auto ret = sut_.pop_front();

	// Assert
	EXPECT_TRUE( std::get<0>( ret ) );
	EXPECT_EQ( std::get<1>( ret ), 2 );
	EXPECT_EQ( sut_.get_size(), 1 );
}

TEST_F( Test_lockfree_list, Empty_DoPushFrontTwicePopFrontTwice_ThenEmpty )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	sut_.push_front( 1 );
	sut_.push_front( 2 );
	auto ret1 = sut_.pop_front();

	// Act
	auto ret2 = sut_.pop_front();

	// Assert
	EXPECT_TRUE( std::get<0>( ret1 ) );
	EXPECT_EQ( std::get<1>( ret1 ), 2 );
	EXPECT_TRUE( std::get<0>( ret2 ) );
	EXPECT_EQ( std::get<1>( ret2 ), 1 );
	EXPECT_EQ( sut_.get_size(), 0 );
}

////

TEST_F( Test_lockfree_list, Empty_DoPushBack_ThenOneElement )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );

	// Act
	sut_.push_back( 1 );

	// Assert
	EXPECT_EQ( sut_.get_size(), 1 );
}

TEST_F( Test_lockfree_list, Empty_DoPushBackTwice_ThenTwoElement )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	sut_.push_back( 1 );

	// Act
	sut_.push_back( 2 );

	// Assert
	EXPECT_EQ( sut_.get_size(), 2 );
}

TEST_F( Test_lockfree_list, Empty_DoPushBackThree_ThenThreeElement )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	sut_.push_back( 1 );
	sut_.push_back( 2 );

	// Act
	sut_.push_back( 3 );

	// Assert
	int count = 0;
	int val_array[3];
	int idx = 0;
	sut_.for_each( [&count, &idx, &val_array]( const tut_list::value_type& v ) {
		count++;
		if ( idx < 3 ) {
			val_array[idx] = v;
			idx++;
		}
	} );
	EXPECT_EQ( count, 3 );
	EXPECT_EQ( sut_.get_size(), 3 );
	EXPECT_EQ( val_array[0], 1 );
	EXPECT_EQ( val_array[1], 2 );
	EXPECT_EQ( val_array[2], 3 );
}

TEST_F( Test_lockfree_list, Empty_DoPopBack_ThenEmpty )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );

	// Act
	auto ret = sut_.pop_back();

	// Assert
	EXPECT_FALSE( std::get<0>( ret ) );
	EXPECT_EQ( sut_.get_size(), 0 );
}

TEST_F( Test_lockfree_list, Empty_DoPushBackPopBack_ThenEmpty )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	sut_.push_back( 1 );

	// Act
	auto ret = sut_.pop_back();

	// Assert
	EXPECT_TRUE( std::get<0>( ret ) );
	EXPECT_EQ( std::get<1>( ret ), 1 );
	EXPECT_EQ( sut_.get_size(), 0 );
}

TEST_F( Test_lockfree_list, Empty_DoPushBackPopBackTwice_ThenEmpty )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	sut_.push_back( 1 );
	sut_.pop_back();

	// Act
	auto ret = sut_.pop_back();

	// Assert
	EXPECT_FALSE( std::get<0>( ret ) );
	EXPECT_EQ( sut_.get_size(), 0 );
}

TEST_F( Test_lockfree_list, Empty_DoPushBackTwicePopBack_ThenOneElement )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	sut_.push_back( 1 );
	sut_.push_back( 2 );

	// Act
	auto ret = sut_.pop_back();

	// Assert
	EXPECT_TRUE( std::get<0>( ret ) );
	EXPECT_EQ( std::get<1>( ret ), 2 );
	EXPECT_EQ( sut_.get_size(), 1 );
}

TEST_F( Test_lockfree_list, Empty_DoPushBackTwicePopBackTwice_ThenEmpty )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	sut_.push_back( 1 );
	sut_.push_back( 2 );
	auto ret1 = sut_.pop_back();

	// Act
	auto ret2 = sut_.pop_back();

	// Assert
	EXPECT_TRUE( std::get<0>( ret1 ) );
	EXPECT_EQ( std::get<1>( ret1 ), 2 );
	EXPECT_TRUE( std::get<0>( ret2 ) );
	EXPECT_EQ( std::get<1>( ret2 ), 1 );
	EXPECT_EQ( sut_.get_size(), 0 );
}

////
TEST_F( Test_lockfree_list, Empty_DoPushFrontPopBack_ThenEmpty )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	sut_.push_front( 1 );

	// Act
	auto ret = sut_.pop_back();

	// Assert
	EXPECT_TRUE( std::get<0>( ret ) );
	EXPECT_EQ( std::get<1>( ret ), 1 );
	EXPECT_EQ( sut_.get_size(), 0 );
}

TEST_F( Test_lockfree_list, Empty_DoPushFrontTwicePopBack_ThenEmpty )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	sut_.push_front( 1 );
	sut_.push_front( 2 );

	// Act
	auto ret = sut_.pop_back();

	// Assert
	EXPECT_TRUE( std::get<0>( ret ) );
	EXPECT_EQ( std::get<1>( ret ), 1 );
	EXPECT_EQ( sut_.get_size(), 1 );
}

TEST_F( Test_lockfree_list, Empty_DoPushFrontTwicePopBackTwice_ThenEmpty )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	sut_.push_front( 1 );
	sut_.push_front( 2 );
	auto ret1 = sut_.pop_back();

	// Act
	auto ret2 = sut_.pop_back();

	// Assert
	EXPECT_TRUE( std::get<0>( ret1 ) );
	EXPECT_EQ( std::get<1>( ret1 ), 1 );
	EXPECT_TRUE( std::get<0>( ret2 ) );
	EXPECT_EQ( std::get<1>( ret2 ), 2 );
	EXPECT_EQ( sut_.get_size(), 0 );
}

TEST_F( Test_lockfree_list, Empty_DoPushBackPopFront_ThenEmpty )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	sut_.push_back( 1 );

	// Act
	auto ret = sut_.pop_front();

	// Assert
	EXPECT_TRUE( std::get<0>( ret ) );
	EXPECT_EQ( std::get<1>( ret ), 1 );
	EXPECT_EQ( sut_.get_size(), 0 );
}

TEST_F( Test_lockfree_list, Empty_DoPushBackTwicePopFront_ThenEmpty )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	sut_.push_back( 1 );
	sut_.push_back( 2 );

	// Act
	auto ret = sut_.pop_front();

	// Assert
	EXPECT_TRUE( std::get<0>( ret ) );
	EXPECT_EQ( std::get<1>( ret ), 1 );
	EXPECT_EQ( sut_.get_size(), 1 );
}

TEST_F( Test_lockfree_list, Empty_DoPushBackTwicePopFrontTwice_ThenEmpty )
{
	// Arrenge
	EXPECT_EQ( sut_.get_size(), 0 );
	sut_.push_back( 1 );
	sut_.push_back( 2 );
	auto ret1 = sut_.pop_front();

	// Act
	auto ret2 = sut_.pop_front();

	// Assert
	EXPECT_TRUE( std::get<0>( ret1 ) );
	EXPECT_EQ( std::get<1>( ret1 ), 1 );
	EXPECT_TRUE( std::get<0>( ret2 ) );
	EXPECT_EQ( std::get<1>( ret2 ), 2 );
	EXPECT_EQ( sut_.get_size(), 0 );
}

TEST_F( Test_lockfree_list, TC4_DoForEach )
{
	// Arrange
	constexpr int loop_num = 2000;
	int           expect   = 0;
	if ( ( loop_num % 2 ) == 0 ) {
		// 偶数の場合
		expect = loop_num * ( loop_num / 2 ) + loop_num / 2;
	} else {
		// 奇数の場合
		expect = loop_num * ( ( loop_num + 1 ) / 2 );
	}

	for ( int i = 0; i <= loop_num; i++ ) {
		sut_.push_front( i );
	}

	// Act
	tut_list::value_type sum = 0;
	sut_.for_each( [&sum]( tut_list::value_type& ref_value ) {
		sum += ref_value;
	} );

	EXPECT_EQ( sut_.get_size(), loop_num + 1 );
	EXPECT_EQ( expect, sum ) << "Expect: " << expect << std::endl
							 << "Sum:    " << sum << std::endl;

	return;
}

TEST_F( Test_lockfree_list, NonOwnerPointer )
{
	using test_fifo_type3 = alpha::concurrent::lockfree_list<int*>;
	test_fifo_type3* p_test_obj;

	std::cout << "Pointer test" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push_front( new int() );
	auto ret = p_test_obj->pop_front();

	ASSERT_TRUE( std::get<0>( ret ) );

	delete std::get<1>( ret );
	delete p_test_obj;

	std::cout << "End Pointer test" << std::endl;
}

TEST_F( Test_lockfree_list, UniquePointer )
{
	using test_fifo_type3 = alpha::concurrent::lockfree_list<std::unique_ptr<int>>;
	test_fifo_type3* p_test_obj;

	std::cout << "std::unique_ptr<int> test" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push_front( std::unique_ptr<int>( new int() ) );
	auto ret = p_test_obj->pop_front();

	ASSERT_TRUE( std::get<0>( ret ) );

	delete p_test_obj;

	std::cout << "End std::unique_ptr<int> test" << std::endl;
}

TEST_F( Test_lockfree_list, UniquePointer_Then_NoLeak )
{
	using test_fifo_type3 = alpha::concurrent::lockfree_list<std::unique_ptr<int>>;
	test_fifo_type3* p_test_obj;

	std::cout << "std::unique_ptr<int> test" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push_front( std::unique_ptr<int>( new int() ) );

	delete p_test_obj;

	std::cout << "End std::unique_ptr<int> test" << std::endl;
}

class array_test {
public:
	array_test( void )
	{
		x = 1;
		return;
	}

	~array_test()
	{
		printf( "called destructor of array_test\n" );
		return;
	}

private:
	int x;
};

TEST_F( Test_lockfree_list, NonOnwerArray )
{
	using test_fifo_type3 = alpha::concurrent::lockfree_list<array_test[]>;
	test_fifo_type3* p_test_obj;

	std::cout << "Array array_test[]" << std::endl;
	p_test_obj = new test_fifo_type3( 8 );

	p_test_obj->push_front( new array_test[2] );
	auto ret = p_test_obj->pop_front();

	ASSERT_TRUE( std::get<0>( ret ) );

	delete[] std::get<1>( ret );
	delete p_test_obj;

	std::cout << "End Array array_test[] test" << std::endl;
}
