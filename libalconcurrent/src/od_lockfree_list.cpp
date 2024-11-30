/**
 * @file od_lockfree_list.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-11-17
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "alconcurrent/internal/od_lockfree_list.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

od_lockfree_list::od_lockfree_list( od_lockfree_list&& src ) noexcept
  : sentinel_()
  , head_( &sentinel_ )
{
	hazard_pointer_w_mark hp_w_m_purged_head = purge_all_from( src );

	while ( true ) {
		auto ret = find_sentinel_without_purge( head_, hp_w_m_purged_head.hp_.get(), &( src.sentinel_ ) );
		if ( ret.second.hp_ == nullptr ) {
			// 本来は発生しないはずだが、一応保護コードとして用意する。
			head_.hazard_handler_of_next().store( &sentinel_, false );
			break;
		}

		if ( ret.first.hp_->hazard_handler_of_next().compare_exchange_strong_to_verify_exchange2( ret.second, &sentinel_ ) ) {
			break;
		}
	}
}

od_lockfree_list& od_lockfree_list::operator=( od_lockfree_list&& src )
{
	if ( this == &src ) return *this;

	// srcからリストを切り離す。
	hazard_pointer_w_mark hp_w_m_purged_head = purge_all_from( src );

	// 切り離したリストの終端をthisのsentinelへ切り替える。
	node_type dummy_head;
	while ( true ) {
		auto ret = find_sentinel_without_purge( dummy_head, hp_w_m_purged_head.hp_.get(), &( src.sentinel_ ) );
		if ( ret.second.hp_ == nullptr ) {
			// 本来は発生しないはずだが、一応保護コードとして用意する。
			// 切り離したheadノードがほかに移動されていたと考えられるので、srcが空に相当する状態だっとみなす。
			break;
		}

		if ( ret.first.hp_->hazard_handler_of_next().compare_exchange_strong_to_verify_exchange2( ret.second, &sentinel_ ) ) {
			break;
		}
	}

	// head_が指す先を、srcから切り離してきたリストの先頭にCASで切り替える。
	hazard_pointer_w_mark hp_w_m_this_head = head_.hazard_handler_of_next().get_to_verify_exchange();
	while ( true ) {
		if ( !head_.hazard_handler_of_next().verify_exchange( hp_w_m_this_head ) ) {
			continue;
		}
		if ( head_.hazard_handler_of_next().compare_exchange_strong_to_verify_exchange2( hp_w_m_this_head, dummy_head.hazard_handler_of_next().load().p_ ) ) {
			break;
		}
	}
	// ここまでで、thisへの移動が完了。移動前にthisが持っていたリストの先頭は、hp_w_m_this_head に保持されている。

	dummy_head.hazard_handler_of_next().store( hp_w_m_this_head.hp_.get(), false );
	clear_impl( dummy_head, sentinel_, this, &od_lockfree_list::do_for_purged_node );

	return *this;
}

od_lockfree_list::~od_lockfree_list()
{
	clear_impl( head_, sentinel_, this, &od_lockfree_list::call_base_do_for_purged_node );
	return;
}

std::pair<od_lockfree_list::hazard_pointer_w_mark, od_lockfree_list::hazard_pointer_w_mark> od_lockfree_list::find_if_impl(
	od_lockfree_list::find_predicate_t pred,             //!< [in]	引数には、const node_pointerが渡される
	od_lockfree_list::node_pointer     p_sentinel_node   //!< [in] 終端として判定するノード
)
{
	hazard_pointer_w_mark hp_prev_node_w_m;
	hazard_pointer_w_mark hp_curr_node_w_m;
	hazard_pointer_w_mark hp_next_node_w_m;
	while ( true ) {   // 先頭からやり直すためのループ
		hp_prev_node_w_m.mark_ = false;
		hp_prev_node_w_m.hp_.store( &head_ );
		head_.hazard_handler_of_next().reuse_to_verify_exchange( hp_curr_node_w_m );
		if ( !head_.hazard_handler_of_next().verify_exchange( hp_curr_node_w_m ) ) {
			continue;
		}
		while ( true ) {   // リストをたどるループ
			if ( hp_curr_node_w_m.hp_ == p_sentinel_node ) {
				// listが空なので、処理終了。
				return std::pair<od_lockfree_list::hazard_pointer_w_mark, od_lockfree_list::hazard_pointer_w_mark> { std::move( hp_prev_node_w_m ), std::move( hp_curr_node_w_m ) };
			}
			hp_curr_node_w_m.hp_->hazard_handler_of_next().reuse_to_verify_exchange( hp_next_node_w_m );
			if ( !hp_curr_node_w_m.hp_->hazard_handler_of_next().verify_exchange( hp_next_node_w_m ) ) {
				continue;
			}

			if ( hp_next_node_w_m.hp_ == nullptr ) {
				// 繋ぎ変え処理と衝突し、currが他のインスタンスのsentinelに到達した。また、検索処理自体は終了したため、処理完了とする。
				// ただし、戻り値のハザードポインタは、currは使用しない。
				return std::pair<od_lockfree_list::hazard_pointer_w_mark, od_lockfree_list::hazard_pointer_w_mark> { std::move( hp_prev_node_w_m ), od_lockfree_list::hazard_pointer_w_mark( p_sentinel_node ) };
			}
			// currノードの削除マークを確認する。なお、currノードの削除マークは、hp_next_node_w_mに入っている。
			if ( hp_next_node_w_m.mark_ ) {
				// 切り離し処理をしたうえで、リストの最初からやり直す。
				bool ret = try_to_purge( hp_prev_node_w_m, hp_curr_node_w_m, hp_next_node_w_m );
				if ( ret ) {
					// 切り離し処理に成功したので、ノードをリサイクルに回す。
					do_for_purged_node( hp_curr_node_w_m.hp_.reset() );
				}
				break;   // リストの最初に戻る
			}

			if ( pred( hp_curr_node_w_m.hp_.get() ) ) {
				// 検査中に削除マークがつく可能性があるため、検査が真であることに加えて、検査後も削除マークがついていなければ、検索成功とする。
				if ( !hp_curr_node_w_m.hp_->is_marked() ) {
					return std::pair<od_lockfree_list::hazard_pointer_w_mark, od_lockfree_list::hazard_pointer_w_mark> { std::move( hp_prev_node_w_m ), std::move( hp_curr_node_w_m ) };
				}
			}

			// currが該当ではないので、次のノードに進める。
			hp_prev_node_w_m.swap( hp_curr_node_w_m );
			hp_curr_node_w_m.swap( hp_next_node_w_m );
		}
	}
}

std::pair<od_lockfree_list::hazard_pointer_w_mark, od_lockfree_list::hazard_pointer_w_mark> od_lockfree_list::find_if(
	od_lockfree_list::find_predicate_t pred   //!< [in]	引数には、const node_pointerが渡される
)
{
	return find_if_impl( pred, &sentinel_ );
}

std::pair<od_lockfree_list::hazard_pointer_w_mark, od_lockfree_list::hazard_pointer_w_mark> od_lockfree_list::find_tail( void )
{
	return find_if_impl(
		[this]( node_pointer p_nd ) -> bool {
			pointer_w_mark tmp = p_nd->hazard_handler_of_next().load();
			if ( tmp.mark_ ) return false;
			if ( tmp.p_ == &sentinel_ ) return true;
			return false;
		},
		&sentinel_ );
}

od_lockfree_list::for_each_func_t od_lockfree_list::for_each(
	for_each_func_t&& f   //!< [in]	A function f is passed value_type& as an argument
)
{
	hazard_pointer_w_mark hp_prev_node_w_m;
	hazard_pointer_w_mark hp_curr_node_w_m;
	hazard_pointer_w_mark hp_next_node_w_m;

	hp_prev_node_w_m.mark_ = false;
	hp_prev_node_w_m.hp_.store( &head_ );
	do {
		head_.hazard_handler_of_next().reuse_to_verify_exchange( hp_curr_node_w_m );
	} while ( !head_.hazard_handler_of_next().verify_exchange( hp_curr_node_w_m ) );
	while ( true ) {   // リストをたどるループ
		if ( hp_curr_node_w_m.hp_ == &sentinel_ ) {
			// listが空なので、処理終了。
			break;
		}
		hp_curr_node_w_m.hp_->hazard_handler_of_next().reuse_to_verify_exchange( hp_next_node_w_m );
		if ( !hp_curr_node_w_m.hp_->hazard_handler_of_next().verify_exchange( hp_next_node_w_m ) ) {
			continue;
		}

		if ( hp_next_node_w_m.hp_ == nullptr ) {
			// 繋ぎ変え処理と衝突し、currが他のインスタンスのsentinelに到達した。また、検索処理自体は終了したため、処理完了とする。
			// ただし、戻り値のハザードポインタは、currは使用しない。
			break;
		}

		// currノードの削除マークを確認する。なお、currノードの削除マークは、hp_next_node_w_mに入っている。
		if ( !hp_next_node_w_m.mark_ ) {
			f( hp_curr_node_w_m.hp_.get() );
		}

		// currが該当ではないので、次のノードに進める。
		hp_prev_node_w_m.swap( hp_curr_node_w_m );
		hp_curr_node_w_m.swap( hp_next_node_w_m );
	}

	return f;
}

od_lockfree_list::for_each_func_t od_lockfree_list::for_each(
	const for_each_func_t& f   //!< [in]	A function f is passed value_type& as an argument
)
{
	for_each_func_t copyed_func = f;
	return for_each( std::move( copyed_func ) );
}

bool od_lockfree_list::insert_to_next_of_prev(
	node_pointer const           p_push_node,   //!< [in]	新たに挿入するノード
	const hazard_pointer_w_mark& prev,          //!< [in]	find_ifの戻り値の第１パラメータ
	hazard_pointer_w_mark&       curr           //!< [in/out]	find_ifの戻り値の第２パラメータ
)
{
	p_push_node->hazard_handler_of_next().store( curr.hp_.get(), false );

	while ( !prev.hp_->hazard_handler_of_next().compare_exchange_strong_to_verify_exchange2( curr, p_push_node ) ) {
		if ( curr.mark_ ) {
			// prevに削除マークがついたので、挿入失敗
			return false;
		}
		p_push_node->hazard_handler_of_next().store( curr.hp_.get(), false );
	}

	return true;
}

bool od_lockfree_list::insert_to_before_of_curr(
	node_pointer const           p_push_node,   //!< [in]	新たに挿入するノード
	const hazard_pointer_w_mark& prev,          //!< [in]	find_ifの戻り値の第１パラメータ
	hazard_pointer_w_mark&       curr           //!< [in/out]	find_ifの戻り値の第２パラメータ
)
{
	p_push_node->hazard_handler_of_next().store( curr.hp_.get(), false );

	bool ans = prev.hp_->hazard_handler_of_next().compare_exchange_strong_to_verify_exchange2( curr, p_push_node );

	return ans;
}

bool od_lockfree_list::remove(
	const hazard_pointer_w_mark& prev,   //!< [in]	find_ifの戻り値の第１パラメータ
	hazard_pointer_w_mark&&      curr    //!< [in]	find_ifの戻り値の第２パラメータ。削除するノード
)
{
	bool marking_ret = remove_mark( curr );

	// ここを通過した時点で、削除ノードへの変更は完了。
	// 以降の切り出し処理は失敗しても気にしない。失敗しても、find_ifでそのうち切り離されるため。

	if ( marking_ret ) {
		pointer_w_mark p_next_w_m = curr.hp_->hazard_handler_of_next().load();
		if ( prev.hp_->hazard_handler_of_next().compare_exchange_strong_to_verify_exchange2( curr, p_next_w_m.p_ ) ) {
			do_for_purged_node( curr.hp_.reset() );
		}
	}

	return marking_ret;
}

bool od_lockfree_list::remove_mark(
	hazard_pointer_w_mark& curr   //!< [in]	削除マークを付与するノード
)
{
	if ( is_end_node( curr ) ) {
		return false;
	}
	return curr.hp_->try_set_mark();
}

std::tuple<bool, od_lockfree_list::hazard_pointer_w_mark> od_lockfree_list::remove_mark_tail( void )
{
	bool                                                                                        remove_ret = true;
	std::pair<od_lockfree_list::hazard_pointer_w_mark, od_lockfree_list::hazard_pointer_w_mark> tail_hp_pair;
	pointer_w_mark                                                                              to_sentinel;
	do {
		tail_hp_pair = find_tail();
		if ( is_end_node( tail_hp_pair ) ) {
			remove_ret = false;
			break;
		}
		to_sentinel.p_ = &sentinel_;
	} while ( !tail_hp_pair.second.hp_->try_set_mark( to_sentinel ) );

	return std::tuple<bool, od_lockfree_list::hazard_pointer_w_mark> { remove_ret, std::move( tail_hp_pair.second ) };
}

void od_lockfree_list::clear_impl( node_type& head_arg, node_type& sentinel_arg, od_lockfree_list* p_this_obj_to_purge, void ( od_lockfree_list::*p_purge_func )( node_pointer ) )
{
	// move assignmentをスレッドセーフとするために、真面目に解放処理を行う。
	hazard_pointer_w_mark hp_curr_node_w_m = head_arg.hazard_handler_of_next().get_to_verify_exchange();
	while ( true ) {   // リストをたどるループ
		if ( hp_curr_node_w_m.hp_ == &sentinel_arg ) {
			// listが空なので、処理終了。
			break;
		}
		if ( !head_arg.hazard_handler_of_next().verify_exchange( hp_curr_node_w_m ) ) {
			continue;
		}

		pointer_w_mark p_next_w_m = hp_curr_node_w_m.hp_->hazard_handler_of_next().load();
		if ( p_next_w_m.p_ == nullptr ) {
			// 繋ぎ変え途中の処理と衝突したため、他のインスタンスのsentinelノードに到達した。
			// sentinelノードに対しては何らかの処理を行ってはならないため、これ以上処理を進行させてはならない。
			// また、clear処理自身は最後まで完了したので、処理を終了する。
			break;
		}

		hp_curr_node_w_m.hp_->set_mark();

		if ( head_arg.hazard_handler_of_next().compare_exchange_strong_to_verify_exchange2( hp_curr_node_w_m, p_next_w_m.p_ ) ) {
			( p_this_obj_to_purge->*p_purge_func )( hp_curr_node_w_m.hp_.reset() );
		}
	}

	return;
}

void od_lockfree_list::clear( void )
{
	clear_impl( head_, sentinel_, this, &od_lockfree_list::do_for_purged_node );
	return;
}

void od_lockfree_list::swap( od_lockfree_list& src )
{

	if ( this == &src ) return;

	od_lockfree_list tmp = std::move( *this );
	*this                = std::move( src );   // ここで、*this側に追加されたノードを開放する可能性がある。
	src                  = std::move( tmp );   // ここで、src側に追加されたノードを開放する可能性がある。
}

std::pair<od_lockfree_list::hazard_pointer_w_mark, od_lockfree_list::hazard_pointer_w_mark> od_lockfree_list::find_sentinel_without_purge(
	node_type&   dummy_head_,      //!< [in] headノード
	node_pointer p_head_node,      //!< [in] 開始ノード
	node_pointer p_sentinel_node   //!< [in] 終端として判定するノード
)
{
	dummy_head_.hazard_handler_of_next().store( p_head_node, false );
	hazard_pointer_w_mark hp_prev_node_w_m;
	hazard_pointer_w_mark hp_curr_node_w_m;
	hazard_pointer_w_mark hp_next_node_w_m;
	while ( true ) {   // 先頭からやり直すためのループ
		hp_prev_node_w_m.mark_ = false;
		hp_prev_node_w_m.hp_.store( &dummy_head_ );
		dummy_head_.hazard_handler_of_next().reuse_to_verify_exchange( hp_curr_node_w_m );
		if ( !dummy_head_.hazard_handler_of_next().verify_exchange( hp_curr_node_w_m ) ) {
			continue;
		}
		while ( true ) {   // リストをたどるループ
			if ( hp_curr_node_w_m.hp_ == p_sentinel_node ) {
				// sentinelまで到達したので、処理終了。
				return std::pair<od_lockfree_list::hazard_pointer_w_mark, od_lockfree_list::hazard_pointer_w_mark> { std::move( hp_prev_node_w_m ), std::move( hp_curr_node_w_m ) };
			}
			hp_curr_node_w_m.hp_->hazard_handler_of_next().reuse_to_verify_exchange( hp_next_node_w_m );
			if ( !hp_curr_node_w_m.hp_->hazard_handler_of_next().verify_exchange( hp_next_node_w_m ) ) {
				continue;
			}

			if ( hp_next_node_w_m.hp_ == nullptr ) {
				// 繋ぎ変え処理と衝突し、currが他のインスタンスのsentinelに到達した。なお、検索処理自体は終了したため、処理完了。
				// すでにsentinelとのつなぎが期待値と変わっているので、検索は失敗とし、nullptrを返す。
				return std::pair<od_lockfree_list::hazard_pointer_w_mark, od_lockfree_list::hazard_pointer_w_mark> { std::move( hp_prev_node_w_m ), od_lockfree_list::hazard_pointer_w_mark( nullptr ) };
			}

			// currが該当ではないので、次のノードに進める。
			hp_prev_node_w_m.swap( hp_curr_node_w_m );
			hp_curr_node_w_m.swap( hp_next_node_w_m );
		}
	}
}

od_lockfree_list::hazard_pointer_w_mark od_lockfree_list::purge_all_from( od_lockfree_list& src )
{
	// src側の先頭ノードを確保し、CASで一気にsrc側を空にする。
	hazard_pointer_w_mark hp_src_next_of_head_w_m;
	while ( true ) {   // 先頭からやり直すためのループ
		src.head_.hazard_handler_of_next().reuse_to_verify_exchange( hp_src_next_of_head_w_m );
		if ( !src.head_.hazard_handler_of_next().verify_exchange( hp_src_next_of_head_w_m ) ) {
			continue;
		}
		if ( src.head_.hazard_handler_of_next().compare_exchange_strong_to_verify_exchange2( hp_src_next_of_head_w_m, &( src.sentinel_ ) ) ) {
			break;
		}
	}

	return hp_src_next_of_head_w_m;
}

void od_lockfree_list::exchange_sentinel_connection( node_pointer p_sentinel_of_from, node_pointer p_sentinel_of_to )
{
	while ( true ) {
		// callback呼び出しのため、クラスインスタンスに紐づけて処理する必要あり。
		auto ret = find_if_impl( []( const node_pointer ) { return false; }, p_sentinel_of_from );
		if ( ret.first.hp_->hazard_handler_of_next().compare_exchange_strong_to_verify_exchange2( ret.second, p_sentinel_of_to ) ) {
			break;
		}
	}
}

size_t od_lockfree_list::size( void ) const noexcept
{
	// Caution: この処理は、スレッドセーフではない。src側がほかのスレッドで削除/挿入されていた場合、数え上げがうまくいかない。
	size_t                               count   = 0;
	hazard_ptr_handler_t::pointer_w_mark p_m_cur = head_.hazard_handler_of_next().load();
	while ( ( p_m_cur.p_ != &sentinel_ ) && ( p_m_cur.p_ != nullptr ) ) {   // 繋ぎ変え処理との競合を考慮し、nullptrもチェックする。
		hazard_ptr_handler_t::pointer_w_mark p_m_nxt = p_m_cur.p_->hazard_handler_of_next().load();
		if ( !p_m_nxt.mark_ ) {
			count++;
		}
		p_m_cur = p_m_nxt;
	}

	return count;
}

bool od_lockfree_list::try_to_purge(
	const hazard_ptr_handler_t::hazard_pointer_w_mark& prev_hp_w_m,   //!< [in] 削除するノードよりひとつ前のノードへのハザードポインタ
	hazard_ptr_handler_t::hazard_pointer_w_mark&       curr_hp_w_m,   //!< [in/out]	削除するノードへのハザードポインタ。戻り値がtrueの場合、呼び出し前の状態を維持している。falseの場合、値が変わっている。
	const hazard_ptr_handler_t::hazard_pointer_w_mark& next_hp_w_m    //!< [in] 削除するノードよりひとつ後のノードへのハザードポインタ
	) noexcept
{
	return prev_hp_w_m.hp_->hazard_handler_of_next().compare_exchange_strong_to_verify_exchange2( curr_hp_w_m, next_hp_w_m.hp_.get() );
}

void od_lockfree_list::do_for_purged_node( node_pointer p_nd )
{
	// 以下のコードは一応メモリーリークを避けるための処理。
	// ただし、deleteで破棄してよいかは状況次第。
	// 本来は、オーバーライドしてリサイクルしてもらうのが期待値。
	// TODO: メモリリークのためとはいえ、せめてハザードポインタかどうかのチェックをした方が良いか？
	// TODO: allocatorを呼び出せるようにしても良いかもしれない。
	// TODO: 今は、デストラクタからしか呼ばれない。。。そうであれば、そもそも用意する必要ある？
	delete p_nd;
}

void od_lockfree_list::call_base_do_for_purged_node( node_pointer p_nd )
{
	od_lockfree_list::do_for_purged_node( p_nd );
	return;
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
