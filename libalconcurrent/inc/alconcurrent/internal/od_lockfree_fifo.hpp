/**
 * @file od_lockfree_fifo.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-11-04
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCURRENT_INC_INTERNAL_OD_LOCKFREE_FIFO_HPP_
#define ALCONCURRENT_INC_INTERNAL_OD_LOCKFREE_FIFO_HPP_

#include "alconcurrent/internal/od_node_essence.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

class od_lockfree_fifo {
public:
	using node_pointer = od_node_link_by_hazard_handler*;

	constexpr od_lockfree_fifo( node_pointer p_sentinel ) noexcept
	  : hph_head_( p_sentinel )
	  , hph_tail_( p_sentinel )
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	  , count_( 0 )
#endif
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	  , pushpop_count_( 0 )
	  , pushpop_loop_count_( 0 )
#endif
	{
	}

	/**
	 * @brief move constructor
	 *
	 * @warning
	 * This move constructor is NOT thread-safe, because this api is not consider the concurrency.
	 */
	od_lockfree_fifo( od_lockfree_fifo&& src ) noexcept;
	virtual ~od_lockfree_fifo();

	od_lockfree_fifo( const od_lockfree_fifo& )            = delete;
	od_lockfree_fifo& operator=( const od_lockfree_fifo& ) = delete;
	od_lockfree_fifo& operator=( od_lockfree_fifo&& src )  = delete;

	void push_back( node_pointer p_nd ) noexcept;

	/**
	 * @brief ノードをpopする
	 *
	 * @return od_lockfree_fifo::node_pointer popされたノード。なお、このノードが保持している情報は、すでに無効情報となっている。
	 */
	ALCC_INTERNAL_NODISCARD_ATTR node_pointer pop_front( void* p_context_local_data ) noexcept;

	/**
	 * @brief pop_front()処理時に、popの見込みがある場合に、値が入っているノードを参照用に渡す。呼び出される側は必要に応じて、値をコピーする。
	 *
	 * @param p_node_stored_value 値が入っているノードへのポインタ
	 * @param p_context_local_data pop_front()の引数と同じポインタ
	 */
	virtual void callback_to_pick_up_value( node_pointer p_node_stored_value, void* p_context_local_data );

	node_pointer release_sentinel_node( void ) noexcept;

	bool is_empty( void ) const
	{
		// return hph_head_.get().get() == hph_tail_.get().get();
		return hph_head_.load() == hph_tail_.load();
	}

	size_t profile_info_count( void ) const;

protected:
	virtual void do_for_purged_node( node_pointer p_nd ) noexcept;

private:
	using hazard_ptr_handler_t = typename od_node_link_by_hazard_handler::hazard_ptr_handler_t;
	using hazard_pointer       = typename od_node_link_by_hazard_handler::hazard_pointer;

	hazard_ptr_handler_t hph_head_;
	hazard_ptr_handler_t hph_tail_;
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	std::atomic<size_t> count_;
#endif
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	std::atomic<size_t> pushpop_count_;
	std::atomic<size_t> pushpop_loop_count_;
#endif
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
