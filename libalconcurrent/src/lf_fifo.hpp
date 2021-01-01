/*
 * lf_fifo.hpp
 *
 * lock free fifo
 *
 *  Created on: 2020/12/31
 *      Author: alpha
 */

#ifndef SRC_LF_FIFO_HPP_
#define SRC_LF_FIFO_HPP_

#include <atomic>
#include <memory>
#include <tuple>

#include "hazard_ptr.hpp"

namespace alpha {
namespace concurrent {

namespace internal {
/*!
 * @breif	T型に対するFIFOキュー内のリスト構造に使用するノード
 *
 * Tは、trivially copyableでなければならい。
 */
template <typename T>
struct node_of_list {

	node_of_list( void )
	  : target_()
	  , next_( nullptr )
	  , next2_( nullptr )
	{
	}

	node_of_list( const T& cont_arg )
	  : target_( cont_arg )
	  , next_( nullptr )
	  , next2_( nullptr )
	{
	}

	node_of_list( T&& cont_arg )
	  : target_( std::move( cont_arg ) )
	  , next_( nullptr )
	  , next2_( nullptr )
	{
	}

	~node_of_list()
	{
		return;
	}

	T get_value( void ) const
	{
		return target_;
	}

	void set_value( const T& value_arg )
	{
		target_ = value_arg;
	}

	node_of_list* get_next( void )
	{
		return next_.load();
	}

	void set_next( node_of_list* p_new_next )
	{
		next_.store( p_new_next );
		return;
	}

	bool next_CAS( node_of_list** pp_expect_ptr, node_of_list* p_desired_ptr )
	{
		return next_.compare_exchange_weak( *pp_expect_ptr, p_desired_ptr );
	}

private:
	T                          target_;
	std::atomic<node_of_list*> next_;

public:
	std::atomic<node_of_list*> next2_;
};

/*!
 * @breif	LIFOのstackクラス
 *
 * Tは、trivially copyableでなければならい。
 *
 * @note
 * https://www.slideshare.net/kumagi/lock-free-safe?next_slideshow=1 @n
 *
 * @note
 * ABA問題を回避できていないので、使用禁止
 */
template <typename T>
class stack_nd_list {
public:
	using value_type   = T;
	using node_type    = node_of_list<T>;
	using node_pointer = node_of_list<T>*;

	stack_nd_list( void )
	  : head_( nullptr )
	{
		return;
	}

	~stack_nd_list()
	{
		node_pointer p_cur = head_.load();
		head_.store( nullptr );

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
		p_push_node->next2_.store( nullptr );

		node_pointer p_cur_first = head_.load();
		while ( true ) {
			p_push_node->next2_.store( p_cur_first );

			head_.compare_exchange_weak( p_cur_first, p_push_node );
		}

		return;
	}

	node_pointer pop( void )
	{
		node_pointer p_cur_first = head_.load();
		node_pointer p_cur_next  = nullptr;
		do {
			p_cur_next = p_cur_first->next2_.load();
		} while ( head_.compare_exchange_weak( p_cur_first, p_cur_next ) );
		// ここのABA問題を回避できない。。。

		return p_cur_first;
	}

private:
	std::atomic<node_pointer> head_;
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
	using node_type                     = node_of_list<T>;
	using node_pointer                  = node_of_list<T>*;
	using hazard_ptr_strage             = hazard_ptr<node_of_list<T>, hzrd_max_slot_>;

	fifo_nd_list( void )
	  : head_( nullptr )
	  , tail_( nullptr )
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
					// ここで、firstの取り出しと所有権確保が完了
					// ただし、ハザードポインタをチェックしていないため、まだ参照している人がいるかもしれない。
					return std::tuple<node_pointer, value_type>( p_cur_first, ans_2nd );
				}
			}
		}
		return std::tuple<node_pointer, value_type>( nullptr, value_type() );
	}

private:
	using scoped_hazard_ref = hazard_ptr_scoped_ref<node_of_list<T>, hzrd_max_slot_>;

	enum class hazard_ptr_idx : int {
		PUSH_FUNC_LAST = 0,
		PUSH_FUNC_NEXT = 1,
		POP_FUNC_FIRST = 2,
		POP_FUNC_LAST  = 3,
		POP_FUNC_NEXT  = 4
	};

	std::atomic<node_pointer> head_;
	std::atomic<node_pointer> tail_;

	hazard_ptr_strage hzrd_ptr_;
};

}   // namespace internal

/*!
 * @breif	リスト型のFIFOキュー
 *
 * Tは、trivially copyableでなければならい。
 *
 * @note
 */
template <typename T>
class fifo_list {
public:
	using value_type = T;

	fifo_list( void )
	  : node_count_( 0 )
	{
		return;
	}

	void push( const T& cont_arg )
	{
		auto [p_new_node, dummy] = free_nd_.pop();

		if ( p_new_node == nullptr ) {
			p_new_node = allocate_one_node( cont_arg );
		} else {
			p_new_node->set_value( cont_arg );
		}

		fifo_.push( p_new_node );
		return;
	}

	std::tuple<bool, value_type> pop( void )
	{
		auto [p_poped_node, ans_value] = fifo_.pop();

		if ( p_poped_node == nullptr ) return std::tuple<bool, value_type>( false, ans_value );

		//		try_minimum_one_gc();

		if ( internal::fifo_nd_list<T>::hazard_ptr_strage::check_ptr_in_hazard_list( p_poped_node ) ) {
			//			in_hzrd_.push( p_poped_node );	// デバッグのために一旦リークさせてみる。
		} else {
			free_nd_.push( p_poped_node );
		}

		return std::tuple<bool, value_type>( true, ans_value );
	}

private:
	void try_minimum_one_gc( void )
	{
		auto [p_chk_node, dummy] = in_hzrd_.pop();

		if ( p_chk_node == nullptr ) return;

		if ( internal::fifo_nd_list<T>::hazard_ptr_strage::check_ptr_in_hazard_list( p_chk_node ) ) {
			in_hzrd_.push( p_chk_node );
		} else {
			free_nd_.push( p_chk_node );
		}

		return;
	}

	typename internal::fifo_nd_list<T>::node_pointer allocate_one_node( const T& cont_arg )
	{
		node_count_++;
		printf( "node count is now, %d\n", node_count_.load() );
		return new typename internal::fifo_nd_list<T>::node_type( cont_arg );
	}

	internal::fifo_nd_list<T> fifo_;
	internal::fifo_nd_list<T> in_hzrd_;
	internal::fifo_nd_list<T> free_nd_;
	std::atomic<int>          node_count_;
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_LF_FIFO_HPP_ */
