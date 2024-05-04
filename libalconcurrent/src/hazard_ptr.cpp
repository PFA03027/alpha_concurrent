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
#include <string>
#include <thread>

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/internal/alloc_only_allocator.hpp"
#include "alconcurrent/internal/cpp_std_configure.hpp"
#include "alconcurrent/internal/hazard_ptr_internal.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

////////////////////////////////////////////////////////////////////////////////////////////////////////
// configuration value
constexpr size_t conf_pre_mmap_size = 16 * 1024;

static ALCC_INTERNAL_CONSTINIT alloc_only_chamber g_alloc_only_inst_for_hzrd_ptr_module( false, conf_pre_mmap_size );   // グローバルインスタンスは、プロセス終了までメモリ領域を維持するために、デストラクタが呼ばれてもmmapした領域を解放しない。

////////////////////////////////////////////////////////////////////////////////////////////////////////
ALCC_INTERNAL_CONSTINIT global_scope_hazard_ptr_chain     global_scope_hazard_ptr_chain::g_scope_hzrd_chain_;
thread_local ALCC_INTERNAL_CONSTINIT bind_hazard_ptr_list tl_bhpl;

///////////////////////////////////////////////////////////////////////
#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
std::atomic<size_t> hazard_ptr_group::call_count_try_assign_( 0 );
std::atomic<size_t> hazard_ptr_group::loop_count_in_try_assign_( 0 );

std::atomic<size_t> call_count_hazard_ptr_get_( 0 );
std::atomic<size_t> loop_count_in_hazard_ptr_get_( 0 );
#endif

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
#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
	call_count_try_assign_++;
#endif
	{
		void* expected_p = nullptr;
		if ( next_assign_hint_it_->compare_exchange_strong( expected_p, p, std::memory_order_release, std::memory_order_relaxed ) ) {
			hzrd_slot_ownership_t ans( &( *next_assign_hint_it_ ) );
			++next_assign_hint_it_;
			if ( next_assign_hint_it_ == end() ) {
				next_assign_hint_it_ = begin();
			}
			return ans;
		}
	}

	for ( auto it = next_assign_hint_it_; it != end(); it++ ) {
#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
		loop_count_in_try_assign_++;
#endif
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
#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
		loop_count_in_try_assign_++;
#endif
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

#ifdef ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
#else
#if __cpp_aligned_new
ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new( std::size_t size, std::align_val_t alignment )   // possible throw std::bad_alloc, from C++17
{
	void* p_ans = g_alloc_only_inst_for_hzrd_ptr_module.allocate( size, static_cast<size_t>( alignment ) );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}
	return p_ans;
}
ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new( std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++17
{
	return g_alloc_only_inst_for_hzrd_ptr_module.allocate( size, static_cast<size_t>( alignment ) );
}
ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new[]( std::size_t size, std::align_val_t alignment )   // possible throw std::bad_alloc, from C++17
{
	void* p_ans = g_alloc_only_inst_for_hzrd_ptr_module.allocate( size, static_cast<size_t>( alignment ) );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}
	return p_ans;
}
ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new[]( std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++17
{
	return g_alloc_only_inst_for_hzrd_ptr_module.allocate( size, static_cast<size_t>( alignment ) );
}
#endif
ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new( std::size_t size )   // possible throw std::bad_alloc, from C++11
{
	void* p_ans = g_alloc_only_inst_for_hzrd_ptr_module.allocate( size );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}
	return p_ans;
}
ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new( std::size_t size, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++11
{
	return g_alloc_only_inst_for_hzrd_ptr_module.allocate( size );
}
ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new[]( std::size_t size )   // possible throw std::bad_alloc, from C++11
{
	void* p_ans = g_alloc_only_inst_for_hzrd_ptr_module.allocate( size );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}
	return p_ans;
}
ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new[]( std::size_t size, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++11
{
	return g_alloc_only_inst_for_hzrd_ptr_module.allocate( size );
}

ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new( std::size_t size, void* ptr ) noexcept   // placement new, from C++11
{
	return ptr;
}
ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new[]( std::size_t size, void* ptr ) noexcept   // placement new for array, from C++11
{
	return ptr;
}

#if __cpp_aligned_new
void hazard_ptr_group::operator delete( void* ptr, std::align_val_t alignment ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete( void* ptr, std::size_t size, std::align_val_t alignment ) noexcept   // from C++17
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete[]( void* ptr, std::align_val_t alignment ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete[]( void* ptr, std::size_t size, std::align_val_t alignment ) noexcept   // from C++17
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete( void* ptr, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete( void* ptr, std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete[]( void* ptr, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete[]( void* ptr, std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
#endif
void hazard_ptr_group::operator delete( void* ptr ) noexcept   // from C++11
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete[]( void* ptr ) noexcept   // from C++11
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}

void hazard_ptr_group::operator delete( void* ptr, std::size_t size ) noexcept   // from C++14
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete[]( void* ptr, std::size_t size ) noexcept   // from C++14
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}

void hazard_ptr_group::operator delete( void* ptr, const std::nothrow_t& ) noexcept   // from C++11
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete[]( void* ptr, const std::nothrow_t& ) noexcept   // from C++11
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}

void hazard_ptr_group::operator delete( void* ptr, std::size_t size, const std::nothrow_t& ) noexcept   // from C++14
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete[]( void* ptr, std::size_t size, const std::nothrow_t& ) noexcept   // from C++14
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}

void hazard_ptr_group::operator delete( void* ptr, void* ) noexcept   // delete for area that is initialized by placement new.
{
	// nothing to do
}
void hazard_ptr_group::operator delete[]( void* ptr, void* ) noexcept   // delete for area that is initialized by placement new.
{
	// nothing to do
}
#endif

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

bind_hazard_ptr_list::hzrd_slot_ownership_t bind_hazard_ptr_list::slot_assign( void* p )
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
		p_new_hpg = new hazard_ptr_group;   // ここから例外がスローされる可能性がある。
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
		if ( p_cur_chain->is_used() ) {
			hazard_ptr_group* p_cur_list = p_cur_chain;
			while ( p_cur_list != nullptr ) {
				for ( auto& e : *p_cur_list ) {
					// if hazard_ptr_group has a pointer that is same to p, p is hazard pointer
					if ( e.load( std::memory_order_acquire ) == p ) return true;
				}
				hazard_ptr_group* p_next_list = p_cur_list->ap_list_next_.load( std::memory_order_acquire );
				p_cur_list                    = p_next_list;
			}
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

#ifdef ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
#else
	alloc_chamber_statistics chk_ret = g_alloc_only_inst_for_hzrd_ptr_module.get_statistics();
	log_type                 lg      = log_type::INFO;
	if ( chk_ret.num_of_using_allocated_ > 0 ) {
		lg = log_type::ERR;
	}
	LogOutput( lg, chk_ret.print().c_str() );
#endif
#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
	LogOutput( log_type::DUMP, "Profile of hazard_ptr_group:" );
	LogOutput( log_type::DUMP, "\tcall count of hazard_ptr_group::try_assign() -> %zu", hazard_ptr_group::call_count_try_assign_.load() );
	LogOutput( log_type::DUMP, "\tloop count in hazard_ptr_group::try_assign() -> %zu", hazard_ptr_group::loop_count_in_try_assign_.load() );
	LogOutput( log_type::DUMP, "\tcall count of hazard_ptr<T>::get() -> %zu", call_count_hazard_ptr_get_.load() );
	LogOutput( log_type::DUMP, "\tloop count in hazard_ptr<T>::get() -> %zu", loop_count_in_hazard_ptr_get_.load() );
#endif
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
