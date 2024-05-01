/**
 * @file hazard_ptr.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-01
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/internal/alloc_only_allocator.hpp"
#include "alconcurrent/internal/cpp_std_configure.hpp"
#include "alconcurrent/internal/hazard_ptr_internal.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

global_scope_hazard_ptr_chain     global_scope_hazard_ptr_chain::g_scope_hzrd_chain_;
thread_local bind_hazard_ptr_list tl_bhpl;

///////////////////////////////////////////////////////////////////////
hazard_ptr_group::~hazard_ptr_group()
{
	for ( auto& e : *this ) {
		if ( e.load( std::memory_order_acquire ) != nullptr ) {
			LogOutput( log_type::ERR, "hazard pointer is still exist." );
		}
	}
}

hazard_ptr_group::hzrd_slot_ownership_t hazard_ptr_group::try_assign( void* p )
{
	for ( auto it = next_assign_hint_it_; it != end(); it++ ) {
		void* expected_p = nullptr;
		if ( it->compare_exchange_strong( expected_p, p, std::memory_order_release, std::memory_order_relaxed ) ) {
			hzrd_slot_ownership_t ans( &( *it ) );
			++it;
			if ( it == end() ) {
				it = begin();
			}
			next_assign_hint_it_ = it;
			return ans;
		}
	}
	auto wraparound_end = next_assign_hint_it_;
	for ( auto it = begin(); it != wraparound_end; it++ ) {
		void* expected_p = nullptr;
		if ( it->compare_exchange_strong( expected_p, p, std::memory_order_release, std::memory_order_relaxed ) ) {
			hzrd_slot_ownership_t ans( &( *it ) );
			++it;
			if ( it == end() ) {
				it = begin();
			}
			next_assign_hint_it_ = it;
			return ans;
		}
	}

	return hzrd_slot_ownership_t( nullptr );
}

hazard_ptr_group::ownership_t hazard_ptr_group::try_ocupy( void )
{
	bool expected_b = false;
	if ( is_used_.compare_exchange_strong( expected_b, true, std::memory_order_release, std::memory_order_relaxed ) ) {
		return ownership_t( this );
	}

	return ownership_t( nullptr );
}

//////////////////////////////////////////////////////////////////////////////
bind_hazard_ptr_list::~bind_hazard_ptr_list()
{
	if ( global_scope_hazard_ptr_chain::IsDestoryed() ) return;

	hazard_ptr_group* p_cur_list = ownership_ticket_.get();
	while ( p_cur_list != nullptr ) {
		for ( auto& e : *p_cur_list ) {
			if ( e.load( std::memory_order_acquire ) != nullptr ) {
				LogOutput( log_type::ERR, "hazard pointer is still exist." );
			}
			e.store( nullptr, std::memory_order_release );
		}

		hazard_ptr_group* p_next_list = p_cur_list->ap_list_next_.load( std::memory_order_acquire );
		p_cur_list                    = p_next_list;
	}
}

bind_hazard_ptr_list::hzrd_slot_ownership_t bind_hazard_ptr_list::assign( void* p )
{
	hazard_ptr_group* p_pre_list = nullptr;
	hazard_ptr_group* p_cur_list = ownership_ticket_.get();
	while ( p_cur_list != nullptr ) {
		hzrd_slot_ownership_t ans = p_cur_list->try_assign( p );
		if ( ans != nullptr ) {
			return ans;   // success to assign the slot of hazard pointer
		}

		hazard_ptr_group* p_next_list = p_cur_list->ap_list_next_.load( std::memory_order_acquire );
		p_pre_list                    = p_cur_list;
		p_cur_list                    = p_next_list;
	}

	hazard_ptr_group* p_new_hpg = nullptr;
	if ( p_pre_list != nullptr ) {
		// 追加割り当てが必要になった状態
		p_new_hpg = new hazard_ptr_group;
		p_pre_list->ap_list_next_.store( p_new_hpg, std::memory_order_release );
	} else {
		// まだなにも割り当てられていない状態。
		// グローバルからの再利用を行う。
		ownership_ticket_ = global_scope_hazard_ptr_chain::GetOwnership();
		p_new_hpg         = ownership_ticket_.get();
	}

	hzrd_slot_ownership_t ans = p_new_hpg->try_assign( p );
	if ( ans == nullptr ) {
		throw std::logic_error( "Fail to assign hazard pointer slot. this is logic error" );
	}
	return ans;
}

//////////////////////////////////////////////////////////////////////////////

hazard_ptr_group::ownership_t global_scope_hazard_ptr_chain::try_get_ownership( void )
{
	hazard_ptr_group* p_cur_hpg = ap_top_hzrd_ptr_chain_.load( std::memory_order_acquire );
	while ( p_cur_hpg != nullptr ) {
		hazard_ptr_group::ownership_t ans = p_cur_hpg->try_ocupy();
		if ( ans != nullptr ) return ans;   // success to get ownership

		hazard_ptr_group* p_next_hpg = p_cur_hpg->ap_chain_next_.load( std::memory_order_acquire );
		p_cur_hpg                    = p_next_hpg;
	}

	return nullptr;
}

void global_scope_hazard_ptr_chain::register_new_hazard_ptr_group( hazard_ptr_group* p_hpg_arg )
{
	// push p_phg_arg to top of chain like stack(LILO, even if not allow out)
	hazard_ptr_group* p_cur_top_hpg = ap_top_hzrd_ptr_chain_.load( std::memory_order_acquire );
	p_hpg_arg->ap_chain_next_.store( p_cur_top_hpg, std::memory_order_release );
	while ( !ap_top_hzrd_ptr_chain_.compare_exchange_weak( p_cur_top_hpg, p_hpg_arg, std::memory_order_release, std::memory_order_release ) ) {
		p_hpg_arg->ap_chain_next_.store( p_cur_top_hpg, std::memory_order_release );
	}
}

hazard_ptr_group::ownership_t global_scope_hazard_ptr_chain::get_ownership( void )
{
	hazard_ptr_group::ownership_t ans = try_get_ownership();
	if ( ans == nullptr ) {
		hazard_ptr_group* p_new_hpg = new hazard_ptr_group;
		ans                         = p_new_hpg->try_ocupy();
		if ( ans == nullptr ) {
			delete p_new_hpg;
			throw std::logic_error( "Fail to get ownership. this is logic error" );
		}
		register_new_hazard_ptr_group( p_new_hpg );
	}

	return ans;
}

bool global_scope_hazard_ptr_chain::check_pointer_is_hazard_pointer( void* p )
{
	if ( p == nullptr ) return false;

	hazard_ptr_group* p_cur_chain = ap_top_hzrd_ptr_chain_.load( std::memory_order_acquire );

	while ( p_cur_chain != nullptr ) {
		hazard_ptr_group* p_next_chain = p_cur_chain->ap_chain_next_.load( std::memory_order_acquire );
		hazard_ptr_group* p_cur_list   = p_cur_chain;
		while ( p_cur_list != nullptr ) {
			for ( auto& e : *p_cur_list ) {
				// if hazard_ptr_group has a pointer that is same to p, p is hazard pointer
				if ( e.load( std::memory_order_acquire ) == p ) return true;
			}
			hazard_ptr_group* p_next_list = p_cur_list->ap_list_next_.load( std::memory_order_acquire );
			p_cur_list                    = p_next_list;
		}
		p_cur_chain = p_next_chain;
	}

	return false;
}

void global_scope_hazard_ptr_chain::remove_all( void )
{
	tl_bhpl = bind_hazard_ptr_list();

	hazard_ptr_group* p_cur_chain = ap_top_hzrd_ptr_chain_.load( std::memory_order_acquire );
	ap_top_hzrd_ptr_chain_.store( nullptr, std::memory_order_release );

	while ( p_cur_chain != nullptr ) {
		hazard_ptr_group* p_next_chain = p_cur_chain->ap_chain_next_.load( std::memory_order_acquire );
		hazard_ptr_group* p_cur_list   = p_cur_chain;
		while ( p_cur_list != nullptr ) {
			hazard_ptr_group* p_next_list = p_cur_list->ap_list_next_.load( std::memory_order_acquire );
			delete p_cur_list;
			p_cur_list = p_next_list;
		}
		p_cur_chain = p_next_chain;
	}
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
