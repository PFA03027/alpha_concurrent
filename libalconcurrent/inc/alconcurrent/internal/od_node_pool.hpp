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
template <typename NODE_T, typename RAW_LIST_NEXT_T, typename HPH_LIST_NEXT_T>
class od_node_pool {
	static_assert( std::is_base_of<RAW_LIST_NEXT_T, NODE_T>::value, "RAW_LIST_NEXT_T should be a base class of NODE_T." );
	static_assert( std::is_base_of<HPH_LIST_NEXT_T, NODE_T>::value, "HPH_LIST_NEXT_T should be a base class of NODE_T." );

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

		tl_od_node_list& tl_odn_list_ = get_tl_od_node_list();

		if ( tl_odn_list_.is_empty() ) {
			tl_odn_list_.push_back( p_nd );   // スレッドローカルな変数が空だったので、やっぱりスレッドローカルな変数に保存する。
			return;
		}

		auto lk = g_odn_list_.try_lock();
		if ( lk.owns_lock() ) {
			lk.ref().push_back( p_nd );
			return;
		}

		if ( hazard_ptr_mgr::CheckPtrIsHazardPtr( p_nd ) ) {
			tl_odn_list_.push_back( p_nd );   // まだハザードポインタに登録されていたので、スレッドローカルな変数に保存する。
			return;
		}

		g_lockfree_odn_list_.push_front( p_nd );
	}

	static node_pointer pop( void )
	{
		tl_od_node_list& tl_odn_list_ = get_tl_od_node_list();
		node_pointer     p_ans        = nullptr;

		p_ans = tl_odn_list_.pop_front();
		if ( p_ans != nullptr ) {
			if ( !hazard_ptr_mgr::CheckPtrIsHazardPtr( p_ans ) ) {
				p_ans->clear_next();
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
				node_count_total_--;
#endif
				return p_ans;
			}
			tl_odn_list_.push_back( p_ans );   // まだハザードポインタに登録されていたので、スレッドローカルな変数に差し戻す。
		}

		auto lk = g_odn_list_.try_lock();
		if ( lk.owns_lock() ) {
			p_ans = lk.ref().pop_front();
			while ( p_ans != nullptr ) {
				if ( !hazard_ptr_mgr::CheckPtrIsHazardPtr( p_ans ) ) {
					p_ans->clear_next();
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
					node_count_total_--;
#endif
#ifdef ALCONCURRENT_CONF_OD_NODE_PREFER_TO_MOVE_FROM_GLOBAL_LIST
					tl_odn_list_.merge_push_back( std::move( lk.ref() ) );
#endif
					return p_ans;
				}
				tl_odn_list_.push_back( p_ans );   // まだハザードポインタに登録されていたので、スレッドローカルな変数に差し戻す。
				p_ans = lk.ref().pop_front();
			}
		}

		p_ans = g_lockfree_odn_list_.pop_front();
		while ( p_ans != nullptr ) {
			if ( !hazard_ptr_mgr::CheckPtrIsHazardPtr( p_ans ) ) {
				// popped node is not in hazard pointer list. therefore clear hph_next_
				p_ans->clear_next();

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
				node_count_total_--;
#endif
				return p_ans;
			}
			tl_odn_list_.push_back( p_ans );   // ハザードポインタに登録されていたので、スレッドローカルな変数に差し戻す。
			p_ans = g_lockfree_odn_list_.pop_front();
		}

		return nullptr;   // 使えるノードがなかった
	}

	static void clear_as_possible_as( void )
	{
#if 1
		tl_od_node_list& tl_odn_list_   = get_tl_od_node_list();
		tl_od_node_list  tmp_odn_list_  = std::move( tl_odn_list_ );
		node_pointer     p_working_node = nullptr;

		p_working_node = tmp_odn_list_.pop_front();
		while ( p_working_node != nullptr ) {
			if ( hazard_ptr_mgr::CheckPtrIsHazardPtr( p_working_node ) ) {
				tl_odn_list_.push_back( p_working_node );   // まだハザードポインタに登録されていたので、スレッドローカルな変数に差し戻す。
			} else {
				p_working_node->clear_next();
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
				node_count_total_--;
#endif
				delete p_working_node;
			}
			p_working_node = tmp_odn_list_.pop_front();
		}

		auto lk = g_odn_list_.try_lock();
		if ( lk.owns_lock() ) {
			p_working_node = lk.ref().pop_front();
			while ( p_working_node != nullptr ) {
				if ( hazard_ptr_mgr::CheckPtrIsHazardPtr( p_working_node ) ) {
					// まだハザードポインタに登録されていたので、差し戻す。
					tl_odn_list_.push_back( p_working_node );
				} else {
					p_working_node->clear_next();
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
					node_count_total_--;
#endif
					delete p_working_node;
				}
				p_working_node = lk.ref().pop_front();
			}
		}

		p_working_node = g_lockfree_odn_list_.pop_front();
		while ( p_working_node != nullptr ) {
			if ( hazard_ptr_mgr::CheckPtrIsHazardPtr( p_working_node ) ) {
				tl_odn_list_.push_back( p_working_node );   // ハザードポインタに登録されていたので、スレッドローカルな変数に差し戻す。
			} else {
				// popped node is not in hazard pointer list. therefore clear hph_next_
				p_working_node->clear_next();
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
				node_count_total_--;
#endif
				delete p_working_node;
			}
			p_working_node = g_lockfree_odn_list_.pop_front();
		}
#else
		internal::LogOutput( log_type::WARN, "clear_as_possible_as() is not implemented yet" );
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
		ans += "\ttl_odn_list_: " + std::to_string( tl_od_node_list::profile_info_count() );
		ans += "\tg_odn_list_: " + std::to_string( g_odn_list_.lock().ref().profile_info_count() );
		ans += "\tg_lockfree_odn_list_: " + std::to_string( g_lockfree_odn_list_.profile_info_count() );
#else
		ans = "ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE is not enabled";
#endif

		return ans;
	}

protected:
	using raw_list          = od_node_list_base_impl<NODE_T, RAW_LIST_NEXT_T>;
	using g_node_list_t     = od_node_list_lockable_base<raw_list>;
	using g_node_lf_stack_t = od_node_stack_lockfree_base<NODE_T, HPH_LIST_NEXT_T>;
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

		static size_t profile_info_count( void )
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

	static tl_od_node_list& get_tl_od_node_list( void );

	static g_node_list_t     g_odn_list_;            //!< 他スレッド終了時にノードを引き受けるためのグローバル変数。
	static g_node_lf_stack_t g_lockfree_odn_list_;   //!< 通常使用するノードプールのためのリスト
#ifdef ALCONCURRENT_CONF_ENABLE_COUNTERMEASURE_GCC_BUG_66944
	static thread_local tl_od_node_list* tl_p_odn_list_;   //!< ハザードポインタに登録されている場合に、一時保管するためのリスト。へのポインタ。下の表現方法の代用。
#else
	static thread_local tl_od_node_list tl_odn_list_;   //!< ハザードポインタに登録されている場合に、一時保管するためのリスト。GCCの不具合でコンパイルエラーとなるため、スレッドローカル変数が使えない。。。
#endif
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	static std::atomic<size_t> node_count_total_;
#endif
};

template <typename NODE_T, typename OD_NODE_LIST_T, typename OD_NODE_LOCKFREE_STACK_T>
typename od_node_pool<NODE_T, OD_NODE_LIST_T, OD_NODE_LOCKFREE_STACK_T>::g_node_list_t od_node_pool<NODE_T, OD_NODE_LIST_T, OD_NODE_LOCKFREE_STACK_T>::g_odn_list_;

template <typename NODE_T, typename OD_NODE_LIST_T, typename OD_NODE_LOCKFREE_STACK_T>
typename od_node_pool<NODE_T, OD_NODE_LIST_T, OD_NODE_LOCKFREE_STACK_T>::g_node_lf_stack_t od_node_pool<NODE_T, OD_NODE_LIST_T, OD_NODE_LOCKFREE_STACK_T>::g_lockfree_odn_list_;

#ifdef ALCONCURRENT_CONF_ENABLE_COUNTERMEASURE_GCC_BUG_66944
extern thread_local std::list<std::unique_ptr<countermeasure_gcc_bug_deletable_obj_abst>> tl_list_list;
template <typename NODE_T, typename OD_NODE_LIST_T, typename OD_NODE_LOCKFREE_STACK_T>
thread_local typename od_node_pool<NODE_T, OD_NODE_LIST_T, OD_NODE_LOCKFREE_STACK_T>::tl_od_node_list* od_node_pool<NODE_T, OD_NODE_LIST_T, OD_NODE_LOCKFREE_STACK_T>::tl_p_odn_list_ = nullptr;
#else
template <typename NODE_T, typename OD_NODE_LIST_T, typename OD_NODE_LOCKFREE_STACK_T>
thread_local typename od_node_pool<NODE_T, OD_NODE_LIST_T, OD_NODE_LOCKFREE_STACK_T>::tl_od_node_list od_node_pool<NODE_T, OD_NODE_LIST_T, OD_NODE_LOCKFREE_STACK_T>::tl_odn_list_( od_node_pool<NODE_T, OD_NODE_LIST_T, OD_NODE_LOCKFREE_STACK_T>::g_odn_list_ );
#endif

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
template <typename NODE_T, typename OD_NODE_LIST_T, typename OD_NODE_LOCKFREE_STACK_T>
std::atomic<size_t> od_node_pool<NODE_T, OD_NODE_LIST_T, OD_NODE_LOCKFREE_STACK_T>::node_count_total_( 0 );
template <typename NODE_T, typename OD_NODE_LIST_T, typename OD_NODE_LOCKFREE_STACK_T>
std::atomic<size_t> od_node_pool<NODE_T, OD_NODE_LIST_T, OD_NODE_LOCKFREE_STACK_T>::tl_od_node_list::node_count_in_tl_odn_list_( 0 );
#endif

template <typename NODE_T, typename OD_NODE_LIST_T, typename OD_NODE_LOCKFREE_STACK_T>
inline typename od_node_pool<NODE_T, OD_NODE_LIST_T, OD_NODE_LOCKFREE_STACK_T>::tl_od_node_list& od_node_pool<NODE_T, OD_NODE_LIST_T, OD_NODE_LOCKFREE_STACK_T>::get_tl_od_node_list( void )
{
#ifdef ALCONCURRENT_CONF_ENABLE_COUNTERMEASURE_GCC_BUG_66944
	if ( tl_p_odn_list_ == nullptr ) {
		tl_p_odn_list_ = new tl_od_node_list( g_odn_list_ );
		tl_list_list.push_back( std::unique_ptr<countermeasure_gcc_bug_deletable_obj_abst>( tl_p_odn_list_ ) );
	}
	return *tl_p_odn_list_;
#else
	return tl_odn_list_;
#endif
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif /* ALCONCURRENT_INC_INTERNAL_OD_NODE_POOL_HPP_ */
