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
#include "alconcurrent/conf_logger.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

///////////////////////////////////////////////////////////////////////////////////////

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
	node_pointer p_cur = hph_head_.load();
	hph_head_.store( nullptr );
	while ( p_cur != nullptr ) {
		node_pointer p_nxt = p_cur->next();
		delete p_cur;
		p_cur = p_nxt;
	}

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	auto   call_cnt_val = pushpop_call_count_.load();
	auto   loop_cnt_val = pushpop_loop_count_.load();
	double ratio_val    = static_cast<double>( loop_cnt_val ) / static_cast<double>( call_cnt_val );
	LogOutput( log_type::DUMP, "od_lockfree_stack push/pop: call count = %zu, loop count = %zu, ratio = %4.2f", call_cnt_val, loop_cnt_val, ratio_val );
#endif
}

void od_lockfree_stack::push_front( node_pointer p_nd ) noexcept
{
	if ( p_nd == nullptr ) return;
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	pushpop_call_count_++;
#endif

	node_pointer p_expected = hph_head_.load();
	do {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		pushpop_loop_count_++;
#endif
		p_nd->set_next( p_expected );
	} while ( !hph_head_.compare_exchange_strong( p_expected, p_nd ) );
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	count_++;
#endif
}

ALCC_INTERNAL_NODISCARD_ATTR od_lockfree_stack::node_pointer od_lockfree_stack::pop_front( void ) noexcept
{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	pushpop_call_count_++;
#endif

	hazard_pointer          hp_cur_head = hph_head_.get_to_verify_exchange();
	hazard_pointer::pointer p_new_head;
	while ( true ) {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		pushpop_loop_count_++;
#endif
		if ( !hph_head_.verify_exchange( hp_cur_head ) ) {
			continue;
		}
		if ( hp_cur_head == nullptr ) return nullptr;

		p_new_head = hp_cur_head->next();
		if ( hph_head_.compare_exchange_strong_to_verify_exchange2( hp_cur_head, p_new_head ) ) {
			// hp_cur_headの所有権を獲得
			break;
		}
	}

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

size_t od_lockfree_stack::count_size( void ) const
{
	size_t                      ans       = 0;
	const hazard_ptr_handler_t* p_hph_cur = &hph_head_;
	hazard_pointer              hp_pre_node;
	hazard_pointer              hp_cur_node = p_hph_cur->get_to_verify_exchange();
	while ( true ) {
		if ( !p_hph_cur->verify_exchange( hp_cur_node ) ) {
			continue;
		}
		if ( hp_cur_node == nullptr ) {
			// 末端に到達したので、ループを終了する。
			break;
		}

		// ここに到達した時点で、1つ分のノードの存在確認完了
		// hp_cur_nodeがハザードポインタとして登録済みの状態。
		ans++;

		// 次のノードに進める
		hp_pre_node.swap( hp_cur_node );
		p_hph_cur   = &( hp_pre_node->hazard_handler_of_next() );
		hp_cur_node = p_hph_cur->get_to_verify_exchange();
	}

	return ans;
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

///////////////////////////////////////////////////////////////////////////////////////

od_lockfree_stack_m::od_lockfree_stack_m( od_lockfree_stack_m&& src ) noexcept
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
od_lockfree_stack_m::~od_lockfree_stack_m()
{
	node_pointer p_cur = hph_head_.load().p_;
	hph_head_.store( nullptr, false );
	while ( p_cur != nullptr ) {
		node_pointer p_nxt = p_cur->next().p_;
		delete p_cur;
		p_cur = p_nxt;
	}

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	LogOutput( log_type::DUMP, "od_lockfree_stack_m push/pop: call count = %zu, loop count = %zu", pushpop_call_count_.load(), pushpop_loop_count_.load() );
#endif
}

void od_lockfree_stack_m::push_front( node_pointer p_nd ) noexcept
{
	if ( p_nd == nullptr ) return;
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	pushpop_call_count_++;
#endif

	auto hp_w_m_expected = hph_head_.get_to_verify_exchange();
	do {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		pushpop_loop_count_++;
#endif
		node_type::pointer_w_mark p_w_m_expected { false, hp_w_m_expected.hp_.get() };
		p_nd->set_next( p_w_m_expected );
	} while ( !hph_head_.compare_exchange_strong_to_verify_exchange2( hp_w_m_expected, p_nd ) );
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	count_++;
#endif
}

ALCC_INTERNAL_NODISCARD_ATTR od_lockfree_stack_m::node_pointer od_lockfree_stack_m::pop_front( void ) noexcept
{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	pushpop_call_count_++;
#endif

	hazard_pointer_w_mark hp_w_m_cur_head = hph_head_.get_to_verify_exchange();
	hazard_pointer_w_mark hp_w_m_new_head;
	while ( true ) {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		pushpop_loop_count_++;
#endif
		if ( !hph_head_.verify_exchange( hp_w_m_cur_head ) ) {
			continue;
		}
		if ( hp_w_m_cur_head.hp_ == nullptr ) return nullptr;

		hp_w_m_cur_head.hp_->hazard_handler_of_next().reuse_to_verify_exchange( hp_w_m_new_head );
		if ( hph_head_.compare_exchange_strong_to_verify_exchange2( hp_w_m_cur_head, hp_w_m_new_head.hp_.get() ) ) {
			// hp_cur_headの所有権を獲得
			break;
		}
	}

	// ここに来た時点で、hp_cur_head で保持されているノードの所有権を確保できた。
	// ※ 確保しているノードへのポインタを他スレッドでも持っているかもしれないが、
	//    メンバ変数 v_ を参照しないアルゴリズムになっているので、以降は参照してよい。
	//    hph_next_ は他スレッドで読みだされているため、書き換えてはならない。
	//    なお、hp_cur_headは、他スレッドでもハザードポインタとして登録中であるため、ハザードポインタとしての登録がなくなるまで破棄してはならない。
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	count_--;
#endif
	return hp_w_m_cur_head.hp_.get();
}

size_t od_lockfree_stack_m::count_size( void ) const
{
	size_t                      ans       = 0;
	const hazard_ptr_handler_t* p_hph_cur = &hph_head_;
	hazard_pointer_w_mark       hp_w_m_pre_node;
	hazard_pointer_w_mark       hp_w_m_cur_node = p_hph_cur->get_to_verify_exchange();
	while ( true ) {
		if ( !p_hph_cur->verify_exchange( hp_w_m_cur_node ) ) {
			continue;
		}
		if ( hp_w_m_cur_node.hp_ == nullptr ) {
			// 末端に到達したので、ループを終了する。
			break;
		}

		// ここに到達した時点で、1つ分のノードの存在確認完了
		// hp_cur_nodeがハザードポインタとして登録済みの状態。
		ans++;

		// 次のノードに進める
		hp_w_m_pre_node.swap( hp_w_m_cur_node );
		p_hph_cur       = &( hp_w_m_pre_node.hp_->hazard_handler_of_next() );
		hp_w_m_cur_node = p_hph_cur->get_to_verify_exchange();
	}

	return ans;
}

bool od_lockfree_stack_m::is_empty( void ) const
{
	return hph_head_.load().p_ == nullptr;
}

size_t od_lockfree_stack_m::profile_info_count( void ) const
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
