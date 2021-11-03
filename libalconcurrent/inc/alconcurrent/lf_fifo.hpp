/*!
 * @file	lf_fifo.hpp
 * @brief	semi Lock free FIFO
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
#include "one_way_list_node.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

/*!
 * @breif	リスト型のFIFOキューの基本要素となるFIFOキュークラス
 *
 * Tは、copy assignableでなければならい。
 *
 * @note
 * https://www.slideshare.net/kumagi/lock-free-safe?next_slideshow=1 @n
 * 明記はされていないが、必ずノードが１つ残る構造。
 *
 * キューに登録された値そのものは、head_.nextが指すノードに存在する。
 */
template <typename T, bool HAS_OWNERSHIP = true>
class fifo_nd_list {
public:
	static constexpr int hzrd_max_slot_ = 5;
	using value_type                    = typename std::decay<T>::type;
	using node_type                     = one_way_list_node<T, HAS_OWNERSHIP>;
	using ticket_type                   = typename one_way_list_node<T, HAS_OWNERSHIP>::ticket_type;
	using node_pointer                  = node_type*;
	using hazard_ptr_storage            = hazard_ptr<node_type, hzrd_max_slot_>;

	fifo_nd_list( void )
	  : head_( nullptr )
	  , tail_( nullptr )
	  , size_count_( 0 )
	{
		node_pointer p_initial_node = new node_type();
		head_.store( p_initial_node, std::memory_order_release );
		tail_.store( p_initial_node, std::memory_order_release );

		return;
	}

	~fifo_nd_list()
	{
		node_pointer p_cur = head_.load( std::memory_order_acquire );
		head_.store( nullptr, std::memory_order_release );
		tail_.store( nullptr, std::memory_order_release );

		if ( p_cur != nullptr ) {
			// 先頭ノードは番兵のため、nullptrであることはありえないが、チェックする。
			// ノード自体は、フリーノードストレージに戻すことが基本だが、デストラクタの場合は、戻さない仕様で割り切る。

			// 先頭ノードは番兵ノードで、既にリソースは取り出し済み。よって、デストラクタ時点でリソースの所有権喪失が確定される。
			p_cur->release_ownership();
			do {
				// リソースの所有権が保持されている２番目のノード以降の破棄を行う。ノードのデストラクタで所有中のリソースは破棄される。
				node_pointer const p_nxt = p_cur->get_next();
				delete p_cur;
				p_cur = p_nxt;
			} while ( p_cur != nullptr );
		}

		return;
	}

	/*!
	 * @breif	FIFOキューへノードをPushする。
	 *
	 * @warning
	 * 引数の管理ノードは、事前にcheck_hazard_list()を使用してハザードポインタに登録されていないことを確認していること。
	 */
	void push( node_pointer const p_push_node )
	{
		p_push_node->set_next( nullptr );

		scoped_hazard_ref scoped_ref_lst( hzrd_ptr_, (int)hazard_ptr_idx::PUSH_FUNC_LAST );
		scoped_hazard_ref scoped_ref_nxt( hzrd_ptr_, (int)hazard_ptr_idx::PUSH_FUNC_NEXT );

		while ( true ) {
			node_pointer p_cur_last = tail_.load( std::memory_order_acquire );
			scoped_ref_lst.regist_ptr_as_hazard_ptr( p_cur_last );
			if ( p_cur_last != tail_.load( std::memory_order_acquire ) ) continue;

			node_pointer p_cur_next = p_cur_last->get_next();
			scoped_ref_nxt.regist_ptr_as_hazard_ptr( p_cur_next );
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
			node_pointer p_cur_first = head_.load( std::memory_order_acquire );
			node_pointer p_cur_last  = tail_.load( std::memory_order_acquire );

			scoped_ref_first.regist_ptr_as_hazard_ptr( p_cur_first );
			if ( p_cur_first != head_.load( std::memory_order_acquire ) ) continue;

			scoped_ref_last.regist_ptr_as_hazard_ptr( p_cur_last );
			if ( p_cur_last != tail_.load( std::memory_order_acquire ) ) continue;

			node_pointer p_cur_next = p_cur_first->get_next();
			scoped_ref_next.regist_ptr_as_hazard_ptr( p_cur_next );
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
				ticket_type ans_2nd_ticket = p_cur_next->get_ticket();   // 値の取り出しに使用するチケットを得る。
				// ここで、プリエンプションして、head_がA->B->A'となった時、p_cur_nextが期待値とは異なるが、
				// ハザードポインタにA相当を確保しているので、A'は現れない。よって、このようなABA問題は起きない。
				if ( head_.compare_exchange_weak( p_cur_first, p_cur_next ) ) {
					size_count_--;
					// ここで、firstの取り出しと所有権確保が完了
					// ただし、ハザードポインタをチェックしていないため、まだ参照している人がいるかもしれない。
					value_type ans_2nd = p_cur_next->exchange_ticket_and_move_value( ans_2nd_ticket );   // firstが取り出せたので、nextから値を取り出すための権利を取得。チケットを使って、値を取り出す。
					p_cur_first->release_ownership();                                                    // 値を取り出した結果として、firstが管理する情報の所有権ロストが確定したため、それを書き込む。
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

	int get_size( void ) const
	{
		return size_count_.load( std::memory_order_acquire );
	}

private:
	fifo_nd_list( const fifo_nd_list& ) = delete;
	fifo_nd_list( fifo_nd_list&& )      = delete;
	fifo_nd_list operator=( const fifo_nd_list& ) = delete;
	fifo_nd_list operator=( fifo_nd_list&& ) = delete;

	using scoped_hazard_ref = hazard_ptr_scoped_ref<node_type, hzrd_max_slot_>;

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
 * In case that template parameter ALLOW_TO_ALLOCATE is true, @n
 * In case of no avialable free node that carries a value, new node is allocated from heap internally. @n
 * In this case, this queue may be locked. And push() may trigger this behavior. @n
 * On the other hand, used free node will be recycled without a memory allocation. In this case, push() is lock free.
 *
 * To reduce lock behavior, pre-allocated nodes are effective. @n
 * get_allocated_num() provides the number of the allocated nodes. This value is hint to configuration.
 *
 * In case that template parameter ALLOW_TO_ALLOCATE is false, @n
 * In case of no avialable free node, push() member function will return false. In this case, it fails to push a value @n
 * User side has a role to recover this condition by User side itself, e.g. backoff approach.
 *
 * If T is pointer, HAS_OWNERSHIP impacts to free of resource. @n
 * If HAS_OWNERSHIP is true, this class free a pointer resource by delete when a this class instance destructed.
 * If HAS_OWNERSHIP is false, this class does not handle a pointer resource when a this class instance destructed.
 *
 * @note
 * To resolve ABA issue, this FIFO queue uses hazard pointer approach.
 */
template <typename T, bool ALLOW_TO_ALLOCATE = true, bool HAS_OWNERSHIP = true>
class fifo_list {
public:
	using value_type = typename std::decay<T>::type;

	/*!
	 * @breif	Constructor
	 *
	 * In case that ALLOW_TO_ALLOCATE is false, argument pre_alloc_nodes must be equal or than 1.
	 * This value should be at least the number of CPUs.
	 * Also, it is recommended to double the number of threads to access.
	 */
	fifo_list(
		unsigned int pre_alloc_nodes = 1   //!< [in]	number of pre-allocated internal free node
	)
	{
		if ( !ALLOW_TO_ALLOCATE && ( pre_alloc_nodes < 1 ) ) {
			LogOutput( log_type::WARN, "Warning: in case that ALLOW_TO_ALLOCATE is false, argument pre_alloc_nodes must be equal or greater than 1, now %d. Therefore it will be modified to 1. This value should be at least the number of CPUs. Also, it is recommended to double the number of threads to access.", pre_alloc_nodes );
			pre_alloc_nodes = 1;
		}

		free_nd_.init_and_pre_allocate<fifo_node_type>( pre_alloc_nodes );
		return;
	}

	/*!
	 * @breif	Push a value to this FIFO queue
	 *
	 * cont_arg will copy to FIFO queue.
	 *
	 * @note
	 * In case that template parameter ALLOW_TO_ALLOCATE is true, this I/F is valid.
	 */
	template <bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push(
		const value_type& cont_arg   //!< [in]	a value to push this FIFO queue
		) -> typename std::enable_if<BOOL_VALUE, void>::type
	{
		std::atomic_thread_fence( std::memory_order_release );

		fifo_node_pointer p_new_node = free_nd_.allocate<fifo_node_type>(
			true,
			[this]( fifo_node_pointer p_chk_node ) {
				return !( this->fifo_.check_hazard_list( p_chk_node ) );
				//				return false;
			} );

		p_new_node->set_value( cont_arg );

		fifo_.push( p_new_node );
		return;
	}

	/*!
	 * @breif	Push a value to this FIFO queue
	 *
	 * cont_arg will copy to FIFO queue.
	 *
	 * @return	success or fail to push
	 * @retval	true	success to push copy value of cont_arg to FIFO
	 * @retval	false	fail to push cont_arg value to FIFO
	 *
	 * @note
	 * @li	In case that template parameter ALLOW_TO_ALLOCATE is false, this I/F is valid.
	 * @li	In case that return value is false, User side has a role to recover this condition by User side itself, e.g. backoff approach.
	 */
	template <bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push(
		const value_type& cont_arg   //!< [in]	a value to push this FIFO queue
		) -> typename std::enable_if<!BOOL_VALUE, bool>::type
	{
		std::atomic_thread_fence( std::memory_order_release );

		fifo_node_pointer p_new_node = free_nd_.allocate<fifo_node_type>(
			false,
			[this]( fifo_node_pointer p_chk_node ) {
				return !( this->fifo_.check_hazard_list( p_chk_node ) );
				//				return false;
			} );

		if ( p_new_node != nullptr ) {
			p_new_node->set_value( cont_arg );

			fifo_.push( p_new_node );
			return true;
		} else {
			return false;
		}
	}

	/*!
	 * @breif	Pop a value from this FIFO queue
	 *
	 * @return	1st element: true=success to pop a value, false=no value to pop
	 * @return	2nd element: a value that is pop. In case that 1st element is true, 2nd element is valid.
	 */
	std::tuple<bool, value_type> pop( void )
	{
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
		auto [p_poped_node, ans_value] = fifo_.pop();
#else
		auto local_ret    = fifo_.pop();
		auto p_poped_node = std::get<0>( local_ret );
		auto ans_value    = std::get<1>( local_ret );
#endif

		std::atomic_thread_fence( std::memory_order_acquire );

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
	int get_size( void ) const
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
	fifo_list& operator=( const fifo_list& ) = delete;
	fifo_list& operator=( fifo_list&& ) = delete;

	using free_nd_storage_type = internal::free_nd_storage;
	using free_node_type       = typename free_nd_storage_type::node_type;
	using free_node_pointer    = typename free_nd_storage_type::node_pointer;
	using fifo_type            = internal::fifo_nd_list<T, HAS_OWNERSHIP>;
	using fifo_node_type       = typename fifo_type::node_type;
	using fifo_node_pointer    = typename fifo_type::node_pointer;

	fifo_type            fifo_;
	free_nd_storage_type free_nd_;
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_LF_FIFO_HPP_ */
