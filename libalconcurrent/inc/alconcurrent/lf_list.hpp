/*!
 * @file	lf_list.hpp
 * @brief	semi lock-free list
 * @author	Teruaki Ata
 * @date	Created on 2021/02/23
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef INC_LF_LIST_HPP_
#define INC_LF_LIST_HPP_

#include <atomic>
#include <functional>
#include <memory>
#include <tuple>

#include "free_node_storage.hpp"
#include "hazard_ptr.hpp"
#include "one_way_list_node.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

/*!
 * @breif	ロックフリー方式用の単方向型リスト
 *
 * Tは、copyable、あるいはmovableでなければならい。
 *
 * @note
 * std::function型を多用している理由は以下。 @n
 * テンプレートメンバ関数の場合、生成される関数インスタンスが予測できないため、ハザードポインタを管理するスロット数を事前確定できなくな。 @n
 * そのため、メンバ関数のテンプレート化を避ける必要がある。
 */
template <typename T, typename DELETER = default_deleter<T>>
class lockfree_list_base {
public:
	using value_type                 = typename std::decay<T>::type;                //!< 保持対象の型
	using node_type                  = one_way_list_node_markable<T>;               //!< 実際の値を保持するノードクラス。
	using node_pointer               = node_type*;                                  //!< 実際の値を保持するノードクラスのインスタンスへのポインタ型
	using find_predicate_t           = std::function<bool( const node_pointer )>;   //!< find関数で使用する述語関数を保持するfunction型
	using for_each_func_t            = std::function<void( value_type& )>;          //!< for_each関数で各要素の処理を実行するための関数を保持するfunction型
	using remove_operator_t          = std::function<void( value_type& )>;          //!< remove処理が行われたノードが保持する保持対象のインスタンスに対して何らかの処理を行う関数を保持するfunction型
	using hazard_ptr_scoped_ref_if_t = hazard_ptr_scoped_ref_if<node_type>;         //!< ハザードポインタの参照制御をサポートするI/Fクラスの型

	lockfree_list_base( void )
	  : head_()
	  , sentinel_node_()
	  , size_count_( 0 )
	{
		head_.set_next( &sentinel_node_ );

		return;
	}

	~lockfree_list_base()
	{
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
		auto [p_cur, cur_mark] = head_.get_next();
#else
		auto local_ret = head_.get_next();
		auto p_cur     = std::get<0>( local_ret );
//			auto cur_mark = std::get<1>( local_ret );
#endif

		if ( !is_end_node( p_cur ) ) {
			LogOutput( log_type::ERR, "ERR: lockfree_list_base is deleted before all data deletion." );
		}

		return;
	}

	/*!
	 * @breif	条件に合致する最初のノードとその直前のノードを見つける。
	 *
	 * 検索で、predが真を返した場合
	 * @retval	1st-value	直前のノードへのポインタ
	 * @retval	2nd-value	最初にpredが真を返したノードへのポインタ
	 *
	 * 検索で、predが真を返さなかった場合
	 * @retval	1st-value	番兵ノードより１つ前のノードへのポインタ
	 * @retval	2nd-value	番兵ノードへのポインタ
	 */
	std::tuple<node_pointer, node_pointer> find_if(
		free_nd_storage&            fn_strg,        //!< [in]	削除ノードを差し戻すためのFree node storageへの参照
		hazard_ptr_scoped_ref_if_t& ans_prev_ref,   //!< [in]	戻り値のprevに対応するnodeポインタを登録するハザードポインタへの参照
		hazard_ptr_scoped_ref_if_t& ans_curr_ref,   //!< [in]	戻り値のcurrに対応するnodeポインタを登録するハザードポインタへの参照
		find_predicate_t&           pred            //!< [in]	引数には、const node_pointerが渡される
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
					// p_currがp_sentinelノードを指していることを意味する。よって、番兵ノードを返す
					ans_prev_ref.regist_ptr_as_hazard_ptr( p_prev );
					ans_curr_ref.regist_ptr_as_hazard_ptr( p_curr );
					return std::tuple<node_pointer, node_pointer>( p_prev, p_curr );
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
					DELETER dt;   // TODO 本当にフリーノードストレージへ戻せたときに削除処理を行う必要がある。。。
					dt( p_detached->ref_value() );
					fn_strg.recycle( p_detached );

					continue;   // p_currの削除フラグ確認からやり直す。
				}

				// p_currは削除されていないので、条件に当てはまるかどうかを確認する
				if ( pred( p_curr ) ) {
					// 条件に合致したので、検索を完了する
					ans_prev_ref.regist_ptr_as_hazard_ptr( p_prev );
					ans_curr_ref.regist_ptr_as_hazard_ptr( p_curr );
					return std::tuple<node_pointer, node_pointer>( p_prev, p_curr );
				}

				// p_currは検索条件に合致しなかったので、次を調べる。
				p_prev = p_curr;
				p_curr = p_next;
				scoped_ref_prev.regist_ptr_as_hazard_ptr( p_prev );   // p_prevのハザードポインタを更新
				scoped_ref_curr.regist_ptr_as_hazard_ptr( p_curr );   // p_currのハザードポインタを更新
			}
		}

		LogOutput( log_type::ERR, "ERR: find_if_common go into logic error" );
		return std::tuple<node_pointer, node_pointer>( nullptr, nullptr );   // 到達不可能コード。安全のために残す。
																			 // TODO ここに到達する場合は、アルゴリズムか実装がおかしいので、例外をスローするか？
	}

	/*!
	 * @breif	prevとcurrの間にノードを挿入する
	 *
	 * @retval	true	挿入に成功
	 * @retval	false	挿入に失敗
	 *
	 * @warning
	 * 引数の管理ノードは、事前にcheck_hazard_list()を使用してハザードポインタに登録されていないことを確認していること。
	 */
	bool insert(
		node_pointer const p_push_node,   //!< [in]	新たに挿入するノード
		node_pointer       prev,          //!< [in]	find_ifの戻り値の第１パラメータ
		node_pointer       curr           //!< [in]	find_ifの戻り値の第２パラメータ
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
	 * @breif	currのノードを削除する
	 *
	 * @retval	true	削除に成功
	 * @retval	false	削除に失敗
	 *
	 * @warning
	 * 引数の管理ノードは、事前にcheck_hazard_list()を使用してハザードポインタに登録されていないことを確認していること。
	 */
	bool remove(
		free_nd_storage&   fn_strg,   //!< [in]	削除ノードを差し戻すためのFree node storageへの参照
		node_pointer       prev,      //!< [in]	find_ifの戻り値の第１パラメータ
		node_pointer       curr,      //!< [in]	find_ifの戻り値の第２パラメータ。削除するノード
		remove_operator_t& rm_op      //!< [in/out]	currの削除に成功した場合、currが保持している値への参照を引数に呼び出される。
	)
	{
		bool ans = curr->set_mark_on();

		if ( ans ) {
			--size_count_;

			rm_op( curr->ref_value() );

			scoped_hazard_ref scoped_ref_next( hzrd_ptr_, (int)hazard_ptr_idx::REMOVE_FUNC_NEXT );

			node_pointer p_curr = curr;

			auto         next_addr_and_mark = p_curr->get_next();
			node_pointer p_next             = std::get<0>( next_addr_and_mark );
			scoped_ref_next.regist_ptr_as_hazard_ptr( p_next );
			next_addr_and_mark = p_curr->get_next();   // p_nextがまだ有効かどうかを再確認
			if ( p_next == std::get<0>( next_addr_and_mark ) ) {
				if ( p_next != nullptr ) {
					if ( prev->next_CAS( &p_curr, p_next ) ) {
						fn_strg.recycle( curr );
					}
					// 失敗しても気にしない。
				}
			}
		}

		return ans;
	}

	/*!
	 * @breif	currのノードを削除する
	 *
	 * 削除時には、ノードが保持する値に対して、DELETERが適用される。
	 *
	 * @retval	true	削除に成功
	 * @retval	false	削除に失敗
	 *
	 * @warning
	 * 引数の管理ノードは、事前にcheck_hazard_list()を使用してハザードポインタに登録されていないことを確認していること。
	 */
	bool remove(
		free_nd_storage& fn_strg,   //!< [in]	削除ノードを差し戻すためのFree node storageへの参照
		node_pointer     prev,      //!< [in]	find_ifの戻り値の第１パラメータ
		node_pointer     curr       //!< [in]	find_ifの戻り値の第２パラメータ。削除するノード
	)
	{
		remove_operator_t rm_op = DELETER();
		return remove( fn_strg, prev, curr, rm_op );
	}

	/*!
	 * @breif	Applies the specified function to all elements.
	 *
	 * @pre
	 * The Function must meet the MoveConstructible requirement, but not the CopyConstructible requirement.
	 *
	 * @warning
	 * Due to the lock-free nature, it is possible that the element being processed may be deleted
	 * (just marked for deletion, the actual deletion may even be delayed until all threads are no
	 * longer referencing it) or modified.
	 * Also, access contention may occur during function execution. Therefore, when changing an element,
	 * it is necessary to perform access control such as exclusive control in Function f.
	 * Therefore, when a non-lock-free function such as exclusive control by mutex is used, the lock-free
	 * property of this class is limited.
	 */
	for_each_func_t for_each(
		for_each_func_t&& f   //!< [in]	A function f is passed value_type& as an argument
	)
	{
		for_each_func_t internal_func = std::move( f );

		scoped_hazard_ref scoped_ref_prev( hzrd_ptr_, (int)hazard_ptr_idx::FOR_EACH_PREV );
		scoped_hazard_ref scoped_ref_curr( hzrd_ptr_, (int)hazard_ptr_idx::FOR_EACH_CURR );

		node_pointer p_prev = &head_;

		while ( true ) {
			auto         next_addr_and_mark = p_prev->get_next();
			node_pointer p_curr             = std::get<0>( next_addr_and_mark );
			scoped_ref_curr.regist_ptr_as_hazard_ptr( p_curr );
			next_addr_and_mark = p_prev->get_next();   // p_currがまだ有効かどうかを再確認

			if ( p_curr == nullptr ) break;           // nullptrの場合、何らかのエラー、あるいはp_prevが番兵ノードなので、終了する。
			if ( p_curr == &sentinel_node_ ) break;   // p_currが番兵ノードなので、終了する。

			if ( p_curr != std::get<0>( next_addr_and_mark ) ) continue;   // p_currポインタが書き換わっていたら読み出しからやり直し。
			if ( !std::get<1>( next_addr_and_mark ) ) {
				// ノードに削除マークがなければ、関数適用開始
				internal_func( p_curr->ref_value() );
			}

			scoped_ref_prev.regist_ptr_as_hazard_ptr( p_curr );
			p_prev = p_curr;
		}

		return std::move( internal_func );
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
	int get_size( void ) const
	{
		return size_count_.load( std::memory_order_acquire );
	}

	/*!
	 * @breif	ヘッドノードかどうかを調べる。
	 *
	 * @retval	true	p_chk_ptr is same to termination node.
	 * @retval	false	p_chk_ptr is not same to termination node.
	 */
	bool is_head_node( const node_pointer p_chk_ptr ) const
	{
		return ( p_chk_ptr == &head_ );
	}

	/*!
	 * @breif	終端ノード（番兵ノード）かどうかを調べる。
	 *
	 * @retval	true	p_chk_ptr is same to termination node.
	 * @retval	false	p_chk_ptr is not same to termination node.
	 */
	bool is_end_node( const node_pointer p_chk_ptr ) const
	{
		return ( p_chk_ptr == &sentinel_node_ );
	}

private:
	static constexpr int hzrd_max_slot_ = 6;
	using hazard_ptr_storage            = hazard_ptr<node_type, hzrd_max_slot_>;
	using scoped_hazard_ref             = hazard_ptr_scoped_ref<node_type, hzrd_max_slot_>;

	lockfree_list_base( const lockfree_list_base& ) = delete;
	lockfree_list_base( lockfree_list_base&& )      = delete;
	lockfree_list_base operator=( const lockfree_list_base& ) = delete;
	lockfree_list_base operator=( lockfree_list_base&& ) = delete;

	enum class hazard_ptr_idx : int {
		FIND_FUNC_PREV   = 0,
		FIND_FUNC_CURR   = 1,
		FIND_FUNC_NEXT   = 2,
		REMOVE_FUNC_NEXT = 3,
		FOR_EACH_PREV    = 4,
		FOR_EACH_CURR    = 5
	};

	node_type        head_;
	node_type        sentinel_node_;
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
	using value_type      = typename std::decay<T>::type;
	using predicate_t     = std::function<bool( const value_type& )>;
	using for_each_func_t = std::function<void( value_type& )>;

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
	 * @breif	Constructor
	 *
	 * In case that ALLOW_TO_ALLOCATE is false, argument pre_alloc_nodes must be equal or than 1.
	 * This value should be at least the number of CPUs.
	 * Also, it is recommended to double the number of threads to access.
	 */
	~lockfree_list()
	{
		predicate_t pred_common = []( const value_type& a ) { return true; };
		remove_all_if( pred_common );
	}

	/*!
	 * @breif	Insert a value to this list
	 *
	 * cont_arg will copy to list.
	 *
	 * The value of cont_arg is inserted before the node that returned true in the predicate function.
	 *
	 * @return	1st return value. Success or fail to insert
	 * @retval	true	success to insert copy value of cont_arg to list
	 * @retval	false	fail to insert copy value of cont_arg, because pred does not return true for any items in this list.
	 *
	 * @return	2nd return value. Success or fail to allocate internal node.
	 * @retval	true	success to allocate internal node
	 * @retval	false	fail the node allocation from free node strage.
	 *
	 * @note
	 * In case that template parameter ALLOW_TO_ALLOCATE is false, 2nd paramter has a possibility that returns false. @n
	 * In case that 2nd return value is false, User side has a role to recover this condition(fail to allocate the free node) by User side itself, e.g. backoff approach is one of recovery approach.
	 */
	std::tuple<bool, bool> insert(
		const value_type& cont_arg,   //!< [in]	a value to insert this list
		predicate_t&      pred        //!< [in]	A predicate function to specify the insertion position. const value_type& is passed as an argument
	)
	{
		typename list_type::find_predicate_t pred_common = [&pred]( const list_node_pointer a ) { return pred( a->ref_value() ); };

		list_node_pointer p_new_node = free_nd_.allocate<list_node_type>(
			ALLOW_TO_ALLOCATE,
			[this]( list_node_pointer p_chk_node ) {
				return !( this->base_list_.check_hazard_list( p_chk_node ) ||
			              this->hzrd_ptr_.check_ptr_in_hazard_list( p_chk_node ) );
				//				return false;
			} );

		if ( p_new_node == nullptr ) return std::tuple<bool, bool>( false, false );

		p_new_node->set_value( cont_arg );

		scoped_hazard_ref hzrd_ref_prev( hzrd_ptr_, (int)hazard_ptr_idx::FIND_ANS_PREV );
		scoped_hazard_ref hzrd_ref_curr( hzrd_ptr_, (int)hazard_ptr_idx::FIND_ANS_CURR );
		while ( true ) {
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
			auto [p_prev, p_curr] = base_list_.find_if( free_nd_, hzrd_ref_prev, hzrd_ref_curr, pred_common );
#else
			auto local_ret = base_list_.find_if( free_nd_, hzrd_ref_prev, hzrd_ref_curr, pred_common );
			auto p_prev    = std::get<0>( local_ret );
			auto p_curr    = std::get<1>( local_ret );
#endif

			if ( base_list_.is_end_node( p_curr ) ) {
				free_nd_.recycle( p_new_node );
				return std::tuple<bool, bool>( false, true );
			}

			if ( base_list_.insert( p_new_node, p_prev, p_curr ) ) {
				break;
			}
		}

		return std::tuple<bool, bool>( true, true );
	}

	/*!
	 * @breif	remove all of nodes that pred return true from this list
	 */
	int remove_all_if(
		predicate_t& pred   //!< [in]	A predicate function to specify the deletion target. const value_type& is passed as an argument
	)
	{
		int                                  ans         = 0;
		typename list_type::find_predicate_t pred_common = [&pred]( const list_node_pointer a ) { return pred( a->ref_value() ); };

		scoped_hazard_ref hzrd_ref_prev( hzrd_ptr_, (int)hazard_ptr_idx::FIND_ANS_PREV );
		scoped_hazard_ref hzrd_ref_curr( hzrd_ptr_, (int)hazard_ptr_idx::FIND_ANS_CURR );
		while ( true ) {
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
			auto [p_prev, p_curr] = base_list_.find_if( free_nd_, hzrd_ref_prev, hzrd_ref_curr, pred_common );
#else
			auto local_ret = base_list_.find_if( free_nd_, hzrd_ref_prev, hzrd_ref_curr, pred_common );
			auto p_prev    = std::get<0>( local_ret );
			auto p_curr    = std::get<1>( local_ret );
#endif

			if ( base_list_.is_end_node( p_curr ) ) break;

			if ( base_list_.remove( free_nd_, p_prev, p_curr ) ) {
				ans++;
			}
		}

		return ans;
	}

	/*!
	 * @breif	remove a first node that pred return true from this list
	 */
	bool remove_one_if(
		predicate_t& pred   //!< [in]	A predicate function to specify the deletion target. const value_type& is passed as an argument
	)
	{
		bool ans = false;

		typename list_type::find_predicate_t pred_common = [&pred]( const list_node_pointer a ) { return pred( a->ref_value() ); };

		scoped_hazard_ref hzrd_ref_prev( hzrd_ptr_, (int)hazard_ptr_idx::FIND_ANS_PREV );
		scoped_hazard_ref hzrd_ref_curr( hzrd_ptr_, (int)hazard_ptr_idx::FIND_ANS_CURR );
		while ( true ) {
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
			auto [p_prev, p_curr] = base_list_.find_if( free_nd_, hzrd_ref_prev, hzrd_ref_curr, pred_common );
#else
			auto local_ret = base_list_.find_if( free_nd_, hzrd_ref_prev, hzrd_ref_curr, pred_common );
			auto p_prev    = std::get<0>( local_ret );
			auto p_curr    = std::get<1>( local_ret );
#endif

			if ( base_list_.is_end_node( p_curr ) ) {
				break;
			}

			if ( base_list_.remove( free_nd_, p_prev, p_curr ) ) {
				ans = true;
				break;
			}
		}

		return ans;
	}

	/*!
	 * @breif	push a value to the front of this list
	 *
	 * cont_arg will copy to list.
	 *
	 * @return	success or fail to insert
	 * @retval	true	success to insert copy value of cont_arg to list
	 * @retval	false	fail to insert copy value of cont_arg, because of failure of the node allocation from free node strage.
	 *
	 * @note
	 * @li	In case that template parameter ALLOW_TO_ALLOCATE is false, this I/F will have a possibility to return false.
	 * @li	In case that return value is false, User side has a role to recover this condition(fail to allocate the free node) by User side itself, e.g. backoff approach is one of recovery approach.
	 */
	bool push_front(
		const value_type& cont_arg   //!< [in]	a value to insert this list
	)
	{
		typename list_type::find_predicate_t pred_common = []( const list_node_pointer a ) { return true; };

		return push_internal_common( cont_arg, pred_common );
	}

	/*!
	 * @breif	pop a value from the front of this list
	 *
	 * @return	1st return value. Success or fail to pop
	 * @retval	true	success to pop a value
	 * @retval	false	because of no data in the list, fail to pop
	 *
	 * @return	2nd return value. poped value, if 1st return value is true.
	 *
	 */
	std::tuple<bool, value_type> pop_front( void )
	{
		scoped_hazard_ref hzrd_ref_prev( hzrd_ptr_, (int)hazard_ptr_idx::FIND_ANS_PREV );
		scoped_hazard_ref hzrd_ref_curr( hzrd_ptr_, (int)hazard_ptr_idx::FIND_ANS_CURR );

		typename list_type::find_predicate_t pred_common = []( const list_node_pointer a ) { return true; };

		value_type                            ans_value {};
		internal::default_mover<T>            dm;
		typename list_type::remove_operator_t remove_op = [&ans_value, &dm]( value_type& a ) { dm(a, ans_value); return; };

		while ( true ) {
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
			auto [p_prev, p_curr] = base_list_.find_if( free_nd_, hzrd_ref_prev, hzrd_ref_curr, pred_common );
#else
			auto local_ret = base_list_.find_if( free_nd_, hzrd_ref_prev, hzrd_ref_curr, pred_common );
			auto p_prev    = std::get<0>( local_ret );
			auto p_curr    = std::get<1>( local_ret );
#endif

			if ( base_list_.is_end_node( p_curr ) ) {
				return std::tuple<bool, value_type>( false, ans_value );
			}

			if ( base_list_.remove( free_nd_, p_prev, p_curr, remove_op ) ) {
				break;
			}
		}

		return std::tuple<bool, value_type>( true, ans_value );
	}

	/*!
	 * @breif	append a value to the end of this list
	 *
	 * cont_arg will copy to list.
	 *
	 * @return	success or fail to insert
	 * @retval	true	success to insert copy value of cont_arg to list
	 * @retval	false	fail to insert copy value of cont_arg, because of failure of the node allocation from free node strage.
	 *
	 * @note
	 * @li	In case that template parameter ALLOW_TO_ALLOCATE is false, this I/F will have a possibility to return false.
	 * @li	In case that return value is false, User side has a role to recover this condition(fail to allocate the free node) by User side itself, e.g. backoff approach is one of recovery approach.
	 */
	bool push_back(
		const value_type& cont_arg   //!< [in]	a value to copy this list
	)
	{
		typename list_type::find_predicate_t pred_common = []( const list_node_pointer a ) { return false; };

		return push_internal_common( cont_arg, pred_common );
	}

	/*!
	 * @breif	pop a value from the end of this list
	 *
	 * @return	1st return value. Success or fail to pop
	 * @retval	true	success to pop a value
	 * @retval	false	because of no data in the list, fail to pop
	 *
	 * @return	2nd return value. poped value, if 1st return value is true.
	 *
	 */
	std::tuple<bool, value_type> pop_back( void )
	{
		scoped_hazard_ref hzrd_ref_prev( hzrd_ptr_, (int)hazard_ptr_idx::FIND_ANS_PREV );
		scoped_hazard_ref hzrd_ref_curr( hzrd_ptr_, (int)hazard_ptr_idx::FIND_ANS_CURR );
		scoped_hazard_ref hzrd_ref_last( hzrd_ptr_, (int)hazard_ptr_idx::POP_BACK_LAST );

		typename list_type::find_predicate_t pred_common = []( const list_node_pointer a ) { return false; };

		value_type                            ans_value {};
		internal::default_mover<value_type>   dm;
		typename list_type::remove_operator_t remove_op = [&ans_value, &dm]( value_type& a ) { dm(a, ans_value); return; };

		while ( true ) {
			list_node_pointer p_last;

			{
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
				auto [p_prev, p_curr] = base_list_.find_if( free_nd_, hzrd_ref_prev, hzrd_ref_curr, pred_common );
#else
				auto local_ret = base_list_.find_if( free_nd_, hzrd_ref_prev, hzrd_ref_curr, pred_common );
				auto p_prev    = std::get<0>( local_ret );
				auto p_curr    = std::get<1>( local_ret );
#endif
				if ( !base_list_.is_end_node( p_curr ) ) continue;
				if ( base_list_.is_head_node( p_prev ) ) {
					return std::tuple<bool, value_type>( false, ans_value );
				}
				p_last = p_prev;
				hzrd_ref_last.regist_ptr_as_hazard_ptr( p_last );
			}

			typename list_type::find_predicate_t pred_common2 = [p_last]( const list_node_pointer a ) { return ( a == p_last ); };
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
			auto [p_prev, p_curr] = base_list_.find_if( free_nd_, hzrd_ref_prev, hzrd_ref_curr, pred_common2 );
#else
			auto local_ret = base_list_.find_if( free_nd_, hzrd_ref_prev, hzrd_ref_curr, pred_common2 );
			auto p_prev    = std::get<0>( local_ret );
			auto p_curr    = std::get<1>( local_ret );
#endif
			if ( base_list_.is_end_node( p_curr ) ) continue;

			if ( base_list_.remove( free_nd_, p_prev, p_curr, remove_op ) ) {
				break;
			}
		}

		return std::tuple<bool, value_type>( true, ans_value );
	}

	/*!
	 * @breif	Applies the specified function to all elements.
	 *
	 * @pre
	 * The Function must meet the MoveConstructible requirement, but not the CopyConstructible requirement.
	 *
	 * @warning
	 * Due to the lock-free nature, it is possible that the element being processed may be deleted
	 * (just marked for deletion, the actual deletion may even be delayed until all threads are no
	 * longer referencing it) or modified.
	 * Also, access contention may occur during function execution. Therefore, when changing an element,
	 * it is necessary to perform access control such as exclusive control in Function f.
	 * Therefore, when a non-lock-free function such as exclusive control by mutex is used, the lock-free
	 * property of this class is limited.
	 */
	for_each_func_t for_each(
		for_each_func_t&& f   //!< [in]	A function f is passed value_type& as an argument
	)
	{
		return base_list_.for_each( std::move( f ) );
	}

	/*!
	 * @breif	number of the queued values in FIFO
	 *
	 * @warning
	 * This FIFO will be access by several thread concurrently. So, true number of this FIFO queue may be changed when caller uses the returned value.
	 */
	int get_size( void ) const
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
	lockfree_list& operator=( const lockfree_list& ) = delete;
	lockfree_list& operator=( lockfree_list&& ) = delete;

	using list_type            = internal::lockfree_list_base<T, DELETER>;
	using list_node_type       = typename list_type::node_type;
	using list_node_pointer    = typename list_type::node_pointer;
	using free_nd_storage_type = internal::free_nd_storage;
	using free_node_type       = typename free_nd_storage_type::node_type;
	using free_node_pointer    = typename free_nd_storage_type::node_pointer;

	static constexpr int hzrd_max_slot_ = 3;
	using hazard_ptr_storage            = hazard_ptr<list_node_type, hzrd_max_slot_>;
	using scoped_hazard_ref             = hazard_ptr_scoped_ref<list_node_type, hzrd_max_slot_>;

	enum class hazard_ptr_idx : int {
		FIND_ANS_PREV = 0,
		FIND_ANS_CURR = 1,
		POP_BACK_LAST = 2
	};

	/*!
	 * @breif	append a value to the end of this list
	 *
	 * cont_arg will copy to list.
	 *
	 * @return	success or fail to insert
	 * @retval	true	success to insert copy value of cont_arg to list
	 * @retval	false	fail to insert copy value of cont_arg, because of failure of the node allocation from free node strage.
	 *
	 * @note
	 * @li	In case that template parameter ALLOW_TO_ALLOCATE is false, this I/F will have a possibility to return false.
	 * @li	In case that return value is false, User side has a role to recover this condition(fail to allocate the free node) by User side itself, e.g. backoff approach is one of recovery approach.
	 */
	bool push_internal_common(
		const value_type&                     cont_arg,     //!< [in]	a value to copy this list
		typename list_type::find_predicate_t& pred_common   //!< [in]	const list_node_pointer& is provided.
	)
	{
		list_node_pointer p_new_node = free_nd_.allocate<list_node_type>(
			ALLOW_TO_ALLOCATE,
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
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
			auto [p_prev, p_curr] = base_list_.find_if( free_nd_, hzrd_ref_prev, hzrd_ref_curr, pred_common );
#else
			auto local_ret = base_list_.find_if( free_nd_, hzrd_ref_prev, hzrd_ref_curr, pred_common );
			auto p_prev    = std::get<0>( local_ret );
			auto p_curr    = std::get<1>( local_ret );
#endif

			if ( base_list_.insert( p_new_node, p_prev, p_curr ) ) {
				// success to insert into front of list.
				break;
			}
		}

		return true;
	}

	list_type            base_list_;
	free_nd_storage_type free_nd_;

	hazard_ptr_storage hzrd_ptr_;
};

}   // namespace concurrent
}   // namespace alpha

#endif /* INC_LF_LIST_HPP_ */
