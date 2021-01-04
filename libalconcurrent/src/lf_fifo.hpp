/*!
 * @file	lf_fifo.hpp
 * @brief
 * @author	Teruaki Ata
 * @date	Created on 2020/12/31
 * @details
 *
 * Copyright (C) 2020 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef SRC_LF_FIFO_HPP_
#define SRC_LF_FIFO_HPP_

#include <atomic>
#include <memory>
#include <tuple>

#include "free_node_storage.hpp"
#include "hazard_ptr.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

template <typename T>
struct fifo_node : public node_of_list {
	using value_type = T;

	fifo_node( void )
	  : target_()
	  , next_( nullptr )
	{
	}

	fifo_node( const T& cont_arg )
	  : target_( cont_arg )
	  , next_( nullptr )
	{
	}

	virtual ~fifo_node()
	{
		return;
	}

	T get_value( void ) const
	{
		std::atomic_thread_fence( std::memory_order_acquire );
		return target_;
	}

	void set_value( const T& value_arg )
	{
		target_ = value_arg;
		std::atomic_thread_fence( std::memory_order_release );
	}

	fifo_node* get_next( void )
	{
		std::atomic_thread_fence( std::memory_order_acquire );
		return next_.load();
	}

	void set_next( fifo_node* p_new_next )
	{
		std::atomic_thread_fence( std::memory_order_acquire );
		next_.store( p_new_next );
		return;
	}

	bool next_CAS( fifo_node** pp_expect_ptr, fifo_node* p_desired_ptr )
	{
		std::atomic_thread_fence( std::memory_order_acquire );
		return next_.compare_exchange_weak( *pp_expect_ptr, p_desired_ptr );
	}

private:
	value_type              target_;
	std::atomic<fifo_node*> next_;
};

/*!
 * @breif	リスト型のFIFOキューの基本要素となるFIFOキュークラス
 *
 * Tは、trivially copyableでなければならい。
 *
 * @note
 * https://www.slideshare.net/kumagi/lock-free-safe?next_slideshow=1 @n
 * 明記はされていないが、必ずノードが１つ残る構造。
 *
 * キューに登録された値そのものは、head_.nextが指すノードに存在する。
 */
template <typename T>
class fifo_nd_list {
public:
	static constexpr int hzrd_max_slot_ = 5;
	using value_type                    = T;
	using node_type                     = fifo_node<T>;
	using node_pointer                  = node_type*;
	using hazard_ptr_storage            = hazard_ptr<node_type, hzrd_max_slot_, true>;

	fifo_nd_list( void )
	  : head_( nullptr )
	  , tail_( nullptr )
	  , size_count_( 0 )
	{
		node_pointer p_initial_node = new node_type();
		head_.store( p_initial_node );
		tail_.store( p_initial_node );

		return;
	}

	~fifo_nd_list()
	{
		node_pointer p_cur = head_.load();
		head_.store( nullptr );
		tail_.store( nullptr );

		if ( p_cur != nullptr ) {
			// 先頭ノードは番兵のため、nullptrであることはありえないが、チェックする。
			do {
				node_pointer const p_nxt = p_cur->get_next();
				delete p_cur;
				p_cur = p_nxt;
			} while ( p_cur != nullptr );
		}

		return;
	}

	void push( node_pointer const p_push_node )
	{
		p_push_node->set_next( nullptr );

		scoped_hazard_ref scoped_ref_cur( hzrd_ptr_, (int)hazard_ptr_idx::PUSH_FUNC_LAST );
		scoped_hazard_ref scoped_ref_nxt( hzrd_ptr_, (int)hazard_ptr_idx::PUSH_FUNC_NEXT );

		while ( true ) {
			node_pointer p_cur_last = tail_.load();
			hzrd_ptr_.regist_ptr_as_hazard_ptr( p_cur_last, (int)hazard_ptr_idx::PUSH_FUNC_LAST );
			if ( p_cur_last != tail_.load() ) continue;

			node_pointer p_cur_next = p_cur_last->get_next();
			hzrd_ptr_.regist_ptr_as_hazard_ptr( p_cur_next, (int)hazard_ptr_idx::PUSH_FUNC_NEXT );
			if ( p_cur_next != p_cur_last->get_next() ) continue;

			if ( p_cur_next == nullptr ) {
				if ( p_cur_last->next_CAS( &p_cur_next, p_push_node ) ) {
					tail_.compare_exchange_weak( p_cur_last, p_push_node );
					size_count_++;
					return;   // 追加完了
				}
			} else {
				// tail_を更新する。
				// ここで、プリエンプションして、tail_がA->B->A'となった時、p_cur_lastが期待値とは異なるが、
				// ハザードポインタにA相当を確保しているので、A'は現れない。よって、このようなABA問題は起きない。
				tail_.compare_exchange_weak( p_cur_last, p_cur_next );
			}
		}
		return;
	}

	/*!
	 * @breif	FIFOキューからポインタを取り出す。
	 *
	 * 返り値の管理ノードに保管されている。
	 *
	 * @return	1st: 取り出された管理ノードへのポインタ。nullptrの場合、FIFOキューが空だったことを示す。管理対象ポインタは、
	 * @return	2nd: 取り出された管理ノードへのポインタが有効の場合、FIFOから取り出された値
	 *
	 * @warning
	 * 管理ノードは、使用中の可能性がある。ハザードポインタに登録されていないことの確認が完了するまで、管理ノードの破棄や、APIを呼び出しで、値を変更してはならない。
	 */
	std::tuple<node_pointer, value_type> pop( void )
	{
		scoped_hazard_ref scoped_ref_first( hzrd_ptr_, (int)hazard_ptr_idx::POP_FUNC_FIRST );
		scoped_hazard_ref scoped_ref_last( hzrd_ptr_, (int)hazard_ptr_idx::POP_FUNC_LAST );
		scoped_hazard_ref scoped_ref_next( hzrd_ptr_, (int)hazard_ptr_idx::POP_FUNC_NEXT );

		while ( true ) {
			node_pointer p_cur_first = head_.load();
			node_pointer p_cur_last  = tail_.load();

			hzrd_ptr_.regist_ptr_as_hazard_ptr( p_cur_first, (int)hazard_ptr_idx::POP_FUNC_FIRST );
			if ( p_cur_first != head_.load() ) continue;

			hzrd_ptr_.regist_ptr_as_hazard_ptr( p_cur_last, (int)hazard_ptr_idx::POP_FUNC_LAST );
			if ( p_cur_last != tail_.load() ) continue;

			node_pointer p_cur_next = p_cur_first->get_next();
			hzrd_ptr_.regist_ptr_as_hazard_ptr( p_cur_next, (int)hazard_ptr_idx::POP_FUNC_NEXT );
			if ( p_cur_next != p_cur_first->get_next() ) continue;

			if ( p_cur_first == p_cur_last ) {
				if ( p_cur_next == nullptr ) {
					// 番兵ノードしかないので、FIFOキューは空。
					return std::tuple<node_pointer, value_type>( nullptr, value_type() );
				} else {
					// 番兵ノードしかないように見えるが、Tailがまだ更新されていないだけ。
					// tailを更新して、pop処理をし直す。
					// ここで、プリエンプションして、head_がA->B->A'となった時、p_cur_nextが期待値とは異なるが、
					// ハザードポインタにA相当を確保しているので、A'は現れない。よって、このようなABA問題は起きない。
					tail_.compare_exchange_weak( p_cur_last, p_cur_next );
				}
			} else {
				if ( p_cur_next == nullptr ) {
					// headが他のスレッドでpopされた。
					continue;
				}
				T ans_2nd = p_cur_next->get_value();   // この処理が必要になるため、T型は、trivially copyableでなければならい。
				// ここで、プリエンプションして、head_がA->B->A'となった時、p_cur_nextが期待値とは異なるが、
				// ハザードポインタにA相当を確保しているので、A'は現れない。よって、このようなABA問題は起きない。
				if ( head_.compare_exchange_weak( p_cur_first, p_cur_next ) ) {
					size_count_--;
					// ここで、firstの取り出しと所有権確保が完了
					// ただし、ハザードポインタをチェックしていないため、まだ参照している人がいるかもしれない。
					return std::tuple<node_pointer, value_type>( p_cur_first, ans_2nd );
				}
			}
		}
		return std::tuple<node_pointer, value_type>( nullptr, value_type() );
	}

	/*!
	 * @breif	Check whether a pointer is in this hazard list
	 *
	 * @retval	true	p_chk_ptr is in this hazard list.
	 * @retval	false	p_chk_ptr is not in this hazard list.
	 */
	bool check_hazard_list( node_pointer const p_chk_node )
	{
		return hzrd_ptr_.check_ptr_in_hazard_list( p_chk_node );
	}

	int get_size( void )
	{
		return size_count_.load();
	}

private:
	fifo_nd_list( const fifo_nd_list& ) = delete;
	fifo_nd_list( fifo_nd_list&& )      = delete;
	fifo_nd_list operator=( const fifo_nd_list& ) = delete;
	fifo_nd_list operator=( fifo_nd_list&& ) = delete;

	using scoped_hazard_ref = hazard_ptr_scoped_ref<node_type, hzrd_max_slot_, true>;

	enum class hazard_ptr_idx : int {
		PUSH_FUNC_LAST = 0,
		PUSH_FUNC_NEXT = 1,
		POP_FUNC_FIRST = 2,
		POP_FUNC_LAST  = 3,
		POP_FUNC_NEXT  = 4
	};

	std::atomic<node_pointer> head_;
	std::atomic<node_pointer> tail_;
	std::atomic<int>          size_count_;

	hazard_ptr_storage hzrd_ptr_;
};

}   // namespace internal

/*!
 * @breif	semi-lock free FIFO type queue
 *
 * Type T should be trivially copyable.
 *
 * In case of no avialable free node that carries a value, new node is allocated from heap internally. @n
 * In this case, this queue may be locked. And push() may trigger this behavior.
 *
 * On the other hand, used free node will be recycled without a memory allocation. In this case, push() is lock free.
 *
 * To reduce lock behavior, pre-allocated nodes are effective. @n
 * get_allocated_num() provides the number of the allocated nodes. This value is hint to configuration.
 *
 * @note
 * To resolve ABA issue, this FIFO queue uses hazard pointer approach.
 */
template <typename T>
class fifo_list {
public:
	using value_type = T;

	/*!
	 * @breif	Constructor
	 */
	fifo_list(
		int pre_alloc_nodes = 0   //!< [in]	number of pre-allocated internal free node
	)
	{
		free_nd_.pre_allocate<fifo_node_type>( pre_alloc_nodes );
		return;
	}

	/*!
	 * @breif	Push a value to this FIFO queue
	 *
	 * cont_arg will copy to FIFO queue.
	 */
	void push(
		const T& cont_arg   //!< [in]	a value to push this FIFO queue
	)
	{
		fifo_node_pointer p_new_node = free_nd_.allocate<fifo_node_type>(
			[this]( fifo_node_pointer p_chk_node ) {
				return !( this->fifo_.check_hazard_list( p_chk_node ) );
				//				return false;
			} );

		p_new_node->set_value( cont_arg );

		fifo_.push( p_new_node );
		return;
	}

	/*!
	 * @breif	Pop a value from this FIFO queue
	 *
	 * @return	1st element: true=success to pop a value, false=no value to pop
	 * @return	2nd element: a value that is pop. In case that 1st element is true, 2nd element is valid.
	 */
	std::tuple<bool, value_type> pop( void )
	{
		auto [p_poped_node, ans_value] = fifo_.pop();

		if ( p_poped_node == nullptr ) return std::tuple<bool, value_type>( false, ans_value );

		free_nd_.recycle( (free_node_pointer)p_poped_node );

		return std::tuple<bool, value_type>( true, ans_value );
	}

	/*!
	 * @breif	number of the queued values in FIFO
	 *
	 * @warning
	 * This FIFO will be access by several thread concurrently. So, true number of this FIFO queue may be changed when caller uses the returned value.
	 */
	int get_size( void )
	{
		return fifo_.get_size();
	}

	/*!
	 * @breif	get the total number of the allocated internal nodes
	 *
	 * @warning
	 * This FIFO will be access by several thread concurrently. So, true number of this FIFO queue may be changed when caller uses the returned value.
	 */
	int get_allocated_num( void )
	{
		return free_nd_.get_allocated_num();
	}

private:
	fifo_list( const fifo_list& ) = delete;
	fifo_list( fifo_list&& )      = delete;
	fifo_list operator=( const fifo_list& ) = delete;
	fifo_list operator=( fifo_list&& ) = delete;

	using free_nd_storage_type = internal::free_nd_storage;
	using free_node_type       = typename free_nd_storage_type::node_type;
	using free_node_pointer    = typename free_nd_storage_type::node_pointer;
	using fifo_type            = internal::fifo_nd_list<T>;
	using fifo_node_type       = typename fifo_type::node_type;
	using fifo_node_pointer    = typename fifo_type::node_pointer;

	fifo_type            fifo_;
	free_nd_storage_type free_nd_;
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_LF_FIFO_HPP_ */
