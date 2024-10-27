/*!
 * @file	lf_fifo.hpp
 * @brief	semi Lock free FIFO
 * @author	Teruaki Ata
 * @date	Created on 2020/12/31
 * @details
 *
 * Copyright (C) 2020 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef ALCONCCURRENT_INC_LF_FIFO_HPP_
#define ALCONCCURRENT_INC_LF_FIFO_HPP_

#include <array>
#include <atomic>
#include <memory>
#include <tuple>

#include "hazard_ptr.hpp"
#include "internal/free_node_storage.hpp"
#include "internal/od_node_essence.hpp"
#include "internal/od_node_pool.hpp"
#include "internal/one_way_list_node.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

/*!
 * @brief	リスト型のFIFOキューの基本要素となるFIFOキュークラス
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
	using node_type                     = one_way_list_node<T, HAS_OWNERSHIP>;
	using input_type                    = typename node_type::input_type;
	using value_type                    = typename node_type::value_type;
	using ticket_type                   = typename node_type::ticket_type;
	using node_pointer                  = node_type*;
	using hazard_ptr_storage_t          = hazard_ptr_storage<node_type, hzrd_max_slot_>;

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
	 * @brief	FIFOキューへノードをPushする。
	 *
	 * @warning
	 * 引数の管理ノードは、事前にcheck_hazard_list()を使用してハザードポインタに登録されていないことを確認していること。
	 */
	void push( node_pointer const p_push_node )
	{
		p_push_node->set_next( nullptr );

		scoped_hazard_ref scoped_ref_lst( hzrd_ptr_, (int)hazard_ptr_idx::PUSH_FUNC_LAST );
		scoped_hazard_ref scoped_ref_nxt( scoped_ref_lst, (int)hazard_ptr_idx::PUSH_FUNC_NEXT );

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
	 * @brief	FIFOキューからポインタを取り出す。
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
		scoped_hazard_ref scoped_ref_last( scoped_ref_first, (int)hazard_ptr_idx::POP_FUNC_LAST );
		scoped_hazard_ref scoped_ref_next( scoped_ref_first, (int)hazard_ptr_idx::POP_FUNC_NEXT );

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
	 * @brief	Check whether a pointer is in this hazard list
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
	fifo_nd_list( const fifo_nd_list& )           = delete;
	fifo_nd_list( fifo_nd_list&& )                = delete;
	fifo_nd_list operator=( const fifo_nd_list& ) = delete;
	fifo_nd_list operator=( fifo_nd_list&& )      = delete;

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

	hazard_ptr_storage_t hzrd_ptr_;
};

template <typename T>
class node_fifo_lockfree_base {
public:
	using node_type    = od_node_basic<T>;
	using node_pointer = od_node_basic<T>*;
	using value_type   = T;

	static_assert( std::is_default_constructible<value_type>::value && std::is_copy_constructible<value_type>::value && std::is_copy_assignable<value_type>::value,
	               "T should be default constructible, move constructible and move assignable at least" );

	constexpr node_fifo_lockfree_base( node_pointer p_sentinel ) noexcept
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
	 * This constructor is dengurous, because this api is not consider the concurrency.
	 */
	constexpr node_fifo_lockfree_base( node_fifo_lockfree_base&& src ) noexcept
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

	~node_fifo_lockfree_base()
	{
		// 以下のコードは一応メモリーリークを避けるための処理。
		// ただし、deleteで破棄してよいかは状況次第。
		// 本来は、release_sentinel_node()で、空っぽにしてから、破棄することがこのクラスを使う上での期待値
		hazard_pointer hp_cur = hph_head_.get();
		hph_head_.store( nullptr );
		hph_tail_.store( nullptr );
		while ( hp_cur != nullptr ) {
			hazard_pointer hp_nxt = hp_cur->get_hazard_ptr_of_next();
			delete hp_cur.get();
			hp_cur = std::move( hp_nxt );
		}
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
		LogOutput(
			log_type::DUMP,
			"node_fifo_lockfree_base statisc: push/pop call count = %zu, loop count = %zu, ratio =%f",
			pushpop_count_.load(), pushpop_loop_count_.load(),
			static_cast<double>( pushpop_loop_count_.load() ) / static_cast<double>( pushpop_count_.load() ) );
#endif
	}

	node_fifo_lockfree_base( const node_fifo_lockfree_base& )            = delete;
	node_fifo_lockfree_base& operator=( const node_fifo_lockfree_base& ) = delete;
	node_fifo_lockfree_base& operator=( node_fifo_lockfree_base&& src )  = delete;

	/**
	 * @brief ノードリストの先頭に、ノード一つを追加する
	 *
	 * @pre
	 * p_ndは、他スレッドからアクセスされない。
	 * 自インスタンスは、他スレッドからアクセスされない。
	 *
	 * @param p_nd
	 */
	void push_back( const value_type& v, node_pointer p_nd ) noexcept
	{
		if ( p_nd == nullptr ) return;
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_PUSH_FRONT_FUNCTION_NULLPTR
		if ( p_nd->next() != nullptr ) {
			LogOutput( log_type::WARN, "node_fifo_lockfree_base::push_back() receives a od_node<T> that has non nullptr in hph_next_" );
		}
#endif

		p_nd->set( v, nullptr );
		push_back_common( p_nd );

		return;
	}
	void push_back( value_type&& v, node_pointer p_nd ) noexcept
	{
		if ( p_nd == nullptr ) return;
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_PUSH_FRONT_FUNCTION_NULLPTR
		if ( p_nd->next() != nullptr ) {
			LogOutput( log_type::WARN, "node_fifo_lockfree_base::push_back() receives a od_node<T> that has non nullptr in hph_next_" );
		}
#endif

		p_nd->set( std::move( v ), nullptr );
		push_back_common( p_nd );

		return;
	}

	/**
	 * @brief pop from front
	 *
	 * @warning because there is a possibility that a returned node is in hazard pointer, hazard_ptr_handler_t hph_next_ in returned node is not set nullptr.
	 * And should not write not only nullptr but also any next pointer information during return pointer is in hazard pointer.
	 *
	 * @return popped node_pointer. please see warning also.
	 */
	std::tuple<node_pointer, value_type> pop_front( void ) noexcept
	{
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
		pushpop_count_++;
#endif
		hazard_pointer hp_head_node = hph_head_.get();
		hazard_pointer hp_tail_node = hph_tail_.get();
		hazard_pointer hp_head_next = hp_head_node->get_hazard_ptr_of_next();
		while ( true ) {
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
			pushpop_loop_count_++;
#endif
			if ( hp_head_node == hp_tail_node ) {
				if ( hp_head_next == nullptr ) {
					// 番兵ノードしかないので、FIFOキューは空。
					return std::tuple<node_pointer, value_type> { nullptr, value_type {} };
				}
				// 番兵ノードしかないように見えるが、Tailがまだ更新されていないだけ。
				// tailを更新して、pop処理をし直す。
				// ここで、プリエンプションして、head_がA->B->A'となった時、p_cur_nextが期待値とは異なるが、
				// ハザードポインタにA相当を確保しているので、A'は現れない。よって、このようなABA問題は起きない。
				typename hazard_pointer::pointer hph_tail_expected = hp_tail_node.get();
				hph_tail_.compare_exchange_weak( hph_tail_expected, hp_head_next.get(), std::memory_order_release, std::memory_order_acquire );
			} else {
				if ( hp_head_next == nullptr ) {
					// headが他のスレッドでpopされた。
				} else {
					// ここで、プリエンプションして、head_がA->B->A'となった時、p_cur_nextが期待値とは異なるが、
					// ハザードポインタにA相当を確保しているので、A'は現れない。よって、このようなABA問題は起きない。
					typename hazard_pointer::pointer p_head_expected_base = hp_head_node.get();                                  // od_node_link_by_hazard_handler*
					node_pointer                     p_head_expected      = static_cast<node_pointer>( p_head_expected_base );   // od_node_basic<T>*に変換する。このクラスで保持しているノードは、すべてx_fifo_node<T>のインスタンスであることをpush関数時点で保証しているので、dynamic_cast<>は不要。
					typename hazard_pointer::pointer p_head_next_base     = hp_head_next.get();                                  // od_node_link_by_hazard_handler*
					node_pointer                     p_head_next          = static_cast<node_pointer>( p_head_next_base );       // od_node_basic<T>*に変換する。このクラスで保持しているノードは、すべてx_fifo_node<T>のインスタンスであることをpush関数時点で保証しているので、dynamic_cast<>は不要。

					std::tuple<node_pointer, value_type> ans { p_head_expected, p_head_next->get() };
					if ( hph_head_.compare_exchange_weak( p_head_expected_base, p_head_next_base, std::memory_order_release, std::memory_order_acquire ) ) {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
						count_--;
#endif
						// ここで、headの取り出しと所有権確保が完了
						// ただし、headノードへのポインタがハザードポインタに登録されているかどうかをチェックしていないため、まだ参照している人がいるかもしれない。
						return ans;
					}
				}
			}
			hph_head_.reuse( hp_head_node );
			hph_tail_.reuse( hp_tail_node );
			hp_head_node->reuse_hazard_ptr_of_next( hp_head_next );
		}
	}

	/**
	 * @brief release sentinel node for destruction
	 *
	 * @pre the instance should be empty.
	 *
	 * @return node_pointer sentinel node
	 *
	 * @post no sentinel node in this instance. therefore, does not work correctly any more except destructor
	 */
	node_pointer release_sentinel_node( void ) noexcept
	{
		node_pointer p_ans = nullptr;
		if ( !is_empty() ) {
			internal::LogOutput( log_type::ERR, "ERR: calling condition is not expected. Before calling release_sentinel_node, this instance should be empty. therefore, now leak all remaining nodes." );
		}

		p_ans = static_cast<node_pointer>( hph_head_.load() );
		hph_head_.store( nullptr );
		hph_tail_.store( nullptr );
		return p_ans;
	}

	bool is_empty( void ) const
	{
		// return hph_head_.get().get() == hph_tail_.get().get();
		return hph_head_.load() == hph_tail_.load();
	}

	size_t profile_info_count( void ) const
	{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		return count_.load( std::memory_order_acquire );
#else
		return 0;
#endif
	}

private:
	using hazard_ptr_handler_t = typename od_node_link_by_hazard_handler::hazard_ptr_handler_t;
	using hazard_pointer       = typename od_node_link_by_hazard_handler::hazard_pointer;

	void push_back_common( node_pointer p_nd ) noexcept
	{
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
		pushpop_count_++;
#endif
		typename hazard_pointer::pointer p_link_of_nd = p_nd;                                     // od_node_link_by_hazard_handler*
		hazard_pointer                   hp_tail_node = hph_tail_.get();                          // od_node_link_by_hazard_handler::hazard_pointer
		hazard_pointer                   hp_tail_next = hp_tail_node->get_hazard_ptr_of_next();   // od_node_link_by_hazard_handler::hazard_pointer
		while ( true ) {
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
			pushpop_loop_count_++;
#endif
			if ( hp_tail_next == nullptr ) {
				typename hazard_pointer::pointer expected = nullptr;   // od_node_link_by_hazard_handler*
				if ( hp_tail_node->hazard_handler_of_next().compare_exchange_weak( expected, p_link_of_nd, std::memory_order_release, std::memory_order_acquire ) ) {
					expected = hp_tail_node.get();
					hph_tail_.compare_exchange_weak( expected, p_link_of_nd, std::memory_order_release, std::memory_order_acquire );
					break;
				}
			} else {
				typename hazard_pointer::pointer p_expected = hp_tail_node.get();
				hph_tail_.compare_exchange_weak( p_expected, hp_tail_next.get(), std::memory_order_release, std::memory_order_acquire );
			}
			hph_tail_.reuse( hp_tail_node );
			hp_tail_node->reuse_hazard_ptr_of_next( hp_tail_next );
		}

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		count_++;
#endif
		return;
	}

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

template <typename T, typename VALUE_DELETER = deleter_nothing<T>>
class x_fifo_list {
public:
	static_assert( ( !std::is_class<T>::value ) ||
	                   ( std::is_class<T>::value &&
	                     std::is_default_constructible<T>::value && std::is_copy_constructible<T>::value && std::is_copy_assignable<T>::value ),
	               "T should be default constructible, move constructible and move assignable at least" );

	using value_type = T;

	constexpr x_fifo_list( void ) noexcept
	  : lf_fifo_impl_( new node_type( nullptr ) )
	  , unused_node_pool_()
	{
	}

	x_fifo_list( size_t reserve_size ) noexcept
	  : x_fifo_list()
	{
	}
	~x_fifo_list()
	{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		if ( node_pool_t::profile_info_count() != 0 ) {
			internal::LogOutput( log_type::TEST, "%s", node_pool_t::profile_info_string().c_str() );
			node_pool_t::clear_as_possible_as();
		}
#endif

		VALUE_DELETER                deleter;
		std::tuple<bool, value_type> tmp = pop();
		while ( std::get<0>( tmp ) ) {
			deleter( std::get<1>( tmp ) );
			tmp = pop();
		}

		delete lf_fifo_impl_.release_sentinel_node();
	}

	void push( const T& v_arg )
	{
		node_pointer p_new_nd = unused_node_pool_.pop();
		if ( p_new_nd == nullptr ) {
			p_new_nd = new node_type( nullptr );
		}
		lf_fifo_impl_.push_back( v_arg, p_new_nd );
	}
	template <bool IsMoveConstructible = std::is_move_constructible<T>::value, bool IsMoveAssignable = std::is_copy_assignable<T>::value, typename std::enable_if<IsMoveConstructible && IsMoveAssignable>::type* = nullptr>
	void push( T&& v_arg )
	{
		node_pointer p_new_nd = unused_node_pool_.pop();
		if ( p_new_nd == nullptr ) {
			p_new_nd = new node_type( nullptr );
		}
		lf_fifo_impl_.push_back( std::move( v_arg ), p_new_nd );
	}

	template <bool IsMoveConstructible = std::is_move_constructible<T>::value, typename std::enable_if<IsMoveConstructible>::type* = nullptr>
	std::tuple<bool, value_type> pop( void )
	{
		// TがMove可能である場合に選択されるAPI実装
		std::tuple<node_pointer, value_type> pop_val = lf_fifo_impl_.pop_front();
		if ( std::get<0>( pop_val ) == nullptr ) return std::tuple<bool, value_type> { false, value_type {} };

		unused_node_pool_.push( std::get<0>( pop_val ) );
		return std::tuple<bool, value_type> { true, std::move( std::get<1>( pop_val ) ) };
	}
	template <bool IsMoveConstructible = std::is_move_constructible<T>::value, typename std::enable_if<!IsMoveConstructible>::type* = nullptr>
	std::tuple<bool, value_type> pop( void )
	{
		// TがMove不可能であるが、Copy可能である場合に選択されるAPI実装
		std::tuple<node_pointer, value_type> pop_val = lf_fifo_impl_.pop_front();
		if ( std::get<0>( pop_val ) == nullptr ) return std::tuple<bool, value_type> { false, value_type {} };

		unused_node_pool_.push( std::get<0>( pop_val ) );
		return std::tuple<bool, value_type> { true, std::get<1>( pop_val ) };
	}

	bool is_empty( void ) const
	{
		return lf_fifo_impl_.is_empty();
	}

private:
	using node_type    = od_node_basic<T>;
	using node_pointer = node_type*;

	using node_fifo_lockfree_t = node_fifo_lockfree_base<value_type>;
	using node_pool_t          = od_node_pool<node_type>;

	node_fifo_lockfree_t lf_fifo_impl_;
	node_pool_t          unused_node_pool_;
};

}   // namespace internal

/*!
 * @brief	semi-lock free FIFO type queue
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
class obsolate_fifo_list {
public:
	using fifo_type  = internal::fifo_nd_list<T, HAS_OWNERSHIP>;
	using input_type = typename fifo_type::input_type;
	using value_type = typename fifo_type::value_type;

	/*!
	 * @brief	Constructor
	 *
	 * In case that ALLOW_TO_ALLOCATE is false, argument pre_alloc_nodes must be equal or than 1.
	 * This value should be at least the number of CPUs.
	 * Also, it is recommended to double the number of threads to access.
	 */
	obsolate_fifo_list(
		unsigned int pre_alloc_nodes = 1   //!< [in]	number of pre-allocated internal free node
	)
	{
		if ( !ALLOW_TO_ALLOCATE && ( pre_alloc_nodes < 1 ) ) {
			internal::LogOutput( log_type::WARN, "Warning: in case that ALLOW_TO_ALLOCATE is false, argument pre_alloc_nodes must be equal or greater than 1, now %d. Therefore it will be modified to 1. This value should be at least the number of CPUs. Also, it is recommended to double the number of threads to access.", pre_alloc_nodes );
			pre_alloc_nodes = 1;
		}

		free_nd_.init_and_pre_allocate<fifo_node_type>( pre_alloc_nodes );
		return;
	}

	/*!
	 * @brief	Push a value to this FIFO queue
	 *
	 * cont_arg will copy to FIFO queue.
	 *
	 * @note
	 * In case that template parameter ALLOW_TO_ALLOCATE is true, this I/F is valid.
	 */
	template <bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push(
		const input_type& cont_arg   //!< [in]	a value to push this FIFO queue
		) -> typename std::enable_if<BOOL_VALUE, void>::type
	{
		push_common( cont_arg );
		return;
	}
	template <bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push(
		input_type&& cont_arg   //!< [in]	a value to push this FIFO queue
		) -> typename std::enable_if<BOOL_VALUE, void>::type
	{
		push_common( std::move( cont_arg ) );
		return;
	}

	/*!
	 * @brief	Push a value to this FIFO queue
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
		const input_type& cont_arg   //!< [in]	a value to push this FIFO queue
		) -> typename std::enable_if<!BOOL_VALUE, bool>::type
	{
		return push_common( cont_arg );
	}
	template <bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push(
		input_type&& cont_arg   //!< [in]	a value to push this FIFO queue
		) -> typename std::enable_if<!BOOL_VALUE, bool>::type
	{
		return push_common( std::move( cont_arg ) );
	}

	/*!
	 * @brief	Pop a value from this FIFO queue
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

		if ( p_poped_node == nullptr ) return std::tuple<bool, value_type>( false, std::move( ans_value ) );

		free_nd_.recycle( (free_node_pointer)p_poped_node );

		return std::tuple<bool, value_type>( true, std::move( ans_value ) );
	}

#ifdef ALCONCURRENT_CONF_ENABLE_SIZE_INFO_FROFILE
	/*!
	 * @brief	number of the queued values in FIFO
	 *
	 * @warning
	 * This FIFO will be access by several thread concurrently. So, true number of this FIFO queue may be changed when caller uses the returned value.
	 */
	int get_size( void ) const
	{
		return fifo_.get_size();
	}

	/*!
	 * @brief	get the total number of the allocated internal nodes
	 *
	 * @warning
	 * This FIFO will be access by several thread concurrently. So, true number of this FIFO queue may be changed when caller uses the returned value.
	 */
	int get_allocated_num( void )
	{
		return free_nd_.get_allocated_num();
	}
#endif

	bool is_empty( void ) const
	{
		return fifo_.get_size() == 0;
	}

private:
	obsolate_fifo_list( const obsolate_fifo_list& )            = delete;
	obsolate_fifo_list( obsolate_fifo_list&& )                 = delete;
	obsolate_fifo_list& operator=( const obsolate_fifo_list& ) = delete;
	obsolate_fifo_list& operator=( obsolate_fifo_list&& )      = delete;

	using free_nd_storage_type = internal::free_nd_storage;
	using free_node_type       = typename free_nd_storage_type::node_type;
	using free_node_pointer    = typename free_nd_storage_type::node_pointer;
	using fifo_node_type       = typename fifo_type::node_type;
	using fifo_node_pointer    = typename fifo_type::node_pointer;

	template <typename TRANSFER_T, bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push_common(
		TRANSFER_T cont_arg   //!< [in]	a value to push this FIFO queue
		) -> typename std::enable_if<BOOL_VALUE, void>::type
	{
		std::atomic_thread_fence( std::memory_order_release );

		fifo_node_pointer p_new_node = free_nd_.allocate<fifo_node_type>(
			true,
			[this]( fifo_node_pointer p_chk_node ) {
				return !( this->fifo_.check_hazard_list( p_chk_node ) );
				//				return false;
			} );

		p_new_node->set_value( std::forward<TRANSFER_T>( cont_arg ) );

		fifo_.push( p_new_node );
		return;
	}

	template <typename TRANSFER_T, bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto push_common(
		TRANSFER_T cont_arg   //!< [in]	a value to push this FIFO queue
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
			p_new_node->set_value( std::forward<TRANSFER_T>( cont_arg ) );

			fifo_.push( p_new_node );
			return true;
		} else {
			return false;
		}
	}

	fifo_type            fifo_;
	free_nd_storage_type free_nd_;
};

template <typename T, typename VALUE_DELETER = internal::deleter_nothing<T>>
class fifo_list : public internal::x_fifo_list<T, VALUE_DELETER> {
public:
	fifo_list( void ) = default;
	fifo_list( size_t reserve_size ) noexcept
	  : fifo_list()
	{
	}
};
template <>
class fifo_list<void*> : public internal::x_fifo_list<void*, internal::deleter_nothing<void>> {
public:
	fifo_list( void ) = default;
	fifo_list( size_t reserve_size ) noexcept
	  : fifo_list()
	{
	}
};
template <typename T>
class fifo_list<T*> : public internal::x_fifo_list<T*, std::default_delete<T>> {
public:
	fifo_list( void ) = default;
	fifo_list( size_t reserve_size ) noexcept
	  : fifo_list()
	{
	}
};
template <typename T>
class fifo_list<T[]> : public internal::x_fifo_list<T*, std::default_delete<T[]>> {
public:
	using value_type = T[];

	fifo_list( void ) = default;
	fifo_list( size_t reserve_size ) noexcept
	  : fifo_list()
	{
	}
};
template <typename T, size_t N>
class fifo_list<T[N]> : public internal::x_fifo_list<std::array<T, N>, internal::deleter_nothing<std::array<T, N>>> {
public:
	using value_type = T[N];

	fifo_list( void ) = default;
	fifo_list( size_t reserve_size ) noexcept
	  : fifo_list()
	{
	}

	void push(
		const value_type& cont_arg   //!< [in]	a value to push this FIFO queue
	)
	{
		std::array<T, N> tmp;
		for ( size_t i = 0; i < N; i++ ) {
			tmp[i] = cont_arg[i];
		}

		internal::x_fifo_list<std::array<T, N>, internal::deleter_nothing<std::array<T, N>>>::push( std::move( tmp ) );
	}
	void push(
		value_type&& cont_arg   //!< [in]	a value to push this FIFO queue
	)
	{
		std::array<T, N> tmp;
		for ( size_t i = 0; i < N; i++ ) {
			tmp[i] = std::move( cont_arg[i] );
		}

		internal::x_fifo_list<std::array<T, N>, internal::deleter_nothing<std::array<T, N>>>::push( std::move( tmp ) );
	}

	std::tuple<bool, value_type> pop( void )
	{
		std::tuple<bool, value_type> ans;
		std::get<0>( ans ) = false;

		std::tuple<bool, std::array<T, N>> ret = internal::x_fifo_list<std::array<T, N>, internal::deleter_nothing<std::array<T, N>>>::pop();
		if ( !std::get<0>( ret ) ) {
			return ans;
		}

		std::get<0>( ans ) = true;
		for ( size_t i = 0; i < N; i++ ) {
			std::get<1>( ans )[i] = std::move( std::get<1>( ret )[i] );
		}
		return ans;
	}
};
template <typename T, typename DELETER>
class fifo_list<T*, DELETER> : public internal::x_fifo_list<T*, DELETER> {
public:
	fifo_list( void ) = default;
	fifo_list( size_t reserve_size ) noexcept
	  : fifo_list()
	{
	}
};
template <typename T, typename DELETER>
class fifo_list<T[], DELETER> : public internal::x_fifo_list<T*, DELETER> {
public:
	using value_type = T[];

	fifo_list( void ) = default;
	fifo_list( size_t reserve_size ) noexcept
	  : fifo_list()
	{
	}
};
// template <typename T, size_t N, typename DELETER>
// class fifo_list<T[N], DELETER> : public internal::x_fifo_list<T*, DELETER> {
// public:
// 	using value_type = T[N];

// 	fifo_list( void ) = default;
// 	fifo_list( size_t reserve_size ) noexcept
// 	  : fifo_list()
// 	{
// 	}
// };

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_LF_FIFO_HPP_ */
