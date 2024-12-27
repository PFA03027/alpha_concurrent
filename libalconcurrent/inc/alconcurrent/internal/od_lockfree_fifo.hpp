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

	od_lockfree_fifo( node_pointer p_sentinel ) noexcept;

	/**
	 * @brief move constructor
	 *
	 * @warning
	 * This move constructor is NOT thread-safe, because this api is not consider the concurrency.
	 *
	 * @warning
	 * src become invalid object. if you would like to reuse src, please call introduce_sentinel_node() with new sentinel node.
	 */
	od_lockfree_fifo( od_lockfree_fifo&& src ) noexcept;
	virtual ~od_lockfree_fifo();

	od_lockfree_fifo( const od_lockfree_fifo& )            = delete;
	od_lockfree_fifo& operator=( const od_lockfree_fifo& ) = delete;
	od_lockfree_fifo& operator=( od_lockfree_fifo&& src )  = delete;

	/**
	 * @brief ノードをFIFOの最後にpushする
	 *
	 * @param p_nd pushするノードへのポインタ
	 */
	void push_back( node_pointer p_nd ) noexcept;

	/**
	 * @brief ノードをFIFOの先頭からpopする
	 *
	 * @param p_context_local_data 仮想関数callback_to_pick_up_value()の第2引数として渡すポインタ
	 * @return popされたノード。なお、このノードが保持している情報に対する参照権を持たない。そのため、中の情報を読み出してはならない。
	 */
	ALCC_INTERNAL_NODISCARD_ATTR node_pointer pop_front( void* p_context_local_data ) noexcept;

	/**
	 * @brief ノードをFIFOの先頭にpushする
	 *
	 * @param p_node_new_sentinel 新たな番兵ノードへのポインタ
	 * @param p_node_w_value FIFOに挿入する情報を保持するノードへのポインタ
	 * @return 不要となったsentinelノードへのポインタ。
	 */
	ALCC_INTERNAL_NODISCARD_ATTR node_pointer push_front( node_pointer p_node_new_sentinel, node_pointer p_node_w_value ) noexcept;

	/**
	 * @brief pop_front()処理時に、popの見込みがある場合に、値が入っているノードを参照用に渡す。呼び出される側は必要に応じて、値をコピーする。
	 *
	 * @param p_node_stored_value 値が入っているノードへのポインタ
	 * @param p_context_local_data pop_front()の引数と同じポインタ
	 */
	virtual void callback_to_pick_up_value( node_pointer p_node_stored_value, void* p_context_local_data );

	ALCC_INTERNAL_NODISCARD_ATTR node_pointer release_sentinel_node( void ) noexcept;

	/**
	 * @brief invalid状態のインスタンスに番兵ノードを追加し、利用可能状態に戻す。
	 *
	 * @param p_sentinel
	 * @return node_pointer if node_pointer is nullptr, success. if node_pointer is not nullptr, fail and return value is same to p_sentinel
	 */
	ALCC_INTERNAL_NODISCARD_ATTR node_pointer introduce_sentinel_node( node_pointer p_sentinel ) noexcept;

	bool is_empty( void ) const;

	size_t count_size( void ) const;

	size_t profile_info_count( void ) const;

protected:
	/**
	 * @brief ノードを破棄する際に呼び出されるコールバック
	 *
	 * 引数p_ndで示されるノードの所有権は、この関数に渡されるため、リークが起きないように処理しなければならない。
	 *
	 * このクラスの実装はdelete式でノードを開放する実装となっている。派生クラスで必要な処理を実装すること。
	 *
	 * @param p_nd 破棄処理のために引き渡すノードへのポインタ
	 */
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
