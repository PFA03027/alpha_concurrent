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

#include "alconcurrent/internal/od_lockfree_list.hpp"

class tut_list : public alpha::concurrent::internal::od_lockfree_list {
	// do_for_purged_node()の呼び出し評価のため、このクラスでは、デストラクタでのclear()呼び出しを行わない。
public:
	using base_cls = alpha::concurrent::internal::od_lockfree_list;

	static size_t call_count_;

	bool is_head_node( const node_pointer p_chk_ptr ) const
	{
		return base_cls::is_head_node( p_chk_ptr );
	}
	bool is_head_node( const pointer_w_mark& p_chk_ptr ) const
	{
		return base_cls::is_head_node( p_chk_ptr.p_ );
	}
	bool is_head_node( const hazard_pointer& hp_chk_ptr ) const
	{
		return base_cls::is_head_node( hp_chk_ptr.get() );
	}
	bool is_head_node( const hazard_pointer_w_mark& hp_chk_ptr ) const
	{
		return base_cls::is_head_node( hp_chk_ptr.hp_.get() );
	}
	bool is_head_node( const std::pair<hazard_pointer_w_mark, hazard_pointer_w_mark>& pair_hp_chk_ptr ) const
	{
		return base_cls::is_head_node( pair_hp_chk_ptr.second.hp_.get() );
	}

	void do_for_purged_node( node_pointer p_nd ) noexcept override
	{
		delete p_nd;
		call_count_++;
	}
};

size_t tut_list::call_count_ = 0;

TEST( Test_od_lockfree_list_Construct, DoConstruct_ThenDestruct )
{
	// Arrenge

	// Act
	ASSERT_NO_THROW( { tut_list sut; } );

	// Assert
}

TEST( Test_od_lockfree_list_Construct, OneNode_DoClear_ThenCallbackCountIsOne )
{
	// Arrenge
	tut_list               sut;
	auto                   ret        = sut.find_if( []( const tut_list::node_pointer ) { return true; } );
	tut_list::node_pointer p_new_node = new tut_list::node_type;
	sut.insert_to_before_of_curr( p_new_node, ret.first, ret.second );
	tut_list::call_count_ = 0;

	// Act
	sut.clear();

	// Assert
	EXPECT_EQ( tut_list::call_count_, 1 );
}

TEST( Test_od_lockfree_list_Construct, OneNode_NotDoClearDestruct_ThenCallbackCountIsZero )
{
	// Arrenge
	tut_list*              p_sut      = new tut_list;
	auto                   ret        = p_sut->find_if( []( const tut_list::node_pointer ) { return true; } );
	tut_list::node_pointer p_new_node = new tut_list::node_type;
	p_sut->insert_to_before_of_curr( p_new_node, ret.first, ret.second );
	tut_list::call_count_ = 0;

	// Act
	delete p_sut;

	// Assert
	EXPECT_EQ( tut_list::call_count_, 0 );
}

// ==================================================================================
class Test_od_lockfree_list : public ::testing::Test {
protected:
	void SetUp() override
	{
		alpha::concurrent::GetErrorWarningLogCountAndReset( nullptr, nullptr );
	}

	void TearDown() override
	{
		sut_.clear();

		int cw, ce;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
		EXPECT_EQ( ce, 0 );
		EXPECT_EQ( cw, 0 );
	}

	tut_list sut_;
};

TEST_F( Test_od_lockfree_list, Empty_DoMoveConstruct )
{
	// Arrenge

	// Act
	ASSERT_NO_THROW( { tut_list target = std::move( sut_ ); } );

	// Assert
}

TEST_F( Test_od_lockfree_list, OneNode_DoMoveConstruct )
{
	// Arrenge
	auto                   ret1       = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	tut_list::node_pointer p_new_node = new tut_list::node_type;
	sut_.insert_to_before_of_curr( p_new_node, ret1.first, ret1.second );

	// Act
	tut_list target = std::move( sut_ );

	// Assert
	EXPECT_EQ( sut_.size(), 0 );
	ret1 = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	EXPECT_FALSE( ret1.first.mark_ );
	EXPECT_FALSE( ret1.second.mark_ );
	EXPECT_FALSE( sut_.is_head_node( ret1 ) );
	EXPECT_TRUE( sut_.is_end_node( ret1 ) );

	EXPECT_EQ( target.size(), 1 );
	ret1      = target.find_if( []( const tut_list::node_pointer ) { return true; } );
	auto ret2 = target.find_if( []( const tut_list::node_pointer ) { return false; } );
	EXPECT_FALSE( target.is_head_node( ret1 ) );
	EXPECT_TRUE( target.is_end_node( ret2 ) );
	EXPECT_EQ( ret1.second.hp_, ret2.first.hp_ );
	EXPECT_FALSE( ret1.first.mark_ );
	EXPECT_FALSE( ret1.second.mark_ );
	EXPECT_FALSE( ret2.first.mark_ );
	EXPECT_FALSE( ret2.second.mark_ );
}

TEST_F( Test_od_lockfree_list, Empty_DoMoveAssignment )
{
	// Arrenge
	tut_list target;

	// Act
	ASSERT_NO_THROW( target = std::move( sut_ ) );

	// Assert
}

TEST_F( Test_od_lockfree_list, EmptyAndOneNode_DoMoveAssignment )
{
	// Arrenge
	auto                   ret1       = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	tut_list::node_pointer p_new_node = new tut_list::node_type;
	sut_.insert_to_before_of_curr( p_new_node, ret1.first, ret1.second );
	tut_list target;

	// Act
	target = std::move( sut_ );

	// Assert
	EXPECT_EQ( sut_.size(), 0 );
	ret1 = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	EXPECT_FALSE( ret1.first.mark_ );
	EXPECT_FALSE( ret1.second.mark_ );
	EXPECT_FALSE( sut_.is_head_node( ret1 ) );
	EXPECT_TRUE( sut_.is_end_node( ret1 ) );

	EXPECT_EQ( target.size(), 1 );
	ret1      = target.find_if( []( const tut_list::node_pointer ) { return true; } );
	auto ret2 = target.find_if( []( const tut_list::node_pointer ) { return false; } );
	EXPECT_FALSE( target.is_head_node( ret1 ) );
	EXPECT_TRUE( target.is_end_node( ret2 ) );
	EXPECT_EQ( ret1.second.hp_, ret2.first.hp_ );
	EXPECT_FALSE( ret1.first.mark_ );
	EXPECT_FALSE( ret1.second.mark_ );
	EXPECT_FALSE( ret2.first.mark_ );
	EXPECT_FALSE( ret2.second.mark_ );
}

TEST_F( Test_od_lockfree_list, OneNodeAndOneNode_DoMoveAssignment )
{
	// Arrenge
	tut_list               target;
	auto                   ret1        = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	tut_list::node_pointer p_new_node1 = new tut_list::node_type;
	sut_.insert_to_before_of_curr( p_new_node1, ret1.first, ret1.second );
	EXPECT_EQ( sut_.size(), 1 );

	ret1                               = target.find_if( []( const tut_list::node_pointer ) { return true; } );
	tut_list::node_pointer p_new_node2 = new tut_list::node_type;
	target.insert_to_before_of_curr( p_new_node2, ret1.first, ret1.second );
	EXPECT_EQ( target.size(), 1 );

	// Act
	target = std::move( sut_ );

	// Assert
	EXPECT_EQ( sut_.size(), 0 );
	ret1 = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	EXPECT_FALSE( ret1.first.mark_ );
	EXPECT_FALSE( ret1.second.mark_ );
	EXPECT_TRUE( sut_.is_head_node( ret1.first ) );
	EXPECT_FALSE( sut_.is_head_node( ret1 ) );
	EXPECT_TRUE( sut_.is_end_node( ret1 ) );

	EXPECT_EQ( target.size(), 1 );
	ret1      = target.find_if( []( const tut_list::node_pointer ) { return true; } );
	auto ret2 = target.find_if( []( const tut_list::node_pointer ) { return false; } );
	EXPECT_FALSE( target.is_head_node( ret1 ) );
	EXPECT_TRUE( target.is_end_node( ret2 ) );
	EXPECT_EQ( ret1.second.hp_, ret2.first.hp_ );
	EXPECT_EQ( ret1.second.hp_, p_new_node1 );
	EXPECT_FALSE( ret1.first.mark_ );
	EXPECT_FALSE( ret1.second.mark_ );
	EXPECT_FALSE( ret2.first.mark_ );
	EXPECT_FALSE( ret2.second.mark_ );
}

TEST_F( Test_od_lockfree_list, Empty_DoClear )
{
	// Arrenge

	// Act
	sut_.clear();

	// Assert
	EXPECT_EQ( sut_.size(), 0 );
	auto ret1 = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	EXPECT_FALSE( ret1.first.mark_ );
	EXPECT_FALSE( ret1.second.mark_ );
	EXPECT_TRUE( sut_.is_head_node( ret1.first ) );
	EXPECT_FALSE( sut_.is_head_node( ret1 ) );
	EXPECT_TRUE( sut_.is_end_node( ret1 ) );
}

TEST_F( Test_od_lockfree_list, OneNode_DoClear )
{
	// Arrenge
	auto                   ret1       = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	tut_list::node_pointer p_new_node = new tut_list::node_type;
	sut_.insert_to_before_of_curr( p_new_node, ret1.first, ret1.second );

	// Act
	sut_.clear();

	// Assert
	EXPECT_EQ( sut_.size(), 0 );
	ret1 = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	EXPECT_FALSE( ret1.first.mark_ );
	EXPECT_FALSE( ret1.second.mark_ );
	EXPECT_TRUE( sut_.is_head_node( ret1.first ) );
	EXPECT_FALSE( sut_.is_head_node( ret1 ) );
	EXPECT_TRUE( sut_.is_end_node( ret1 ) );
}

TEST_F( Test_od_lockfree_list, Empty_DoFindIf_ThenReturnEnd )
{
	// Arrenge

	// Act
	auto ret = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );

	// Assert
	EXPECT_TRUE( sut_.is_head_node( ret.first ) );
	EXPECT_FALSE( sut_.is_head_node( ret ) );
	EXPECT_TRUE( sut_.is_end_node( ret ) );
	EXPECT_FALSE( ret.first.mark_ );
	EXPECT_FALSE( ret.second.mark_ );
}

TEST_F( Test_od_lockfree_list, Empty_DoInsertBefore_ThenOneNode )
{
	// Arrenge
	auto                   ret        = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	tut_list::node_pointer p_new_node = new tut_list::node_type;
	EXPECT_EQ( sut_.size(), 0 );

	// Act
	sut_.insert_to_before_of_curr( p_new_node, ret.first, ret.second );

	// Assert
	EXPECT_EQ( sut_.size(), 1 );

	ret = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	EXPECT_TRUE( sut_.is_head_node( ret.first ) );
	EXPECT_FALSE( sut_.is_head_node( ret ) );
	EXPECT_FALSE( sut_.is_end_node( ret ) );
	EXPECT_EQ( ret.second.hp_, p_new_node );
	EXPECT_FALSE( ret.first.mark_ );
	EXPECT_FALSE( ret.second.mark_ );

	ret = sut_.find_if( []( const tut_list::node_pointer ) { return false; } );
	EXPECT_FALSE( sut_.is_head_node( ret.first ) );
	EXPECT_FALSE( sut_.is_head_node( ret ) );
	EXPECT_TRUE( sut_.is_end_node( ret ) );
	EXPECT_EQ( ret.first.hp_, p_new_node );
	EXPECT_FALSE( ret.first.mark_ );
	EXPECT_FALSE( ret.second.mark_ );
}

TEST_F( Test_od_lockfree_list, Empty_DoInsertNext_ThenOneNode )
{
	// Arrenge
	auto                   ret        = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	tut_list::node_pointer p_new_node = new tut_list::node_type;
	EXPECT_EQ( sut_.size(), 0 );

	// Act
	sut_.insert_to_next_of_prev( p_new_node, ret.first, ret.second );

	// Assert
	EXPECT_EQ( sut_.size(), 1 );

	ret = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	EXPECT_TRUE( sut_.is_head_node( ret.first ) );
	EXPECT_FALSE( sut_.is_head_node( ret ) );
	EXPECT_FALSE( sut_.is_end_node( ret ) );
	EXPECT_EQ( ret.second.hp_, p_new_node );
	EXPECT_FALSE( ret.first.mark_ );
	EXPECT_FALSE( ret.second.mark_ );

	ret = sut_.find_if( []( const tut_list::node_pointer ) { return false; } );
	EXPECT_FALSE( sut_.is_head_node( ret.first ) );
	EXPECT_FALSE( sut_.is_head_node( ret ) );
	EXPECT_TRUE( sut_.is_end_node( ret ) );
	EXPECT_EQ( ret.first.hp_, p_new_node );
	EXPECT_FALSE( ret.first.mark_ );
	EXPECT_FALSE( ret.second.mark_ );
}

TEST_F( Test_od_lockfree_list, Empty_DoRemove_ThenEmpty )
{
	// Arrenge
	auto ret = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	EXPECT_EQ( sut_.size(), 0 );

	// Act
	sut_.remove( ret.first, std::move( ret.second ) );

	// Assert
	EXPECT_EQ( sut_.size(), 0 );

	auto ret1 = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	EXPECT_FALSE( ret1.first.mark_ );
	EXPECT_FALSE( ret1.second.mark_ );
	EXPECT_TRUE( sut_.is_head_node( ret1.first ) );
	EXPECT_FALSE( sut_.is_head_node( ret1 ) );
	EXPECT_TRUE( sut_.is_end_node( ret1 ) );
}

TEST_F( Test_od_lockfree_list, OneNode_DoRemoveFirstNode_ThenEmpty )
{
	// Arrenge
	auto                   ret        = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	tut_list::node_pointer p_new_node = new tut_list::node_type;
	sut_.insert_to_next_of_prev( p_new_node, ret.first, ret.second );
	EXPECT_EQ( sut_.size(), 1 );
	ret = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );

	// Act
	sut_.remove( ret.first, std::move( ret.second ) );

	// Assert
	EXPECT_EQ( sut_.size(), 0 );

	auto ret1 = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	EXPECT_FALSE( ret1.first.mark_ );
	EXPECT_FALSE( ret1.second.mark_ );
	EXPECT_TRUE( sut_.is_head_node( ret1.first ) );
	EXPECT_FALSE( sut_.is_head_node( ret1 ) );
	EXPECT_TRUE( sut_.is_end_node( ret1 ) );
}

TEST_F( Test_od_lockfree_list, EmptyAndEmpty_DoSwap )
{
	// Arrenge
	tut_list target;

	// Act
	sut_.swap( target );

	// Assert
	EXPECT_EQ( sut_.size(), 0 );
	auto ret1 = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	EXPECT_FALSE( ret1.first.mark_ );
	EXPECT_FALSE( ret1.second.mark_ );
	EXPECT_TRUE( sut_.is_head_node( ret1.first ) );
	EXPECT_FALSE( sut_.is_head_node( ret1 ) );
	EXPECT_TRUE( sut_.is_end_node( ret1 ) );

	EXPECT_EQ( target.size(), 0 );
	auto ret2 = target.find_if( []( const tut_list::node_pointer ) { return true; } );
	EXPECT_FALSE( ret2.first.mark_ );
	EXPECT_FALSE( ret2.second.mark_ );
	EXPECT_TRUE( target.is_head_node( ret2.first ) );
	EXPECT_FALSE( target.is_head_node( ret2 ) );
	EXPECT_TRUE( target.is_end_node( ret2 ) );
}

TEST_F( Test_od_lockfree_list, EmptyAndOneNode_DoSwap )
{
	// Arrenge
	tut_list               target;
	auto                   ret        = target.find_if( []( const tut_list::node_pointer ) { return true; } );
	tut_list::node_pointer p_new_node = new tut_list::node_type;
	target.insert_to_next_of_prev( p_new_node, ret.first, ret.second );
	EXPECT_EQ( target.size(), 1 );

	// Act
	sut_.swap( target );

	// Assert
	EXPECT_EQ( sut_.size(), 1 );
	auto ret1 = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	EXPECT_FALSE( ret1.first.mark_ );
	EXPECT_FALSE( ret1.second.mark_ );
	EXPECT_TRUE( sut_.is_head_node( ret1.first ) );
	EXPECT_FALSE( sut_.is_head_node( ret1 ) );
	EXPECT_FALSE( sut_.is_end_node( ret1 ) );
	EXPECT_EQ( ret1.second.hp_, p_new_node );

	EXPECT_EQ( target.size(), 0 );
	auto ret2 = target.find_if( []( const tut_list::node_pointer ) { return true; } );
	EXPECT_FALSE( ret2.first.mark_ );
	EXPECT_FALSE( ret2.second.mark_ );
	EXPECT_TRUE( target.is_head_node( ret2.first ) );
	EXPECT_FALSE( target.is_head_node( ret2 ) );
	EXPECT_TRUE( target.is_end_node( ret2 ) );
}

TEST_F( Test_od_lockfree_list, OneNodeAndEmpty_DoSwap )
{
	// Arrenge
	tut_list target;

	auto                   ret        = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	tut_list::node_pointer p_new_node = new tut_list::node_type;
	sut_.insert_to_next_of_prev( p_new_node, ret.first, ret.second );
	EXPECT_EQ( sut_.size(), 1 );

	// Act
	sut_.swap( target );

	// Assert
	EXPECT_EQ( sut_.size(), 0 );
	auto ret1 = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	EXPECT_FALSE( ret1.first.mark_ );
	EXPECT_FALSE( ret1.second.mark_ );
	EXPECT_TRUE( sut_.is_head_node( ret1.first ) );
	EXPECT_FALSE( sut_.is_head_node( ret1 ) );
	EXPECT_TRUE( sut_.is_end_node( ret1 ) );

	EXPECT_EQ( target.size(), 1 );
	auto ret2 = target.find_if( []( const tut_list::node_pointer ) { return true; } );
	EXPECT_FALSE( ret2.first.mark_ );
	EXPECT_FALSE( ret2.second.mark_ );
	EXPECT_TRUE( target.is_head_node( ret2.first ) );
	EXPECT_FALSE( target.is_head_node( ret2 ) );
	EXPECT_FALSE( target.is_end_node( ret2 ) );
	EXPECT_EQ( ret2.second.hp_, p_new_node );
}

TEST_F( Test_od_lockfree_list, OneNodeAndOneNode_DoSwap )
{
	// Arrenge
	auto                   ret         = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	tut_list::node_pointer p_new_node1 = new tut_list::node_type;
	sut_.insert_to_next_of_prev( p_new_node1, ret.first, ret.second );
	EXPECT_EQ( sut_.size(), 1 );

	tut_list target;
	ret                                = target.find_if( []( const tut_list::node_pointer ) { return true; } );
	tut_list::node_pointer p_new_node2 = new tut_list::node_type;
	target.insert_to_next_of_prev( p_new_node2, ret.first, ret.second );
	EXPECT_EQ( target.size(), 1 );

	// Act
	sut_.swap( target );

	// Assert
	EXPECT_EQ( sut_.size(), 1 );
	auto ret1 = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	EXPECT_FALSE( ret1.first.mark_ );
	EXPECT_FALSE( ret1.second.mark_ );
	EXPECT_TRUE( sut_.is_head_node( ret1.first ) );
	EXPECT_FALSE( sut_.is_head_node( ret1 ) );
	EXPECT_FALSE( sut_.is_end_node( ret1 ) );
	EXPECT_EQ( ret1.second.hp_, p_new_node2 );

	EXPECT_EQ( target.size(), 1 );
	auto ret2 = target.find_if( []( const tut_list::node_pointer ) { return true; } );
	EXPECT_FALSE( ret2.first.mark_ );
	EXPECT_FALSE( ret2.second.mark_ );
	EXPECT_TRUE( target.is_head_node( ret2.first ) );
	EXPECT_FALSE( target.is_head_node( ret2 ) );
	EXPECT_FALSE( target.is_end_node( ret2 ) );
	EXPECT_EQ( ret2.second.hp_, p_new_node1 );
}

TEST_F( Test_od_lockfree_list, Empty_DoForeach )
{
	// Arrenge
	int  count = 0;
	auto f     = [&count]( tut_list::node_pointer ) { count++; };

	// Act
	sut_.for_each( f );

	// Assert
	EXPECT_EQ( count, 0 );
}

TEST_F( Test_od_lockfree_list, OneNode_DoForeach )
{
	// Arrenge
	auto                   ret         = sut_.find_if( []( const tut_list::node_pointer ) { return true; } );
	tut_list::node_pointer p_new_node1 = new tut_list::node_type;
	sut_.insert_to_next_of_prev( p_new_node1, ret.first, ret.second );
	EXPECT_EQ( sut_.size(), 1 );

	int  count = 0;
	auto f     = [&count]( tut_list::node_pointer ) { count++; };

	// Act
	sut_.for_each( f );

	// Assert
	EXPECT_EQ( count, 1 );
}
