/*!
 * @file	lf_list.hpp
 * @brief
 * @author	alpha
 * @date	Created on 2021/02/23
 * @details
 *
 * Copyright (C) 2021 by alpha <e-mail address>
 */

#ifndef INC_LF_LIST_HPP_
#define INC_LF_LIST_HPP_

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
 * @breif	単方向リスト
 *
 * Tは、trivially copyableでなければならい。
 *
 * @note
 * https://www.slideshare.net/kumagi/lock-free-safe?next_slideshow=1 @n
 */
template <typename T, typename DELETER = default_deleter<T>>
class lockfree_list_base {
public:
	static constexpr int hzrd_max_slot_ = 4;
	using value_type                    = T;
	using node_type                     = one_way_list_node_markable<T>;
	using node_pointer                  = node_type*;
	using iterator                      = node_pointer;
	using const_iterator                = const node_pointer;
	using hazard_ptr_storage            = hazard_ptr<node_type, hzrd_max_slot_>;
	using scoped_hazard_ref             = hazard_ptr_scoped_ref<node_type, hzrd_max_slot_>;

	lockfree_list_base( void )
	  : p_sentinel_node( new node_type() )
	  , head_()
	  , size_count_( 0 )
	{
		head_.set_next( p_sentinel_node );

		return;
	}

	~lockfree_list_base()
	{
		node_pointer p_expect_sentinel = head_.load( std::memory_order_acquire );

		assert( p_expect_sentinel == p_sentinel_node );   // need to move all nodes to free node list before destruction

		delete p_sentinel_node;

		return;
	}

	/*!
	 * @breif	条件に合致する最初のノードとその直前のノードを見つける。
	 *
	 * @retval	1st-value	直前のノードへのイテレータ
	 * @retval	2nd-value	最初に条件に合致したノードへのイテレータ
	 */
	template <typename Hazard_ref, typename Predicate>
	std::tuple<iterator, iterator> find_if(
		free_nd_storage& fn_strg,        //!< [in]	削除ノードを差し戻すためのFree node storageへの参照
		Hazard_ref&      ans_prev_ref,   //!< [in]	戻り値のprevに対応するハザードポインタの登録先
		Hazard_ref&      ans_curr_ref,   //!< [in]	戻り値のcurrに対応するハザードポインタの登録先
		Predicate        pred            //!< [in]	引数には、const node_pointerが渡される
	)
	{
		scoped_hazard_ref scoped_ref_prev( hzrd_ptr_, (int)hazard_ptr_idx::FIND_FUNC_PREV );
		scoped_hazard_ref scoped_ref_curr( hzrd_ptr_, (int)hazard_ptr_idx::FIND_FUNC_CURR );
		scoped_hazard_ref scoped_ref_next( hzrd_ptr_, (int)hazard_ptr_idx::FIND_FUNC_NEXT );

		ans_prev_ref.regist_ptr_as_hazard_ptr( nullptr );
		ans_curr_ref.regist_ptr_as_hazard_ptr( nullptr );

		node_pointer p_prev = nullptr;
		node_pointer p_curr = nullptr;
		node_pointer p_next = nullptr;

		while ( true ) {
			p_prev = &head_;
			scoped_ref_prev.regist_ptr_as_hazard_ptr( nullptr );

			auto next_addr_and_mark = p_prev->get_next();
			p_curr                  = std::get<0>( next_addr_and_mark );
			scoped_ref_curr.regist_ptr_as_hazard_ptr( p_curr );
			next_addr_and_mark = p_prev->get_next();
			if ( p_curr != std::get<0>( next_addr_and_mark ) ) continue;

			while ( true ) {
				next_addr_and_mark = p_curr->get_next();
				p_next             = std::get<0>( next_addr_and_mark );
				scoped_ref_next.regist_ptr_as_hazard_ptr( p_next );
				if ( p_next == nullptr ) {
					// p_currがp_sentinelノードを指していることを意味する。よって、検索失敗でリターンする。
					return std::tuple<iterator, iterator>( nullptr, nullptr );
				}
				next_addr_and_mark = p_curr->get_next();                       // p_nextがまだ有効かどうかを再確認
				if ( p_next != std::get<0>( next_addr_and_mark ) ) continue;   // nextが変わっていたので、nextを取だし直す。

				if ( std::get<1>( next_addr_and_mark ) ) {             // 	p_currに削除マークがついていたら、削除を試みる
					bool snip = p_prev->next_CAS( &p_curr, p_next );   // p_currを削除を試みる。
					if ( !snip ) {
						// 削除に失敗。誰かがすでに削除を成功させたので、検索を最初からやり直す。
						break;
					}

					node_pointer p_detached = p_curr;
					p_curr                  = p_next;
					scoped_ref_curr.regist_ptr_as_hazard_ptr( p_curr );   // p_currのハザードポインタを更新

					// p_currを削除できたので、フリーノードストレージへ登録
					DELETER dt;
					dt( p_detached->ref_value() );
					fn_strg.recycle( p_detached );

					continue;   // p_currの削除フラグ確認からやり直す。
				}

				// p_currは削除されていないので、条件に当てはまるかどうかを確認する
				if ( pred( (const node_pointer)p_curr ) ) {
					// 条件に合致したので、検索を完了する
					ans_prev_ref.regist_ptr_as_hazard_ptr( p_prev );
					ans_curr_ref.regist_ptr_as_hazard_ptr( p_curr );
					return std::tuple<iterator, iterator>( p_prev, p_curr );
				}

				// p_currは検索条件に合致しなかったので、次を調べる。
				p_prev = p_curr;
				p_curr = p_next;
				scoped_ref_prev.regist_ptr_as_hazard_ptr( p_prev );   // p_prevのハザードポインタを更新
				scoped_ref_curr.regist_ptr_as_hazard_ptr( p_curr );   // p_currのハザードポインタを更新
			}
		}

		return std::tuple<iterator, iterator>( nullptr, nullptr );
	}

	/*!
	 * @breif	ノードを挿入する
	 *
	 * @retval	true	挿入に成功
	 * @retval	false	挿入に失敗
	 *
	 * @warning
	 * 引数の管理ノードは、事前にcheck_hazard_list()を使用してハザードポインタに登録されていないことを確認していること。
	 */
	bool insert(
		node_pointer const p_push_node,   //!< [in]	新たに挿入するノード
		iterator           prev,          //!< [in]	find_ifの戻り値の第１パラメータ
		iterator           curr           //!< [in]	find_ifの戻り値の第２パラメータ
	)
	{
		p_push_node->set_next( curr );

		node_pointer curr_back = curr;

		bool ans = prev->next_CAS( &curr_back, p_push_node );
		if ( ans ) {
			++size_count_;
		}

		return ans;
	}

	/*!
	 * @breif	ノードを削除する
	 *
	 * @retval	true	削除に成功
	 * @retval	false	削除に失敗
	 *
	 * @warning
	 * 引数の管理ノードは、事前にcheck_hazard_list()を使用してハザードポインタに登録されていないことを確認していること。
	 */
	bool remove(
		free_nd_storage& fn_strg,   //!< [in]	削除ノードを差し戻すためのFree node storageへの参照
		iterator         prev,      //!< [in]	find_ifの戻り値の第１パラメータ
		iterator         curr       //!< [in]	find_ifの戻り値の第２パラメータ。削除するノード
	)
	{
		bool ans = curr->set_mark_on();

		if ( ans ) {
			--size_count_;

			scoped_hazard_ref scoped_ref_next( hzrd_ptr_, (int)hazard_ptr_idx::REMOVE_FUNC_NEXT );

			node_pointer p_curr = curr;

			auto         next_addr_and_mark = p_curr->get_next();
			node_pointer p_next             = std::get<0>( next_addr_and_mark );
			scoped_ref_next.regist_ptr_as_hazard_ptr( p_next );
			next_addr_and_mark = p_curr->get_next();   // p_nextがまだ有効かどうかを再確認
			if ( p_next == std::get<0>( next_addr_and_mark ) ) {
				if ( p_next != nullptr ) {
					if ( prev->next_CAS( &p_curr, p_next ) ) {
						DELETER dt;
						dt( curr->ref_value() );
						fn_strg.recycle( curr );
					}
					// 失敗しても気にしない。
				}
			}
		}

		return ans;
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

	/*!
	 * @breif	現在、リスト内に存在するノード数を得る
	 *
	 * @warning
	 * 必ずしも取得した瞬間の状態を正確に反映するわけではないので、参考程度の使用にとどめること。
	 */
	int get_size( void )
	{
		return size_count_.load( std::memory_order_acquire );
	}

private:
	lockfree_list_base( const lockfree_list_base& ) = delete;
	lockfree_list_base( lockfree_list_base&& )      = delete;
	lockfree_list_base operator=( const lockfree_list_base& ) = delete;
	lockfree_list_base operator=( lockfree_list_base&& ) = delete;

	enum class hazard_ptr_idx : int {
		FIND_FUNC_PREV   = 0,
		FIND_FUNC_CURR   = 1,
		FIND_FUNC_NEXT   = 2,
		REMOVE_FUNC_NEXT = 3
	};

	node_pointer     p_sentinel_node;
	node_type        head_;
	std::atomic<int> size_count_;

	hazard_ptr_storage hzrd_ptr_;
};

}   // namespace internal

/*!
 * @breif	semi-lock free list
 *
 * Type T should be trivially copyable.
 *
 * In case that template parameter ALLOW_TO_ALLOCATE is true, @n
 * In case of no avialable free node that carries a value, new node is allocated from heap internally. @n
 * In this case, this queue may be locked. And push() may trigger this behavior.
 *
 * On the other hand, used free node will be recycled without a memory allocation. In this case, push() is lock free.
 *
 * To reduce lock behavior, pre-allocated nodes are effective. @n
 * get_allocated_num() provides the number of the allocated nodes. This value is hint to configuration.
 *
 * In case that template parameter ALLOW_TO_ALLOCATE is false, @n
 * In case of no avialable free node, push() member function will return false. In this case, it fails to push a value @n
 * User side has a role to recover this condition by User side itself, e.g. backoff approach.
 *
 * @note
 * To resolve ABA issue, this Stack queue uses hazard pointer approach.
 */
template <typename T, bool ALLOW_TO_ALLOCATE = true, typename DELETER = internal::default_deleter<T>>
class lockfree_list {
public:
	using value_type        = T;
	using list_type         = internal::lockfree_list_base<T, DELETER>;
	using list_node_type    = typename list_type::node_type;
	using list_node_pointer = typename list_type::node_pointer;

	/*!
	 * @breif	Constructor
	 *
	 * In case that ALLOW_TO_ALLOCATE is false, argument pre_alloc_nodes must be equal or than 1.
	 * This value should be at least the number of CPUs.
	 * Also, it is recommended to double the number of threads to access.
	 */
	lockfree_list(
		unsigned int pre_alloc_nodes = 1   //!< [in]	number of pre-allocated internal free node
	)
	{
		if ( !ALLOW_TO_ALLOCATE && ( pre_alloc_nodes < 1 ) ) {
			LogOutput( log_type::WARN, "Warning: in case that ALLOW_TO_ALLOCATE is false, argument pre_alloc_nodes must be equal or than 1, now %d. Therefore it will be modified to 1. This value should be at least the number of CPUs. Also, it is recommended to double the number of threads to access.", pre_alloc_nodes );
			pre_alloc_nodes = 1;
		}

		free_nd_.init_and_pre_allocate<list_node_type>( pre_alloc_nodes );
		return;
	}

	/*!
	 * @breif	Insert a value to this list
	 *
	 * cont_arg will copy to list.
	 *
	 * @return	success or fail to insert
	 * @retval	true	success to insert copy value of cont_arg to list
	 * @retval	false	fail to insert cont_arg value to list
	 *
	 * @note
	 * In case that template parameter ALLOW_TO_ALLOCATE is true, this I/F is valid.
	 */
	template <typename Predicate, bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto insert(
		const T&  cont_arg,   //!< [in]	a value to push this LIFO queue
		Predicate pred        //!< [in]	引数には、const list_node_pointerが渡される
		) -> typename std::enable_if<BOOL_VALUE, bool>::type
	{
		list_node_pointer p_new_node = free_nd_.allocate<list_node_type>(
			true,
			[this]( list_node_pointer p_chk_node ) {
				return !( this->base_list_.check_hazard_list( p_chk_node ) ||
			              this->hzrd_ptr_.check_ptr_in_hazard_list( p_chk_node ) );
				//				return false;
			} );

		p_new_node->set_value( cont_arg );

		scoped_hazard_ref hzrd_ref_prev( hzrd_ptr_, (int)hazard_ptr_idx::FIND_ANS_PREV );
		scoped_hazard_ref hzrd_ref_curr( hzrd_ptr_, (int)hazard_ptr_idx::FIND_ANS_CURR );
		while ( true ) {
			auto [p_prev, p_curr] = base_list_.find_if( free_nd_, hzrd_ref_prev, hzrd_ref_curr, pred );

			if ( p_prev == nullptr ) return false;

			if ( base_list_.insert( p_new_node, p_prev, p_curr ) ) {
				break;
			}
		}

		return true;
	}

	/*!
	 * @breif	Insert a value to this list
	 *
	 * cont_arg will copy to list.
	 *
	 * @return	success or fail to insert
	 * @retval	true	success to insert copy value of cont_arg to list
	 * @retval	false	fail to insert cont_arg value to list
	 *
	 * @note
	 * @li	In case that template parameter ALLOW_TO_ALLOCATE is false, this I/F is valid.
	 * @li	In case that return value is false, User side has a role to recover this condition by User side itself, e.g. backoff approach.
	 */
	template <typename Predicate, bool BOOL_VALUE = ALLOW_TO_ALLOCATE>
	auto insert(
		const T&  cont_arg,   //!< [in]	a value to push this LIFO queue
		Predicate pred        //!< [in]	引数には、const list_node_pointerが渡される
		) -> typename std::enable_if<!BOOL_VALUE, bool>::type
	{
		list_node_pointer p_new_node = free_nd_.allocate<list_node_type>(
			false,
			[this]( list_node_pointer p_chk_node ) {
				return !( this->base_list_.check_hazard_list( p_chk_node ) ||
			              this->hzrd_ptr_.check_ptr_in_hazard_list( p_chk_node ) );
				//				return false;
			} );

		if ( p_new_node == nullptr ) return false;

		p_new_node->set_value( cont_arg );

		scoped_hazard_ref hzrd_ref_prev( hzrd_ptr_, (int)hazard_ptr_idx::FIND_ANS_PREV );
		scoped_hazard_ref hzrd_ref_curr( hzrd_ptr_, (int)hazard_ptr_idx::FIND_ANS_CURR );
		while ( true ) {
			auto [p_prev, p_curr] = base_list_.find_if( free_nd_, hzrd_ref_prev, hzrd_ref_curr, pred );

			if ( p_prev == nullptr ) return false;

			if ( base_list_.insert( p_new_node, p_prev, p_curr ) ) {
				break;
			}
		}
		return true;
	}

	/*!
	 * @breif	remove all of nodes that pred return true from this list
	 */
	template <typename Predicate>
	void remove_if(
		Predicate pred   //!< [in]	引数には、const list_node_pointerが渡される
	)
	{
		scoped_hazard_ref hzrd_ref_prev( hzrd_ptr_, (int)hazard_ptr_idx::FIND_ANS_PREV );
		scoped_hazard_ref hzrd_ref_curr( hzrd_ptr_, (int)hazard_ptr_idx::FIND_ANS_CURR );
		while ( true ) {
			auto [p_prev, p_curr] = base_list_.find_if( free_nd_, hzrd_ref_prev, hzrd_ref_curr, pred );

			if ( p_prev == nullptr ) break;

			base_list_.remove( free_nd_, p_prev, p_curr );
		}

		return;
	}

	/*!
	 * @breif	remove a first node that pred return true from this list
	 */
	template <typename Predicate>
	void erase_if(
		Predicate pred   //!< [in]	引数には、const list_node_pointerが渡される
	)
	{
		scoped_hazard_ref hzrd_ref_prev( hzrd_ptr_, (int)hazard_ptr_idx::FIND_ANS_PREV );
		scoped_hazard_ref hzrd_ref_curr( hzrd_ptr_, (int)hazard_ptr_idx::FIND_ANS_CURR );
		while ( true ) {
			auto [p_prev, p_curr] = base_list_.find_if( free_nd_, hzrd_ref_prev, hzrd_ref_curr, pred );

			if ( p_prev == nullptr ) break;

			if ( base_list_.remove( free_nd_, p_prev, p_curr ) ) break;
		}

		return;
	}

	/*!
	 * @breif	number of the queued values in FIFO
	 *
	 * @warning
	 * This FIFO will be access by several thread concurrently. So, true number of this FIFO queue may be changed when caller uses the returned value.
	 */
	int get_size( void )
	{
		return base_list_.get_size();
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
	lockfree_list( const lockfree_list& ) = delete;
	lockfree_list( lockfree_list&& )      = delete;
	lockfree_list operator=( const lockfree_list& ) = delete;
	lockfree_list operator=( lockfree_list&& ) = delete;

	using free_nd_storage_type = internal::free_nd_storage;
	using free_node_type       = typename free_nd_storage_type::node_type;
	using free_node_pointer    = typename free_nd_storage_type::node_pointer;

	static constexpr int hzrd_max_slot_ = 2;
	using hazard_ptr_storage            = hazard_ptr<list_node_type, hzrd_max_slot_>;
	using scoped_hazard_ref             = hazard_ptr_scoped_ref<list_node_type, hzrd_max_slot_>;

	enum class hazard_ptr_idx : int {
		FIND_ANS_PREV = 0,
		FIND_ANS_CURR = 1
	};

	list_type            base_list_;
	free_nd_storage_type free_nd_;

	hazard_ptr_storage hzrd_ptr_;
};

}   // namespace concurrent
}   // namespace alpha

#endif /* INC_LF_LIST_HPP_ */
