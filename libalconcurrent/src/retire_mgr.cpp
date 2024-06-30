/**
 * @file retire_mgr.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-04
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include <atomic>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/internal/alloc_only_allocator.hpp"
#include "alconcurrent/internal/cpp_std_configure.hpp"
#include "alconcurrent/internal/hazard_ptr_internal.hpp"
#include "alconcurrent/internal/retire_mgr.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

std::atomic<size_t> retire_node_abst::count_allocate_;

class retire_node_list : public od_node_list_base<retire_node_abst> {
public:
	retire_node_list( void )                               = default;
	retire_node_list( const retire_node_list& )            = delete;
	retire_node_list( retire_node_list&& )                 = default;
	retire_node_list& operator=( const retire_node_list& ) = delete;
	retire_node_list& operator=( retire_node_list&& )      = default;

	void recycle_all( void )
	{
		if ( is_empty() ) {
			return;
		}

		retire_node_list nodes_not_in_hazard( std::move( *this ) );   // いったん自分自身のノードを移す。処理が終わったときに残っているノードがハザードポインタに登録されていないノードのリストとなっている。
		retire_node_list nodes_still_in_hazard;

		internal::hazard_ptr_mgr::ScanHazardPtrs(
			[&nodes_not_in_hazard, &nodes_still_in_hazard]( void* p_hzd ) {
				od_node_list_base<retire_node_abst> ret = nodes_not_in_hazard.split_if( [p_hzd]( const retire_node_abst& rnd ) {
					return rnd.get_retire_pointer() == p_hzd;
				} );
				nodes_still_in_hazard.merge_push_back( std::move( ret ) );
			} );
		swap( nodes_still_in_hazard );   // ハザードポインタに登録されているノードを自分自身に戻す。

		nodes_not_in_hazard.clear(
			[]( retire_node_abst* p_target ) {
				p_target->call_deleter();
				retire_node_abst::recycle( p_target );
			} );
	}
};

/**
 * @brief スレッドローカルストレージを一時バッファとして、グローバル共有バッファにod_node<T>をプールする
 */
class unorder_retire_node_buffer {
public:
	using node_type    = retire_node_abst;
	using node_pointer = retire_node_abst*;

	static void push( node_pointer p_nd )
	{
		if ( p_nd == nullptr ) return;

		tl_rnd_list_.push_back( p_nd );
		auto lk = g_rnd_list_.try_lock();
		if ( lk.owns_lock() ) {
			lk.ref().merge_push_back( std::move( tl_rnd_list_ ) );
			lk.notify_all();
		}
	}

	static void merge_push_with_lock( retire_node_list&& src_rnd_list )
	{
		if ( src_rnd_list.is_empty() ) return;
		g_rnd_list_.lock().ref().merge_push_front( std::move( src_rnd_list ) );
	}

	static void merge_push_without_lock( retire_node_list&& src_rnd_list )
	{
		if ( src_rnd_list.is_empty() ) return;

		tl_rnd_list_.merge_push_back( std::move( src_rnd_list ) );
		auto lk = g_rnd_list_.try_lock();
		if ( lk.owns_lock() ) {
			lk.ref().merge_push_back( std::move( tl_rnd_list_ ) );
			lk.notify_all();
		}
	}

	static retire_node_list pop_all( void )
	{
		retire_node_list ans = std::move( tl_rnd_list_ );
		ans.merge_push_back( std::move( g_rnd_list_.lock().ref() ) );
		return ans;
	}

	static retire_node_list try_pop_all( void )
	{
		retire_node_list ans = std::move( tl_rnd_list_ );
		auto             lk  = g_rnd_list_.lock();
		if ( lk.owns_lock() ) {
			ans.merge_push_back( std::move( lk.ref() ) );
		}
		return ans;
	}

	static retire_node_list wait_pop_all( void )
	{
		retire_node_list ans;
		auto             locker = g_rnd_list_.lock();
		locker.wait( [&locker]() -> bool { return !( locker.ref().is_empty() ); } );
		return retire_node_list( std::move( locker.ref() ) );
	}

private:
	using g_retire_node_list = od_node_list_conditional_lockable_base<retire_node_list>;
	class tl_retire_node_list : public retire_node_list {
	public:
		constexpr tl_retire_node_list( g_retire_node_list& g_rnd_list_arg )
		  : ref_g_rnd_list_( g_rnd_list_arg )
		{
		}
		~tl_retire_node_list()
		{
			ref_g_rnd_list_.lock().ref().merge_push_back( std::move( *this ) );
		}

	private:
		g_retire_node_list& ref_g_rnd_list_;
	};

	static g_retire_node_list               g_rnd_list_;
	static thread_local tl_retire_node_list tl_rnd_list_;
};

unorder_retire_node_buffer::g_retire_node_list               unorder_retire_node_buffer::g_rnd_list_;
thread_local unorder_retire_node_buffer::tl_retire_node_list unorder_retire_node_buffer::tl_rnd_list_( unorder_retire_node_buffer::g_rnd_list_ );

///////////////////////////////////////////////////////////////////////////////////////
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
ALCC_INTERNAL_CONSTINIT alignas( internal::atomic_variable_align ) std::atomic<size_t> g_call_count_retire( 0 );
#endif

///////////////////////////////////////////////////////////////////////////////////////
class retire_mgr::prune_thread_inst_t {
public:
	prune_thread_inst_t( void )
	  : prune_thread_obj_()
	{
	}

	void start( void )
	{
		if ( prune_thread_obj_.joinable() ) return;   // already started

		prune_thread_obj_ = std::thread( retire_mgr::prune_thread );
	}

	void stop( void )
	{
		if ( prune_thread_obj_.joinable() ) {
			retire_mgr::request_stop_prune_thread();
			prune_thread_obj_.join();
		}
	}

	~prune_thread_inst_t()
	{
		stop();
	}

private:
	std::thread prune_thread_obj_;
};

retire_mgr::prune_thread_inst_t retire_mgr::g_prune_thread_obj;

///////////////////////////////////////////////////////////////////////////////////////
class retire_mgr::prune_thread_mgr {
public:
	prune_thread_mgr( void )
	{
		g_prune_thread_obj.start();
	}

	~prune_thread_mgr()
	{
		g_prune_thread_obj.stop();
	}

	void increment_call_count( void )
	{
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
		g_call_count_retire++;
#endif
	}
};

///////////////////////////////////////////////////////////////////////////////////////
std::atomic<bool> retire_mgr::loop_flag_prune_thread_( true );

void retire_mgr::prune_one_work( void )
{
	retire_node_list recycle_list;
	recycle_list = unorder_retire_node_buffer::wait_pop_all();

	recycle_list.recycle_all();

	if ( recycle_list.is_empty() ) return;

	unorder_retire_node_buffer::merge_push_with_lock( std::move( recycle_list ) );
}

void retire_mgr::prune_thread( void )
{
	loop_flag_prune_thread_.store( true, std::memory_order_release );
	while ( loop_flag_prune_thread_.load( std::memory_order_acquire ) ) {
#ifdef ALCONCURRENT_CONF_ENABLE_PRUNE_THREAD_SLEEP_SEC
		std::this_thread::sleep_for( std::chrono::seconds( ALCONCURRENT_CONF_ENABLE_PRUNE_THREAD_SLEEP_SEC ) );
#endif
		prune_one_work();
	}
}

void retire_mgr::request_stop_prune_thread( void )
{
	loop_flag_prune_thread_.store( false );

	// push dummy node to trigger prune thread
	unorder_retire_node_buffer::push( new retire_node<int, std::default_delete<int>>( nullptr, std::default_delete<int>() ) );
}

void retire_mgr::stop_prune_thread( void )
{
	g_prune_thread_obj.stop();
}

void retire_mgr::retire_impl( retire_node_abst* p_new_retire )
{
#ifdef ALCONCURRENT_CONF_ENABLE_PRUNE_THREAD
	static prune_thread_mgr pt_mgr_obj;
	pt_mgr_obj.increment_call_count();
#else
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	g_call_count_retire++;
#endif
	{
		static thread_local int recycle_skip = 0;
		recycle_skip++;
		if ( recycle_skip > 4 ) {
			recycle_skip                  = 0;
			retire_node_list recycle_list = unorder_retire_node_buffer::try_pop_all();

			recycle_list.recycle_all();

			if ( !recycle_list.is_empty() ) {
				unorder_retire_node_buffer::merge_push_without_lock( std::move( recycle_list ) );
			}
		}
	}
#endif

	unorder_retire_node_buffer::push( p_new_retire );
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
