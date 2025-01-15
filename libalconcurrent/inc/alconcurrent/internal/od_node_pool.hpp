/**
 * @file od_node_pool.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-06-30
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCURRENT_INC_INTERNAL_OD_NODE_POOL_HPP_
#define ALCONCURRENT_INC_INTERNAL_OD_NODE_POOL_HPP_

#ifdef ALCONCURRENT_CONF_ENABLE_COUNTERMEASURE_GCC_BUG_66944
#include <list>
#include <memory>
#endif

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/hazard_ptr.hpp"
#include "alconcurrent/internal/od_simple_list.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

#ifdef ALCONCURRENT_CONF_ENABLE_COUNTERMEASURE_GCC_BUG_66944
/**
 * @brief counter measure code for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66944
 *
 */
struct countermeasure_gcc_bug_deletable_obj_abst {
	virtual ~countermeasure_gcc_bug_deletable_obj_abst() = default;
};
#endif

struct is_member_function_callable_impl {
	template <typename T>
	static auto check_reset_value( T* ) -> decltype( std::declval<T>().reset_value(), std::true_type() );
	template <typename T>
	static auto check_reset_value( ... ) -> std::false_type;
};

template <typename T>
struct is_member_function_callable_reset_value : decltype( is_member_function_callable_impl::check_reset_value<T>( nullptr ) ) {};

template <typename NODE_T>
class od_node_pool {
	static_assert( std::is_base_of<od_node_simple_link, NODE_T>::value, "NODE_T should be a derived class of od_node_simple_link." );
	static_assert(
		std::is_base_of<od_node_link_by_hazard_handler, NODE_T>::value ||
			std::is_base_of<od_node_1bit_markable_link_by_hazard_handler, NODE_T>::value,
		"NODE_T should be a derived class of od_node_simple_link." );

public:
	using node_type    = NODE_T;
	using node_pointer = NODE_T*;

	static void push( node_pointer p_nd )
	{
		if ( p_nd == nullptr ) return;

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		++node_count_total_;
#endif

		if ( hazard_ptr_mgr::CheckPtrIsHazardPtr( p_nd->get_pointer_of_hazard_check() ) ) {
			get_tl_odn_list_still_in_hazard().push_back( p_nd );
			return;
		}
		reset_value_of_node( p_nd );

		tl_od_node_list& tl_odn_list_no_in_hazard = get_tl_odn_list_no_in_hazard();

		// スレッドローカルな変数に格納されているノードが少なかったら、スレッドローカルな変数に保存する。
		// 「少ない」の判定を簡単にするために、判定が簡単な1以下で少ないと判定している。
		// こうすることで、スレッドローカルなノードの再利用の機会が増え、新たなノードの割り当てによるメモリ消費を抑制できる。
		if ( !tl_odn_list_no_in_hazard.is_more_than_one() ) {
			tl_odn_list_no_in_hazard.push_back( p_nd );
			return;
		}

		auto lk = g_odn_list_no_in_hazard_.try_lock();
		if ( lk.owns_lock() ) {
			lk.ref().push_back( p_nd );   // ロック付きリストのロックを取得できたので、ロック付きリストに登録
			return;
		}

		tl_odn_list_no_in_hazard.push_back( p_nd );   // スレッドローカルな変数に保存する。
		return;
	}

	static node_pointer pop( void )
	{
		tl_od_node_list& tl_odn_list_no_in_hazard = get_tl_odn_list_no_in_hazard();

		typename tl_od_node_list::node_pointer p_ans_baseclass_node = tl_odn_list_no_in_hazard.pop_front();
		if ( p_ans_baseclass_node != nullptr ) {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			--node_count_total_;
#endif
			return static_cast<node_pointer>( p_ans_baseclass_node );   // このクラスが保持するノードはnode_typeであることをpush関数が保証しているので、dynamic_cast<>は不要。
		}

		{
			auto lk = g_odn_list_no_in_hazard_.try_lock();
			if ( lk.owns_lock() ) {
				p_ans_baseclass_node = lk.ref().pop_front();
				if ( p_ans_baseclass_node != nullptr ) {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
					--node_count_total_;
#endif
					// tl_odn_list_no_in_hazard.merge_push_back( std::move( lk.ref() ) );
					return static_cast<node_pointer>( p_ans_baseclass_node );
				}
			}
		}

		{
			// スレッドローカルのハザードポインタ登録中のリストの中から使えるノードを探す。
			tl_od_node_list& tl_odn_list_still_in_hazard = get_tl_odn_list_still_in_hazard();
			node_pointer     p_ans                       = try_pop_from_still_in_hazard_list(
                std::move( tl_odn_list_still_in_hazard.move_to() ),
                tl_odn_list_still_in_hazard,
                tl_odn_list_no_in_hazard );
			if ( p_ans != nullptr ) {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
				--node_count_total_;
#endif
				return p_ans;
			}
			// 使えるノードがなかった
		}

		{
			// グローバルのハザードポインタに登録中リストから使えるノードを探す。
			auto lk = g_odn_list_still_in_hazard_.try_lock();
			if ( lk.owns_lock() ) {
				node_pointer p_ans = try_pop_from_still_in_hazard_list( std::move( lk.ref() ), lk.ref(), tl_odn_list_no_in_hazard );
				if ( p_ans != nullptr ) {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
					--node_count_total_;
#endif
					return p_ans;
				}
				// 使えるノードがなかった
			}
		}

		return nullptr;
	}

	static void clear_as_possible_as( void )
	{
		tl_od_node_list& tl_odn_list_no_in_hazard = get_tl_odn_list_no_in_hazard();
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		node_count_total_.fetch_sub( tl_odn_list_no_in_hazard.size() );
#endif
		tl_odn_list_no_in_hazard.clear();

		{
			tl_od_node_list& tl_odn_list_still_in_hazard = get_tl_odn_list_still_in_hazard();
			raw_list         check_target_list           = std::move( tl_odn_list_still_in_hazard.move_to() );
			check_hazard_then_clear( check_target_list );
			tl_odn_list_still_in_hazard.merge_push_back( std::move( check_target_list ) );
		}

		{
			auto lk = g_odn_list_no_in_hazard_.try_lock();
			if ( lk.owns_lock() ) {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
				node_count_total_.fetch_sub( lk.ref().size() );
#endif
				lk.ref().clear();
			}
		}

		{
			auto lk = g_odn_list_still_in_hazard_.try_lock();
			if ( lk.owns_lock() ) {
				raw_list check_target_list = std::move( lk.ref() );
				check_hazard_then_clear( check_target_list );
				lk.ref().merge_push_back( std::move( check_target_list ) );
			}
		}

		return;
	}

	static size_t profile_info_count( void ) noexcept
	{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		return node_count_total_.load( std::memory_order_acquire );
#else
		return 0;
#endif
	}

	static std::string profile_info_string( void )
	{
		std::string ans;

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		ans = "Free nodes:";
		ans += "\ttotal: " + std::to_string( node_count_total_ );
		ans += "\ttl_odn_list_: " + std::to_string( tl_od_node_list::profile_info_all_tl_count() );
		ans += "\tg_odn_list_non_hazard_: " + std::to_string( g_odn_list_no_in_hazard_.lock().ref().size() );
		ans += "\tg_odn_list_still_in_hazard_: " + std::to_string( g_odn_list_still_in_hazard_.lock().ref().size() );
#else
		ans = "ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE is not enabled";
#endif

		return ans;
	}

private:
	using const_node_pointer = const NODE_T*;
	using raw_list           = od_simple_list;
	using g_node_list_t      = od_simple_list_lockable;

	class tl_od_node_list
#ifdef ALCONCURRENT_CONF_ENABLE_COUNTERMEASURE_GCC_BUG_66944
	  : public countermeasure_gcc_bug_deletable_obj_abst
#endif
	{
	public:
		using node_pointer = typename raw_list::node_pointer;

		constexpr tl_od_node_list( g_node_list_t& g_odn_list_arg )
		  : ref_g_odn_list_( g_odn_list_arg )
		  , od_list_()
		{
		}
		tl_od_node_list( tl_od_node_list&& src )
		  : ref_g_odn_list_( src.ref_g_odn_list_ )
		  , od_list_( std::move( src.od_list_ ) )
		{
		}
		~tl_od_node_list()
		{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			// internal::LogOutput( log_type::DUMP, "this thread local nodes: %zu, current total thread local nodes: %zu", od_list_.size(), node_count_in_tl_odn_list_.load() );
			node_count_in_tl_odn_list_ -= od_list_.size();
#endif
			ref_g_odn_list_.lock().ref().merge_push_back( std::move( od_list_ ) );
		}

		void push_back( node_pointer p )
		{
			od_list_.push_back( p );
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			if ( p != nullptr ) {
				node_count_in_tl_odn_list_++;
			}
#endif
		}

		node_pointer pop_front( void )
		{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			auto p_ans = od_list_.pop_front();
			if ( p_ans != nullptr ) {
				node_count_in_tl_odn_list_--;
			}
			return p_ans;
#else
			return od_list_.pop_front();
#endif
		}

		void merge_push_back( raw_list&& src ) noexcept
		{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			node_count_in_tl_odn_list_ += src.size();
#endif
			od_list_.merge_push_back( std::move( src ) );
		}

		bool is_empty( void )
		{
			return od_list_.is_empty();
		}
		bool is_one( void )
		{
			return od_list_.is_one();
		}
		bool is_more_than_one( void )
		{
			return od_list_.is_more_than_one();
		}

		void clear( void )
		{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			node_count_in_tl_odn_list_ -= od_list_.size();
#endif
			od_list_.clear();
		}

		raw_list move_to( void )
		{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			node_count_in_tl_odn_list_ -= od_list_.size();
#endif
			return std::move( od_list_ );
		}

		static size_t profile_info_all_tl_count( void )
		{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			return node_count_in_tl_odn_list_.load( std::memory_order_acquire );
#else
			return 0;
#endif
		}

		size_t size( void ) const noexcept
		{
			return od_list_.size();
		}

	private:
		g_node_list_t& ref_g_odn_list_;
		raw_list       od_list_;
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		static std::atomic<size_t> node_count_in_tl_odn_list_;
#endif
	};

	template <typename U = NODE_T, typename std::enable_if<is_member_function_callable_reset_value<U>::value>::type* = nullptr>
	static void reset_value_of_node( node_pointer p_nd )
	{
		p_nd->reset_value();
		return;
	}
	template <typename U = NODE_T, typename std::enable_if<!is_member_function_callable_reset_value<U>::value>::type* = nullptr>
	static void reset_value_of_node( node_pointer p_nd )
	{
		return;
	}
	static void reset_value_of_node_wrap( node_pointer p_nd )
	{
		reset_value_of_node( p_nd );
	}

	/**
	 * @brief check_target_listに含まれるすべてのノードに対し、ハザードポインタとして登録中かどうかを検査する。
	 *
	 * check_target_listに含まれるすべてのノードに対し、ハザードポインタとして登録中かどうかを検査し、登録中のノードは残す。
	 * また、登録されていないノードは解放する。
	 *
	 * @param check_target_list 検査対象のリスト
	 */
	static void check_hazard_then_clear( raw_list& check_target_list )
	{
		raw_list tmp_odn_list = std::move( check_target_list );

		if ( !tmp_odn_list.is_empty() ) {
			hazard_ptr_mgr::ScanHazardPtrs( [&tmp_odn_list, &check_target_list]( const void* p_in_hazard ) {
				auto tt_n_list = tmp_odn_list.split_if( [p_in_hazard]( auto p_cur_node ) -> bool {
					return p_in_hazard == static_cast<const_node_pointer>( p_cur_node )->get_pointer_of_hazard_check();   // pointerが同じならtrueを返す。
				} );
				check_target_list.merge_push_back( std::move( tt_n_list ) );
			} );
		}
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		node_count_total_.fetch_sub( tmp_odn_list.size() );
#endif
	}

	/**
	 * @brief input_still_in_hazard_list_argを検査し、ハザードポインタ登録されていないのノードの取り出しを試みる。
	 *
	 * @tparam STILL_IN_HAZARD_OUTPUT_LIST_T
	 * @param input_still_in_hazard_list_arg ノードの取り出しを試みるリスト
	 * @param output_still_in_hazard_list ノードの取り出し過程で、まだハザードポインタ登録中のノードを返すリスト
	 * @param output_no_hazard_list ノードの取り出し過程で、2つ目以降のハザードポインタに登録されていないノードを返すリスト
	 * @return node_pointer 取り出したノードへのポインタ。nullptrの場合、取り出せたノードがなかったことを示す。
	 */
	template <typename STILL_IN_HAZARD_OUTPUT_LIST_T>
	static node_pointer try_pop_from_still_in_hazard_list(
		raw_list&&                     input_still_in_hazard_list_arg,
		STILL_IN_HAZARD_OUTPUT_LIST_T& output_still_in_hazard_list,
		tl_od_node_list&               output_no_hazard_list )
	{
		raw_list tmp_odn_list( std::move( input_still_in_hazard_list_arg ) );

		if ( tmp_odn_list.is_empty() ) {
			// 使えるノードがなかった
		} else if ( tmp_odn_list.is_one() ) {
			node_pointer p_ans = static_cast<node_pointer>( tmp_odn_list.pop_front() );
			if ( !hazard_ptr_mgr::CheckPtrIsHazardPtr( p_ans->get_pointer_of_hazard_check() ) ) {
				// reset_value_of_node( p_ans ); しても良いが、呼び出し元へ戻したらすぐに上書きされるので、ここでの解放は無駄が多いと思われる。
				return p_ans;
			}
			output_still_in_hazard_list.push_back( p_ans );
			// 使えるノードがなかった
		} else {
			hazard_ptr_mgr::ScanHazardPtrs( [&tmp_odn_list, &output_still_in_hazard_list]( const void* p_in_hazard ) {
				auto tt_n_list = tmp_odn_list.split_if( [p_in_hazard]( auto p_cur_node ) -> bool {
					// cur_nodeの型は、od_node_simple_linkへの参照型で渡される。
					// cur_nodeの本来の型は、od_node_poolが保持する型node_typeである。
					// よって、ハザードポインタに登録されているポインタは、od_node_poolが保持する型node_typeへのポインタである。
					// 従って、比較すべきポインタは、od_node_poolが保持する型node_typeのポインター型である必要がある。
					return p_in_hazard == static_cast<const_node_pointer>( p_cur_node )->get_pointer_of_hazard_check();   // pointerが同じならtrueを返す。
				} );
				output_still_in_hazard_list.merge_push_back( std::move( tt_n_list ) );
			} );
			typename tl_od_node_list::node_pointer p_ans_baseclass_node = tmp_odn_list.pop_front();
			tmp_odn_list.for_each<node_pointer>( reset_value_of_node_wrap );
			output_no_hazard_list.merge_push_back( std::move( tmp_odn_list ) );
			if ( p_ans_baseclass_node != nullptr ) {
				node_pointer p_ans = static_cast<node_pointer>( p_ans_baseclass_node );
				// reset_value_of_node( p_ans ); しても良いが、呼び出し元へ戻したらすぐに上書きされるので、ここでの解放は無駄が多いと思われる。
				return p_ans;
			}
			// 使えるノードがなかった
		}

		return nullptr;
	}

	static tl_od_node_list& get_tl_odn_list_still_in_hazard( void );
	static tl_od_node_list& get_tl_odn_list_no_in_hazard( void );

	static g_node_list_t g_odn_list_no_in_hazard_;      //!< 他スレッド終了時等、共通プールとしてハザードポインタに登録されていないノードを引き受けるためのグローバル変数。
	static g_node_list_t g_odn_list_still_in_hazard_;   //!< 他スレッド終了時等、共通プールとしてハザードポインタに登録されているノードを引き受けるためのグローバル変数。
#ifdef ALCONCURRENT_CONF_ENABLE_COUNTERMEASURE_GCC_BUG_66944
	static thread_local tl_od_node_list* x_tl_p_odn_list_still_in_hazard_;   //!< ハザードポインタに登録されている場合に、一時保管するためのリスト。へのポインタ。下の表現方法の代用。
	static thread_local tl_od_node_list* x_tl_p_odn_list_no_in_hazard_;      //!< ハザードポインタに登録されている場合に、一時保管するためのリスト。へのポインタ。下の表現方法の代用。
#else
	static thread_local tl_od_node_list x_tl_odn_list_still_in_hazard_;   //!< ハザードポインタに登録されている場合に、一時保管するためのリスト。GCCの不具合でコンパイルエラーとなるため、スレッドローカル変数が使えない。。。
	static thread_local tl_od_node_list x_tl_odn_list_no_in_hazard_;      //!< ハザードポインタに登録されている場合に、一時保管するためのリスト。GCCの不具合でコンパイルエラーとなるため、スレッドローカル変数が使えない。。。
#endif
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	static std::atomic<size_t> node_count_total_;
#endif
};

template <typename NODE_T>
typename od_node_pool<NODE_T>::g_node_list_t od_node_pool<NODE_T>::g_odn_list_no_in_hazard_;
template <typename NODE_T>
typename od_node_pool<NODE_T>::g_node_list_t od_node_pool<NODE_T>::g_odn_list_still_in_hazard_;

#ifdef ALCONCURRENT_CONF_ENABLE_COUNTERMEASURE_GCC_BUG_66944
extern thread_local std::list<std::unique_ptr<countermeasure_gcc_bug_deletable_obj_abst>> tl_list_list;
template <typename NODE_T>
thread_local typename od_node_pool<NODE_T>::tl_od_node_list* od_node_pool<NODE_T>::x_tl_p_odn_list_still_in_hazard_ = nullptr;
template <typename NODE_T>
thread_local typename od_node_pool<NODE_T>::tl_od_node_list* od_node_pool<NODE_T>::x_tl_p_odn_list_no_in_hazard_ = nullptr;
#else
template <typename NODE_T>
thread_local typename od_node_pool<NODE_T>::tl_od_node_list od_node_pool<NODE_T>::x_tl_odn_list_still_in_hazard_( od_node_pool<NODE_T>::g_odn_list_still_in_hazard_ );
template <typename NODE_T>
thread_local typename od_node_pool<NODE_T>::tl_od_node_list od_node_pool<NODE_T>::x_tl_odn_list_no_in_hazard_( od_node_pool<NODE_T>::g_odn_list_no_in_hazard_ );
#endif

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
template <typename NODE_T>
std::atomic<size_t> od_node_pool<NODE_T>::node_count_total_( 0 );
template <typename NODE_T>
std::atomic<size_t> od_node_pool<NODE_T>::tl_od_node_list::node_count_in_tl_odn_list_( 0 );
#endif

template <typename NODE_T>
inline typename od_node_pool<NODE_T>::tl_od_node_list& od_node_pool<NODE_T>::get_tl_odn_list_still_in_hazard( void )
{
#ifdef ALCONCURRENT_CONF_ENABLE_COUNTERMEASURE_GCC_BUG_66944
	if ( x_tl_p_odn_list_still_in_hazard_ == nullptr ) {
		x_tl_p_odn_list_still_in_hazard_ = new tl_od_node_list( g_odn_list_still_in_hazard_ );
		tl_list_list.push_back( std::unique_ptr<countermeasure_gcc_bug_deletable_obj_abst>( x_tl_p_odn_list_still_in_hazard_ ) );
	}
	return *x_tl_p_odn_list_still_in_hazard_;
#else
	return x_tl_odn_list_still_in_hazard_;
#endif
}

template <typename NODE_T>
inline typename od_node_pool<NODE_T>::tl_od_node_list& od_node_pool<NODE_T>::get_tl_odn_list_no_in_hazard( void )
{
#ifdef ALCONCURRENT_CONF_ENABLE_COUNTERMEASURE_GCC_BUG_66944
	if ( x_tl_p_odn_list_no_in_hazard_ == nullptr ) {
		x_tl_p_odn_list_no_in_hazard_ = new tl_od_node_list( g_odn_list_no_in_hazard_ );
		tl_list_list.push_back( std::unique_ptr<countermeasure_gcc_bug_deletable_obj_abst>( x_tl_p_odn_list_no_in_hazard_ ) );
	}
	return *x_tl_p_odn_list_no_in_hazard_;
#else
	return x_tl_odn_list_no_in_hazard_;
#endif
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif /* ALCONCURRENT_INC_INTERNAL_OD_NODE_POOL_HPP_ */
