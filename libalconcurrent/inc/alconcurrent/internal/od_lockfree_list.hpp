/**
 * @file od_lockfree_list.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-11-17
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCURRENT_INC_INTERNAL_OD_LOCKFREE_LIST_HPP_
#define ALCONCURRENT_INC_INTERNAL_OD_LOCKFREE_LIST_HPP_

#include "alconcurrent/internal/od_node_essence.hpp"
#include "alconcurrent/internal/alcc_optional.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

/**
 * @brief 短方向リストのリスト関連操作機能を提供するクラス、ただしノードのリソース管理は行わない。
 *
 * やむなく、このクラスのデストラクタでノードの解放が発生した場合、delete式で解放が行われる。
 *
 * @todo
 * swapをより安全にするには、head_を直接使うのではなく、head_とは別に、head_へのポインタを使ってアクセスする方式にする必要がある。
 * そうすることで、切り替え中に追加されたノードを開放することなく処理が完了できると思われる。
 * だたし、解放はされないが、意図した側に追加されるかはわからない。
 * これは、swap中に、*thisとsrcが混じった状況が極短時間であっても発生するため。
 * これを完全に回避するには、2点のメモリ間双方での同時にcompareし、両方が同時に真となった場合に、メモリ間での値のswapをatomicに実施するハード命令が必要(*)。
 * しかし、このような命令は存在しないため、上記のような不確定な振る舞いは不可避。
 *
 * (*) 下記のダブル・コンペアアンドスワップを参照のこと。
 * https://ja.wikipedia.org/wiki/%E3%82%B3%E3%83%B3%E3%83%9A%E3%82%A2%E3%83%BB%E3%82%A2%E3%83%B3%E3%83%89%E3%83%BB%E3%82%B9%E3%83%AF%E3%83%83%E3%83%97
 *
 */
class od_lockfree_list {
public:
	using node_type             = od_node_1bit_markable_link_by_hazard_handler;
	using node_pointer          = od_node_1bit_markable_link_by_hazard_handler*;
	using hazard_ptr_handler_t  = typename od_node_1bit_markable_link_by_hazard_handler::hazard_ptr_handler_t;
	using hazard_pointer        = typename od_node_1bit_markable_link_by_hazard_handler::hazard_pointer;
	using pointer_w_mark        = typename od_node_1bit_markable_link_by_hazard_handler::pointer_w_mark;
	using hazard_pointer_w_mark = typename od_node_1bit_markable_link_by_hazard_handler::hazard_pointer_w_mark;
	using find_predicate_t      = std::function<bool( const node_pointer )>;   //!< find_if関数で使用する述語関数を保持するfunction型
	using for_each_func_t       = std::function<void( node_pointer )>;         //!< for_each関数で各要素の処理を実行するための関数を保持するfunction型

	constexpr od_lockfree_list( void ) noexcept
	  : sentinel_()
	  , head_( &sentinel_ )
	{
	}

	od_lockfree_list( od_lockfree_list&& src ) noexcept;
	od_lockfree_list& operator=( od_lockfree_list&& src );

	/**
	 * @brief Destroy the od lockfree list object
	 *
	 * もしリストが空でない場合、残っているノードは、delete式で解放される。
	 * do_for_purged_node()で解放が必要な場合、clear()を事前に呼び出しておくこと。
	 * また、同様の理由で、派生クラスのデストラクタでclear()を呼び出しておくこと。
	 */
	virtual ~od_lockfree_list();

	od_lockfree_list( const od_lockfree_list& )            = delete;
	od_lockfree_list& operator=( const od_lockfree_list& ) = delete;

	/*!
	 * @brief	prevの後に挿入する
	 *
	 * 挿入と挿入が競合した場合、currを取得しなおしたうえでprevの後にノードを挿入する
	 *
	 * @retval	true	挿入に成功
	 * @retval	false	挿入に失敗。削除処理と競合した場合に発生する。この場合、挿入位置を最初から検索しなおすこと。
	 *
	 * @pre
	 * p_push_nodeは、ハザードポインタに登録されていないこと。
	 */
	bool insert_to_next_of_prev(
		node_pointer const           p_push_node,   //!< [in]	新たに挿入するノード
		const hazard_pointer_w_mark& prev,          //!< [in]	find_ifの戻り値の第１パラメータ
		hazard_pointer_w_mark&       curr           //!< [in]	find_ifの戻り値の第２パラメータ
	);

	/*!
	 * @brief	currの後に挿入する
	 *
	 * @retval	true	挿入に成功
	 * @retval	false	挿入に失敗。挿入と挿入が競合した場合や、削除処理と競合した場合に発生する。この場合、挿入位置を最初から検索しなおすこと。
	 *
	 * @pre
	 * p_push_nodeは、ハザードポインタに登録されていないこと。
	 */
	bool insert_to_before_of_curr(
		node_pointer const           p_push_node,   //!< [in]	新たに挿入するノード
		const hazard_pointer_w_mark& prev,          //!< [in]	find_ifの戻り値の第１パラメータ
		hazard_pointer_w_mark&       curr           //!< [in]	find_ifの戻り値の第２パラメータ
	);

	/*!
	 * @brief	currのノードを削除する
	 *
	 * @retval	true	少なくとも削除マークの付与に成功。切り離しまで成功した場合、切り離したノードに対しdo_for_purged_node()が呼び出される。
	 * @retval	false	削除マークの付与に失敗。すでに誰かが削除マークを付与していた場合、currがsentinel_を指している場合。
	 */
	bool remove(
		const hazard_pointer_w_mark& prev,   //!< [in]	find_ifの戻り値の第１パラメータ
		hazard_pointer_w_mark&&      curr    //!< [in]	find_ifの戻り値の第２パラメータ。削除するノード
	);

	/*!
	 * @brief	currのノードに削除マークを付与する。
	 *
	 * 削除マークを付けるだけで、ノードの切り離しは行わない。ほかの処理で切り離されることとする。
	 *
	 * @retval	true	削除マークの付与に成功。
	 * @retval	false	削除マークの付与に失敗。すでに誰かが削除マークを付与していた場合、currがsentinel_を指している場合。
	 */
	bool remove_mark(
		hazard_pointer_w_mark& curr   //!< [in]	削除マークを付与するノード
	);

	/*!
	 * @brief	先頭ノードに削除マークを付与する。
	 *
	 * @return 1st element: 削除処理における削除マーク付与の成否
	 * @retval	true	少なくとも削除マークの付与に成功。
	 * @retval	false	削除マークの付与に失敗。すでに誰かが削除マークを付与していた場合、リストが空でノードが存在しない場合。
	 */
	alcc_optional<hazard_pointer_w_mark> remove_mark_head( void );

	/*!
	 * @brief	最終ノードに削除マークを付与する。
	 *
	 * @return 1st element: 削除処理における削除マーク付与の成否
	 * @retval	true	少なくとも削除マークの付与に成功。
	 * @retval	false	削除マークの付与に失敗。すでに誰かが削除マークを付与していた場合、リストが空でノードが存在しない場合。
	 */
	alcc_optional<hazard_pointer_w_mark> remove_mark_tail( void );

	/**
	 * @brief リストを空にする
	 *
	 * remove()とは異なり、すべてのノードの切り離しまで行う。
	 */
	void clear( void );

	/**
	 * @brief 入れ替える
	 *
	 * スレッドセーフではない。
	 *
	 * 入れ替え中に追加されたノードは解放されてしまう可能性があるため。
	 *
	 * @param src
	 */
	void swap( od_lockfree_list& src );

	/*!
	 * @brief	条件に合致する最初のノードとその直前のノードを見つける。
	 *
	 * 検索で、predが真を返した場合
	 * @retval	1st-value	直前のノードへのポインタ
	 * @retval	2nd-value	最初にpredが真を返したノードへのポインタ
	 *
	 * 検索で、predが真を返さなかった場合
	 * @retval	1st-value	番兵ノードより１つ前のノードへのポインタ
	 * @retval	2nd-value	番兵ノードへのポインタ
	 *
	 * predでtrueを返す処理の最中に、他のスレッドで削除処理によって削除マークがつく可能性がある。
	 * predの処理中に削除マークが付けられた場合、predでtrueを返したとしても検索条件に合致したとはみなされない。
	 *
	 * また、trueを返した後、呼び出し元に戻るまでの間に削除マークがつく可能性がある。
	 * よって、有効なノードへのhazardポインタが返ってきたとしても、そのノードが削除されないわけではない。
	 * ただし、この場合はハザードポインタとして登録がされているため、ノードの解放までは行われないため継続してアクセスは可能。
	 */
	std::pair<hazard_pointer_w_mark, hazard_pointer_w_mark> find_if(
		find_predicate_t pred   //!< [in]	引数には、const node_pointerが渡される
	);

	std::pair<hazard_pointer_w_mark, hazard_pointer_w_mark> find_head( void );
	std::pair<hazard_pointer_w_mark, hazard_pointer_w_mark> find_tail( void );

	/*!
	 * @brief	Applies the specified function to all elements.
	 *
	 * 対象を処理している最中にも削除マークがつく可能性がある。
	 * また、対象の処理をしている間の排他制御は行わないため、処理関数内で排他制御を行うこと。
	 */
	for_each_func_t for_each(
		const for_each_func_t& f   //!< [in]	A function f is passed value_type& as an argument
	);
	for_each_func_t for_each(
		for_each_func_t&& f   //!< [in]	A function f is passed value_type& as an argument
	);

	/**
	 * @brief 有効なノード数を数え上げる
	 *
	 * この処理は、スレッドセーフだが、ほかのスレッドで削除/挿入されていた場合、数え上げがうまくいかない。
	 *
	 * @return size_t
	 */
	size_t size( void ) const noexcept;

	/*!
	 * @brief	インスタンス内で保持している終端ノード（番兵ノード）かどうかを調べる。
	 *
	 * @retval	true	p_chk_ptr is same to termination node.
	 * @retval	false	p_chk_ptr is not same to termination node.
	 */
	bool is_end_node( const node_pointer p_chk_ptr ) const
	{
		return ( p_chk_ptr == &sentinel_ );
	}
	bool is_end_node( const pointer_w_mark& p_chk_ptr ) const
	{
		return is_end_node( p_chk_ptr.p_ );
	}
	bool is_end_node( const hazard_pointer_w_mark& hp_chk_ptr ) const
	{
		return is_end_node( hp_chk_ptr.hp_.get() );
	}
	bool is_end_node( const std::pair<hazard_pointer_w_mark, hazard_pointer_w_mark>& pair_hp_chk_ptr ) const
	{
		return is_end_node( pair_hp_chk_ptr.second.hp_.get() );
	}

protected:
	/*!
	 * @brief	インスタンス内で保持しているヘッドノードかどうかを調べる。
	 *
	 * @retval	true	p_chk_ptr is same to termination node.
	 * @retval	false	p_chk_ptr is not same to termination node.
	 */
	bool is_head_node( const node_pointer p_chk_ptr ) const
	{
		return ( p_chk_ptr == &head_ );
	}
	bool is_head_node( const pointer_w_mark& p_chk_ptr ) const
	{
		return is_head_node( p_chk_ptr.p_ );
	}
	bool is_head_node( const hazard_pointer_w_mark& hp_chk_ptr ) const
	{
		return is_head_node( hp_chk_ptr.hp_.get() );
	}
	bool is_head_node( const std::pair<hazard_pointer_w_mark, hazard_pointer_w_mark>& pair_hp_chk_ptr ) const
	{
		return is_head_node( pair_hp_chk_ptr.second.hp_.get() );
	}

	/**
	 * @brief 切り離し処理が成功した場合に、切り離したノードを引き渡すために呼び出されるコールバック関数
	 *
	 * なお、このコールバック関数は、自身のリストが空ではない状態でデストラクタが呼び出されたとしても、
	 * od_lockfree_listのデストラクタから呼び出されることはない。
	 *
	 * @param p_nd 切り離したノードへのポインタ
	 */
	virtual void do_for_purged_node( node_pointer p_nd );

private:
	std::pair<hazard_pointer_w_mark, hazard_pointer_w_mark> find_if_impl(
		find_predicate_t pred,             //!< [in]	引数には、const node_pointerが渡される
		node_pointer     p_sentinel_node   //!< [in] 終端として判定するノード
	);

	void call_base_do_for_purged_node( node_pointer p_nd );

	static std::pair<hazard_pointer_w_mark, hazard_pointer_w_mark> find_sentinel_without_purge(
		node_type&   dummy_head_,      //!< [in] headノード
		node_pointer p_head_node,      //!< [in] 開始ノード
		node_pointer p_sentinel_node   //!< [in] 終端として判定するノード
	);

	/**
	 * @brief src内の全ノードまとめて切り離す。
	 *
	 * @warning
	 * 最後のノードはsrcのsentinel_につながったままの状態
	 *
	 * @param src
	 */
	static hazard_pointer_w_mark purge_all_from( od_lockfree_list& src );

	/**
	 * @brief sentinelノードへの接続を切り替える
	 */
	void exchange_sentinel_connection( node_pointer p_sentinel_of_from, node_pointer p_sentinel_of_to );

	/**
	 * @brief clear()の実装
	 */
	static void clear_impl( node_type& head_arg, node_type& sentinel_arg, od_lockfree_list* p_this_obj_to_purge, void ( od_lockfree_list::*p_purge_func )( node_pointer ) );

	static bool try_to_purge(
		const hazard_ptr_handler_t::hazard_pointer_w_mark& prev_hp_w_m,   //!< [in] 削除するノードよりひとつ前のノードへのハザードポインタ
		hazard_ptr_handler_t::hazard_pointer_w_mark&       curr_hp_w_m,   //!< [in/out]	削除するノードへのハザードポインタ。戻り値がtrueの場合、呼び出し前の状態を維持している。falseの場合、値が変わっている。
		const hazard_ptr_handler_t::hazard_pointer_w_mark& next_hp_w_m    //!< [in] 削除するノードよりひとつ後のノードへのハザードポインタ
		) noexcept;

	node_type sentinel_;   // head_、sentinel_をヒープ領域に確保する構成であれば、moveが速くなる。が、今はこの方式とする。
	node_type head_;
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
