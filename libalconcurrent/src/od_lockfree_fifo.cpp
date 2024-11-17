/**
 * @file od_lockfree_fifo.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-11-04
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "alconcurrent/internal/od_lockfree_fifo.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

od_lockfree_fifo::od_lockfree_fifo( od_lockfree_fifo&& src ) noexcept
  : hph_head_( std::move( src.hph_head_ ) )
  , hph_tail_( std::move( src.hph_tail_ ) )
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
  , count_( src.count_.load( std::memory_order_acquire ) )
#endif
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
  , pushpop_count_( src.pushpop_count_.load( std::memory_order_acquire ) )
  , pushpop_loop_count_( src.pushpop_loop_count_.load( std::memory_order_acquire ) )
#endif
{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	src.count_.store( 0, std::memory_order_release );
#endif
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	src.pushpop_count_.store( 0, std::memory_order_release );
	src.pushpop_loop_count_.store( 0, std::memory_order_release );
#endif
}

od_lockfree_fifo::~od_lockfree_fifo()
{
	// 本来は、release_sentinel_node()で、空っぽにしてから、破棄することがこのクラスを使う上での期待値
	hazard_pointer hp_cur = hph_head_.get_to_verify_exchange();
	if ( hp_cur != nullptr ) {
		LogOutput( log_type::WARN, "there is no call of release_sentinel_node()." );

		node_pointer p_cur = hph_head_.load();
		hph_head_.store( nullptr );
		while ( p_cur != nullptr ) {
			node_pointer p_nxt = p_cur->next();
			purge_node( p_cur );
			p_cur = p_nxt;
		}
	}
	hph_head_.store( nullptr );
	hph_tail_.store( nullptr );
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	LogOutput(
		log_type::DUMP,
		"node_fifo_lockfree_base statisc: push/pop call count = %zu, loop count = %zu, ratio =%f",
		pushpop_count_.load(), pushpop_loop_count_.load(),
		static_cast<double>( pushpop_loop_count_.load() ) / static_cast<double>( pushpop_count_.load() ) );
#endif
}

void od_lockfree_fifo::push_back( node_pointer p_nd ) noexcept
{
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	pushpop_count_++;
#endif
	p_nd->set_next( nullptr );

	hazard_pointer hp_tail_node = hph_tail_.get_to_verify_exchange();   // od_node_link_by_hazard_handler::hazard_pointer
	while ( true ) {
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
		pushpop_loop_count_++;
#endif

		if ( !hph_tail_.verify_exchange( hp_tail_node ) ) {
			continue;
		}
		hazard_pointer::pointer p_tail_next = hp_tail_node->hazard_handler_of_next().load();
		if ( p_tail_next != nullptr ) {
			// tailがまだ最後のノードを指していないので、更新して、やり直す。
			hph_tail_.compare_exchange_strong_to_verify_exchange1( hp_tail_node, p_tail_next );
			continue;
		}

		if ( hp_tail_node->hazard_handler_of_next().compare_exchange_strong( p_tail_next, p_nd ) ) {
			// ここに来た時点で、push_backは成功
			// hph_tail_を可能であれば、更新する。ここで、更新できなくても、自スレッドを含むどこかのスレッドでのpop/pushの際に、うまく更新される。
			hph_tail_.compare_exchange_weak( std::move( hp_tail_node ), p_nd );
			break;
		}
	}

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	count_++;
#endif
	return;
}

od_lockfree_fifo::node_pointer od_lockfree_fifo::pop_front( void* p_context_local_data ) noexcept
{
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	pushpop_count_++;
#endif
	hazard_pointer hp_head_node = hph_head_.get_to_verify_exchange();
	hazard_pointer hp_head_next;
	while ( true ) {
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
		pushpop_loop_count_++;
#endif
		if ( !hph_head_.verify_exchange( hp_head_node ) ) {
			continue;
		}

		hazard_pointer::pointer p_head_next = hp_head_node->hazard_handler_of_next().load();
		if ( p_head_next == nullptr ) {
			// 番兵ノードしかないので、FIFOキューは空。ループを終了する。
			break;
		}

		hazard_pointer::pointer p_tail_node = hph_tail_.load();
		if ( hp_head_node == p_tail_node ) {
			// ここに来た場合、番兵ノードしかないように見えるが、Tailはまだ更新されていないので、tailを更新する。
			hph_tail_.compare_exchange_strong( p_tail_node, p_head_next );
			// CASに成功しても失敗しても、処理を続行する。
			// たとえCASに失敗しても、(他スレッドによって)tailが更新されたことに変わりはないため。
		}

		hp_head_next.store( p_head_next );
		if ( !hp_head_node->hazard_handler_of_next().verify_exchange( hp_head_next ) ) {
			continue;
		}

		// ここに到達した時点で、hp_head_nodeとhp_head_nextがハザードポインタとして登録済みの状態。

		if ( hph_head_.compare_exchange_strong_to_verify_exchange2( hp_head_node, hp_head_next.get() ) ) {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			count_--;
#endif
			callback_to_pick_up_value( hp_head_next.get(), p_context_local_data );
			// ここで、headの取り出しと所有権確保が完了
			// ただし、headノードへのポインタがハザードポインタに登録されているかどうかをチェックしていないため、まだ参照している人がいるかもしれない。
			return hp_head_node.get();
		}
	}

	return nullptr;
}

void od_lockfree_fifo::callback_to_pick_up_value( node_pointer p_node_stored_value, void* p_context_local_data )
{
}

od_lockfree_fifo::node_pointer od_lockfree_fifo::release_sentinel_node( void ) noexcept
{
	if ( !is_empty() ) {
		internal::LogOutput( log_type::ERR, "ERR: calling condition is not expected. Before calling release_sentinel_node, this instance should be empty. therefore, now leak all remaining nodes." );
	}

	node_pointer p_ans = hph_head_.load();
	if ( p_ans == nullptr ) {
		internal::LogOutput( log_type::WARN, "WARN: sentinel node has already released." );
	}

	hph_head_.store( nullptr );
	hph_tail_.store( nullptr );

	return p_ans;
}

size_t od_lockfree_fifo::profile_info_count( void ) const
{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	return count_.load( std::memory_order_acquire );
#else
	return 0;
#endif
}

void od_lockfree_fifo::purge_node( node_pointer p_nd ) noexcept
{
	// 以下のコードは一応メモリーリークを避けるための処理。
	// ただし、deleteで破棄してよいかは状況次第
	delete p_nd;
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
