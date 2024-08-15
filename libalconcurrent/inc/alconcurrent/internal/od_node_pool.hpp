/**
 * @file od_node_pool.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief od_node<T> pool that keeps od_node<T> globally
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

#include "alconcurrent/hazard_ptr.hpp"
#include "alconcurrent/internal/od_node_base.hpp"

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

// template <typename NODE_T, typename OD_NODE_LIST_T = od_node_raw_list_base<NODE_T>, typename OD_NODE_LOCKFREE_STACK_T = od_node_stack_lockfree_base<NODE_T>>
template <typename NODE_T, typename RAW_LIST_NEXT_T>
class od_node_pool {
	static_assert( std::is_base_of<RAW_LIST_NEXT_T, NODE_T>::value, "RAW_LIST_NEXT_T should be a base class of NODE_T." );

public:
	using node_t       = NODE_T;
	using node_pointer = NODE_T*;

	static void push( node_pointer p_nd )
	{
		if ( p_nd == nullptr ) return;

#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_PUSH_FRONT_FUNCTION_NULLPTR
		if ( p_nd->p_raw_next_ != nullptr ) {
			LogOutput( log_type::WARN, "od_node_pool::push() receives a od_node<T> that has non nullptr in raw next" );
			p_nd->p_raw_next_ = nullptr;
		}
		if ( p_nd->hph_next_.load() != nullptr ) {
			LogOutput( log_type::WARN, "od_node_pool::push() receives a od_node<T> that has non nullptr in hph next" );
			p_nd->hph_next_.store( nullptr );
		}
#endif
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		++node_count_total_;
#endif

		tl_od_node_list& tl_odn_list_no_in_hazard = get_tl_odn_list_no_in_hazard();

		if ( hazard_ptr_mgr::CheckPtrIsHazardPtr( p_nd ) ) {
			get_tl_odn_list_still_in_hazard().push_back( p_nd );
			return;
		}

		if ( tl_odn_list_no_in_hazard.is_empty() ) {
			tl_odn_list_no_in_hazard.push_back( p_nd );   // スレッドローカルな変数が空だったので、スレッドローカルな変数に保存する。
			return;
		}

		auto lk = g_odn_list_.try_lock();
		if ( lk.owns_lock() ) {
			lk.ref().push_back( p_nd );   // ロック付きリストのロックを取得できたので、ロック付きリストに登録
			return;
		}

		tl_odn_list_no_in_hazard.push_back( p_nd );   // スレッドローカルな変数に保存する。
		return;
	}

	static node_pointer pop( void )
	{
		node_pointer p_ans = nullptr;

		tl_od_node_list& tl_odn_list_no_in_hazard = get_tl_odn_list_no_in_hazard();
		p_ans                                     = tl_odn_list_no_in_hazard.pop_front();
		if ( p_ans != nullptr ) {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			--node_count_total_;
#endif
			return p_ans;
		}

		auto lk = g_odn_list_.try_lock();
		if ( lk.owns_lock() ) {
			p_ans = lk.ref().pop_front();
			if ( p_ans != nullptr ) {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
				--node_count_total_;
#endif
				return p_ans;
			}
		}

		tl_od_node_list& tl_odn_list_still_in_hazard = get_tl_odn_list_still_in_hazard();
		if ( tl_odn_list_still_in_hazard.is_empty() ) {
			return nullptr;
		}

		raw_list tmp_odn_list_( std::move( tl_odn_list_still_in_hazard ) );

		hazard_ptr_mgr::ScanHazardPtrs( [&tmp_odn_list_, &tl_odn_list_still_in_hazard]( void* p_in_hazard ) {
			auto tt_n_list = tmp_odn_list_.split_if( [p_in_hazard]( const auto& cur_node ) -> bool {
				return p_in_hazard == &cur_node;   // pointerが同じならtrueを返す。
			} );
			tl_odn_list_still_in_hazard.merge_push_back( std::move( tt_n_list ) );
		} );
		p_ans = tmp_odn_list_.pop_front();
		tl_odn_list_no_in_hazard.merge_push_back( std::move( tmp_odn_list_ ) );
		if ( p_ans != nullptr ) {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			--node_count_total_;
#endif
			return p_ans;
		}

		return nullptr;   // 使えるノードがなかった
	}

	static void clear_as_possible_as( void )
	{
		auto lk = g_odn_list_.try_lock();
		if ( lk.owns_lock() ) {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			node_count_total_.fetch_sub( lk.ref().profile_info_count() );
#endif
			lk.ref().clear();
		}

		tl_od_node_list& tl_odn_list_no_in_hazard = get_tl_odn_list_no_in_hazard();
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		node_count_total_.fetch_sub( tl_odn_list_no_in_hazard.profile_info_count() );
#endif
		tl_odn_list_no_in_hazard.clear();

		tl_od_node_list& tl_odn_list_still_in_hazard = get_tl_odn_list_still_in_hazard();
		raw_list         tmp_odn_list                = std::move( tl_odn_list_still_in_hazard );

		if ( !tmp_odn_list.is_empty() ) {
			hazard_ptr_mgr::ScanHazardPtrs( [&tmp_odn_list, &tl_odn_list_still_in_hazard]( void* p_in_hazard ) {
				auto tt_n_list = tmp_odn_list.split_if( [p_in_hazard]( const auto& cur_node ) -> bool {
					return p_in_hazard == &cur_node;   // pointerが同じならtrueを返す。
				} );
				tl_odn_list_still_in_hazard.merge_push_back( std::move( tt_n_list ) );
			} );
		}
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		node_count_total_.fetch_sub( tmp_odn_list.profile_info_count() );
#endif

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
		ans += "\tg_odn_list_: " + std::to_string( g_odn_list_.lock().ref().profile_info_count() );
#else
		ans = "ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE is not enabled";
#endif

		return ans;
	}

protected:
	using raw_list      = od_node_list_base_impl<NODE_T, RAW_LIST_NEXT_T>;
	using g_node_list_t = od_node_list_lockable_base<raw_list>;
#ifdef ALCONCURRENT_CONF_ENABLE_COUNTERMEASURE_GCC_BUG_66944
	class tl_od_node_list : public raw_list, public countermeasure_gcc_bug_deletable_obj_abst {
#else
	class tl_od_node_list : private raw_list {
#endif
	public:
		using node_type    = typename raw_list::node_type;
		using node_pointer = typename raw_list::node_pointer;

		constexpr tl_od_node_list( g_node_list_t& g_odn_list_arg )
		  : ref_g_odn_list_( g_odn_list_arg )
		{
		}
		tl_od_node_list( tl_od_node_list&& src )
		  : raw_list( std::move( src ) )
		  , ref_g_odn_list_( src.ref_g_odn_list_ )
		{
		}
		~tl_od_node_list()
		{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			internal::LogOutput( log_type::TEST, "mv thread local node %zu (%zu)", raw_list::profile_info_count(), node_count_in_tl_odn_list_.load() );
			node_count_in_tl_odn_list_ -= raw_list::profile_info_count();
#endif
			ref_g_odn_list_.lock().ref().merge_push_back( std::move( *this ) );
		}

		void push_back( node_pointer p )
		{
			raw_list::push_back( p );
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			if ( p != nullptr ) {
				node_count_in_tl_odn_list_++;
			}
#endif
		}

		node_pointer pop_front( void )
		{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			auto p_ans = raw_list::pop_front();
			if ( p_ans != nullptr ) {
				node_count_in_tl_odn_list_--;
			}
			return p_ans;
#else
			return raw_list::pop_front();
#endif
		}

		void merge_push_back( raw_list&& src ) noexcept
		{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			node_count_in_tl_odn_list_ += src.profile_info_count();
#endif
			raw_list::merge_push_back( std::move( src ) );
		}

		bool is_empty( void )
		{
			return raw_list::is_empty();
		}

		size_t profile_info_count( void )
		{
			return raw_list::profile_info_count();
		}

		static size_t profile_info_all_tl_count( void )
		{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			return node_count_in_tl_odn_list_.load( std::memory_order_acquire );
#else
			return 0;
#endif
		}

	private:
		g_node_list_t& ref_g_odn_list_;
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		static std::atomic<size_t> node_count_in_tl_odn_list_;
#endif
	};

	static tl_od_node_list& get_tl_odn_list_still_in_hazard( void );
	static tl_od_node_list& get_tl_odn_list_no_in_hazard( void );

	static g_node_list_t g_odn_list_;   //!< 他スレッド終了時等、共通プールとしてノードを引き受けるためのグローバル変数。
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

template <typename NODE_T, typename OD_NODE_LIST_T>
typename od_node_pool<NODE_T, OD_NODE_LIST_T>::g_node_list_t od_node_pool<NODE_T, OD_NODE_LIST_T>::g_odn_list_;

#ifdef ALCONCURRENT_CONF_ENABLE_COUNTERMEASURE_GCC_BUG_66944
extern thread_local std::list<std::unique_ptr<countermeasure_gcc_bug_deletable_obj_abst>> tl_list_list;
template <typename NODE_T, typename OD_NODE_LIST_T>
thread_local typename od_node_pool<NODE_T, OD_NODE_LIST_T>::tl_od_node_list* od_node_pool<NODE_T, OD_NODE_LIST_T>::x_tl_p_odn_list_still_in_hazard_ = nullptr;
template <typename NODE_T, typename OD_NODE_LIST_T>
thread_local typename od_node_pool<NODE_T, OD_NODE_LIST_T>::tl_od_node_list* od_node_pool<NODE_T, OD_NODE_LIST_T>::x_tl_p_odn_list_no_in_hazard_ = nullptr;
#else
template <typename NODE_T, typename OD_NODE_LIST_T>
thread_local typename od_node_pool<NODE_T, OD_NODE_LIST_T>::tl_od_node_list od_node_pool<NODE_T, OD_NODE_LIST_T>::x_tl_odn_list_still_in_hazard_( od_node_pool<NODE_T, OD_NODE_LIST_T>::g_odn_list_ );
template <typename NODE_T, typename OD_NODE_LIST_T>
thread_local typename od_node_pool<NODE_T, OD_NODE_LIST_T>::tl_od_node_list od_node_pool<NODE_T, OD_NODE_LIST_T>::x_tl_odn_list_no_in_hazard_( od_node_pool<NODE_T, OD_NODE_LIST_T>::g_odn_list_ );
#endif

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
template <typename NODE_T, typename OD_NODE_LIST_T>
std::atomic<size_t> od_node_pool<NODE_T, OD_NODE_LIST_T>::node_count_total_( 0 );
template <typename NODE_T, typename OD_NODE_LIST_T>
std::atomic<size_t> od_node_pool<NODE_T, OD_NODE_LIST_T>::tl_od_node_list::node_count_in_tl_odn_list_( 0 );
#endif

template <typename NODE_T, typename OD_NODE_LIST_T>
inline typename od_node_pool<NODE_T, OD_NODE_LIST_T>::tl_od_node_list& od_node_pool<NODE_T, OD_NODE_LIST_T>::get_tl_odn_list_still_in_hazard( void )
{
#ifdef ALCONCURRENT_CONF_ENABLE_COUNTERMEASURE_GCC_BUG_66944
	if ( x_tl_p_odn_list_still_in_hazard_ == nullptr ) {
		x_tl_p_odn_list_still_in_hazard_ = new tl_od_node_list( g_odn_list_ );
		tl_list_list.push_back( std::unique_ptr<countermeasure_gcc_bug_deletable_obj_abst>( x_tl_p_odn_list_still_in_hazard_ ) );
	}
	return *x_tl_p_odn_list_still_in_hazard_;
#else
	return x_tl_odn_list_still_in_hazard_;
#endif
}

template <typename NODE_T, typename OD_NODE_LIST_T>
inline typename od_node_pool<NODE_T, OD_NODE_LIST_T>::tl_od_node_list& od_node_pool<NODE_T, OD_NODE_LIST_T>::get_tl_odn_list_no_in_hazard( void )
{
#ifdef ALCONCURRENT_CONF_ENABLE_COUNTERMEASURE_GCC_BUG_66944
	if ( x_tl_p_odn_list_no_in_hazard_ == nullptr ) {
		x_tl_p_odn_list_no_in_hazard_ = new tl_od_node_list( g_odn_list_ );
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
