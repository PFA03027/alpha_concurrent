/**
 * @file lf_mem_alloc_slot_array.cpp
 * @author Teruaki Ata <PFA03027@nifty.com>
 * @brief implemenation of slot array and slot array manager
 * @version 0.1
 * @date 2023-08-06
 *
 * @copyright Copyright (c) 2023 Teruaki Ata <PFA03027@nifty.com>
 *
 */

#include "lf_mem_alloc_slot_array.hpp"
#include "mmap_allocator.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

#ifdef PERFORMANCE_ANALYSIS_LOG1
std::atomic<size_t> call_count_push_to_free_node_stack( 0 );
std::atomic<size_t> spin_count_push_to_free_node_stack( 0 );
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////

struct slot_array_mgr_alloc_mem_addr_and_size {
	void*  p_alloc_;
	size_t alloc_size_;
};

static constexpr uintptr_t calc_diff( void )
{
	if ( sizeof( size_t ) <= sizeof( uintptr_t ) ) {
		return static_cast<uintptr_t>( sizeof( uintptr_t ) );
	} else {
		size_t mx      = sizeof( size_t ) / sizeof( uintptr_t );
		size_t rx      = sizeof( size_t ) % sizeof( uintptr_t );
		size_t ans_cnd = mx * sizeof( uintptr_t ) + ( ( rx == 0 ) ? 0 : sizeof( uintptr_t ) );
		return static_cast<uintptr_t>( ans_cnd );
	}
}

static inline slot_array_mgr_alloc_mem_addr_and_size calc_addr_and_size( void* p )
{
	uintptr_t addr_rm_mem = reinterpret_cast<uintptr_t>( p ) - calc_diff();
	size_t    alloc_size  = *( reinterpret_cast<size_t*>( addr_rm_mem ) );
	return slot_array_mgr_alloc_mem_addr_and_size { reinterpret_cast<void*>( addr_rm_mem ), alloc_size };
}

static inline void dealloc_allocated_area_of_slot_array_mgr( void* p )
{
	if ( p == nullptr ) return;

	auto alloc_area_info = calc_addr_and_size( p );

	int dealloc_ret = deallocate_by_munmap( alloc_area_info.p_alloc_, alloc_area_info.alloc_size_ );
	if ( dealloc_ret != 0 ) {
		LogOutput( log_type::ERR, "fail deallocate_by_munmap(%p, %zu)", alloc_area_info.p_alloc_, alloc_area_info.alloc_size_ );
	}
}

// usual delete...(2)
void slot_array_mgr::operator delete( void* p_mem ) noexcept
{
	dealloc_allocated_area_of_slot_array_mgr( p_mem );
}

// placement new    可変長部分の領域も確保するnew operator
void* slot_array_mgr::operator new( std::size_t n_of_slot_array_mgr, size_t num_of_slots_, size_t expected_alloc_n_per_slot )
{
	size_t total_size = n_of_slot_array_mgr;
	total_size += sizeof( slot_header_of_array[num_of_slots_] );
	total_size += calc_total_slot_container_array_bytes( num_of_slots_, expected_alloc_n_per_slot );
	auto alloc_ret = allocate_by_mmap( total_size, default_slot_alignsize );
	if ( alloc_ret.p_allocated_addr_ == nullptr ) {
		internal::LogOutput( log_type::ERR, "fail allocate memory by allocate_by_mmap(%zu, %zu)", total_size, default_slot_alignsize );
		throw std::bad_alloc();
	}

	*( reinterpret_cast<size_t*>( alloc_ret.p_allocated_addr_ ) ) = alloc_ret.allocated_size_;

	uintptr_t ans_addr = reinterpret_cast<uintptr_t>( alloc_ret.p_allocated_addr_ ) + calc_diff();
	return reinterpret_cast<void*>( ans_addr );
}

void slot_array_mgr::operator delete( void* p, size_t num_of_slots_, size_t expected_alloc_n_per_slot ) noexcept
{
	// 配置newに対応するdelete。
	dealloc_allocated_area_of_slot_array_mgr( p );
}

slot_array_mgr::slot_array_mgr( chunk_header_multi_slot* p_owner, size_t num_of_slots, size_t n )
  : num_of_slots_( num_of_slots )
  , expected_n_per_slot_( n )
  , slot_container_size_of_this_( calc_one_slot_container_bytes( n ) )
  , p_owner_chunk_header_( p_owner )
  , allocator_( true, 4 * 1024 )
  , free_slots_storage_( &allocator_ )
  , p_slot_container_top( reinterpret_cast<slot_container*>( &( slot_header_array_[num_of_slots_] ) ) )
  , slot_header_array_ {}
{
	if ( num_of_slots_ == 0 ) {
		return;
	}

	slot_header_of_array* p_pre_slot_header_of_array = nullptr;
	slot_header_of_array* p_cur_slot                 = get_pointer_of_slot( num_of_slots_ - 1 );
	for ( size_t i = 0; i < num_of_slots_; i++ ) {
		p_pre_slot_header_of_array = new ( p_cur_slot ) slot_header_of_array( reinterpret_cast<void*>( this ), p_pre_slot_header_of_array );
		p_cur_slot                 = unchk_get_pre_pointer_of_slot( p_cur_slot );
	}
	free_slots_storage_.unchk_push_stack_list_to_head( p_pre_slot_header_of_array );
}

bool_size_t slot_array_mgr::get_slot_idx_from_slot_header_of_array( slot_header_of_array* p_slot_header )
{
	uintptr_t byte_offset = reinterpret_cast<uintptr_t>( p_slot_header ) - reinterpret_cast<uintptr_t>( slot_header_array_ );
	size_t    ans_idx     = static_cast<size_t>( byte_offset / sizeof( slot_header_of_array ) );

	if ( reinterpret_cast<uintptr_t>( p_slot_header ) < reinterpret_cast<uintptr_t>( slot_header_array_ ) ) {
		return bool_size_t { false, ans_idx };
	}
	if ( ans_idx > num_of_slots_ ) {
		// char buff[128];
		// snprintf( buff, 128, "too big idx: calculated idx is %zu, max_slot is %zu", ans_idx, num_of_slots_ );
		return bool_size_t { false, ans_idx };
	}

	return bool_size_t { true, ans_idx };
}

void slot_array_mgr::dump( int indent )
{
	auto alloc_area_info = calc_addr_and_size( this );

	std::string indent_str;
	for ( int i = 0; i < indent; i++ ) {
		indent_str += "\t";
	}
	internal::LogOutput( log_type::DUMP, "%sslot_array_mgr(%p)={alloc_size_=%zu,num_of_slots_=%zu,expected_n_per_slot_=%zu,slot_container_size_of_this_=%zu,p_owner_chunk_header_=%p,p_slot_container_top=%p",
	                     indent_str.c_str(),
	                     this, alloc_area_info.alloc_size_, num_of_slots_, expected_n_per_slot_, slot_container_size_of_this_, p_owner_chunk_header_.load(), p_slot_container_top );
	auto total_statistics = allocator_.get_statistics();
	internal::LogOutput( log_type::DUMP, "%s%s", indent_str.c_str(), total_statistics.print().c_str() );
	internal::LogOutput( log_type::DUMP, "%s}", indent_str.c_str() );
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
