/**
 * @file od_lockfree_stack.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-10-20
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "alconcurrent/internal/od_lockfree_stack.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

od_lockfree_stack::od_lockfree_stack( od_lockfree_stack&& src ) noexcept
  : hph_head_( std::move( src.hph_head_ ) )
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
  , count_( src.count_.load( std::memory_order_acquire ) )
  , pushpop_call_count_( src.pushpop_call_count_.load( std::memory_order_acquire ) )
  , pushpop_loop_count_( src.pushpop_loop_count_.load( std::memory_order_acquire ) )
#endif
{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	src.count_.store( 0, std::memory_order_release );
	src.pushpop_call_count_.store( 0, std::memory_order_release );
	src.pushpop_loop_count_.store( 0, std::memory_order_release );
#endif
}
od_lockfree_stack::~od_lockfree_stack()
{
	// 以下のコードは一応メモリーリークを避けるための処理。
	// ただし、deleteで破棄してよいかは状況次第
	node_pointer p_cur = hph_head_.load();
	hph_head_.store( nullptr );
	while ( p_cur != nullptr ) {
		node_pointer p_nxt = p_cur->next();
		delete p_cur;
		p_cur = p_nxt;
	}

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	LogOutput( log_type::DUMP, "od_lockfree_stack push/pop: call count = %zu, loop count = %zu", pushpop_call_count_.load(), pushpop_loop_count_.load() );
#endif
}

void od_lockfree_stack::push_front( node_pointer p_nd ) noexcept
{
	if ( p_nd == nullptr ) return;
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_PUSH_FRONT_FUNCTION_NULLPTR
	if ( p_nd->next() != nullptr ) {
		LogOutput( log_type::WARN, "od_lockfree_stack::push_front() receives a od_node<T> that has non nullptr in hph_next_" );
	}
#endif
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	pushpop_call_count_++;
#endif

	node_pointer p_expected = hph_head_.load();
	do {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		pushpop_loop_count_++;
#endif
		p_nd->set_next( p_expected );
	} while ( !hph_head_.compare_exchange_strong( p_expected, p_nd, std::memory_order_release, std::memory_order_relaxed ) );
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	count_++;
#endif
}

od_lockfree_stack::node_pointer od_lockfree_stack::pop_front( void ) noexcept
{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	pushpop_call_count_++;
#endif

	hazard_pointer hp_cur_head = hph_head_.get();
	node_pointer   p_expected;
	do {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		pushpop_loop_count_++;
#endif
		p_expected = hp_cur_head.get();
		if ( p_expected == nullptr ) return nullptr;
	} while ( !hph_head_.compare_exchange_strong( hp_cur_head, p_expected->next(), std::memory_order_release, std::memory_order_relaxed ) );

	// ここに来た時点で、hp_cur_head で保持されているノードの所有権を確保できた。
	// ※ 確保しているノードへのポインタを他スレッドでも持っているかもしれないが、
	//    メンバ変数 v_ を参照しないアルゴリズムになっているので、以降は参照してよい。
	//    hph_next_ は他スレッドで読みだされているため、書き換えてはならない。
	//    なお、hp_cur_headは、他スレッドでもハザードポインタとして登録中であるため、ハザードポインタとしての登録がなくなるまで破棄してはならない。
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	count_--;
#endif
	return hp_cur_head.get();
}

bool od_lockfree_stack::is_empty( void ) const
{
	return hph_head_.load() == nullptr;
}

size_t od_lockfree_stack::profile_info_count( void ) const
{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	return count_.load( std::memory_order_acquire );
#else
	return 0;
#endif
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
