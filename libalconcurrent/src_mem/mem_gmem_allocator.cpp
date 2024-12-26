/**
 * @file mem_gmem_allocator.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-22
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "alconcurrent/lf_mem_alloc.hpp"
#include "mem_big_memory_slot.hpp"
#include "mem_retrieved_slot_array_mgr.hpp"
#include "mem_small_memory_slot.hpp"

namespace alpha {
namespace concurrent {

internal::memory_slot_group_list g_memory_slot_group_list_array[] = {
	{ 8, 1024 * 4, 1024 * 1024, 0 },
	{ 16, 1024 * 4, 1024 * 1024, 1 },
	{ 32, 1024 * 4, 1024 * 1024, 2 },
	{ 64, 1024 * 4, 1024 * 1024, 3 },
	{ 128, 1024 * 4, 1024 * 1024, 4 },
	{ 256, 1024 * 4, 1024 * 1024, 5 },
	{ 512, 1024 * 8, 1024 * 1024 * 2, 6 },
	{ 1024, 1024 * 16, 1024 * 1024 * 4, 7 },
	{ 2048, 1024 * 32, 1024 * 1024 * 4, 8 },
	{ 4096, 1024 * 64, 1024 * 1024 * 4, 9 },
	{ 8192, 1024 * 128, 1024 * 1024 * 4, 10 },
	{ 16384, 1024 * 256, 1024 * 1024 * 4, 11 },
	{ 32768, 1024 * 512, 1024 * 1024 * 4, 12 },
	{ 65536, 1024 * 1024, 1024 * 1024 * 4, 13 },
	{ 131072, 1024 * 1024 * 2, 1024 * 1024 * 4, 14 } };

internal::big_memory_slot_list g_big_memory_slot_list;

/*!
 * @brief	allocate memory
 *
 * This I/F allocates a memory from a global general_mem_allocator instance. @n
 * The allocated memory must free by gmem_deallocate().
 *
 * @note
 * This uses default_param_array and num_of_default_param_array as initial allocation parameter
 *
 * @exception
 * If req_align is not power of 2, throw std::logic_error.
 */
void* x_gmem_allocate(
	size_t n,          //!< [in] memory size to allocate
	size_t req_align   //!< [in] requested align size. req_align should be the power of 2
)
{
	size_t needed_bytes = n;
	if ( req_align > internal::allocated_mem_top::min_alignment_size_ ) {
		needed_bytes += req_align - 1;
		if ( needed_bytes < n ) {
			internal::LogOutput( log_type::ERR, "overflow. requested bytes = %zu, requested align = %zu", n, req_align );
			return nullptr;
		}
	}

	const size_t array_count = sizeof( g_memory_slot_group_list_array ) / sizeof( g_memory_slot_group_list_array[0] );
	for ( size_t i = 0; i < array_count; ++i ) {
		if ( needed_bytes <= g_memory_slot_group_list_array[i].allocatable_bytes_ ) {
			internal::slot_link_info* p_slot = g_memory_slot_group_list_array[i].allocate();
			if ( p_slot == nullptr ) {
				g_memory_slot_group_list_array[i].request_allocate_memory_slot_group();
				p_slot = g_memory_slot_group_list_array[i].allocate();
			}
			if ( p_slot != nullptr ) {
				internal::allocated_mem_top* p_ans = p_slot->get_aligned_allocated_mem_top(
					req_align,
					n,
					internal::memory_slot_group::calc_one_slot_size( g_memory_slot_group_list_array[i].allocatable_bytes_ ) );
				return reinterpret_cast<void*>( p_ans->data_ );
			}
		}
	}

	internal::big_memory_slot* p_big_slot = g_big_memory_slot_list.reuse_allocate( needed_bytes );
	if ( p_big_slot == nullptr ) {
		p_big_slot = g_big_memory_slot_list.allocate_newly( needed_bytes );
	}
	if ( p_big_slot != nullptr ) {
		internal::allocated_mem_top* p_ans = p_big_slot->get_aligned_allocated_mem_top( req_align, n );
		return reinterpret_cast<void*>( p_ans->data_ );
	}

	return nullptr;
}

/*!
 * @brief	deallocate memory
 *
 * This I/F free a memory area that is allocated by gmem_allocate().
 *
 * @note
 * If p_mem is not a memory that is not allocated by gmem_allocate(), this I/F will try to free by calling free().
 */
bool x_gmem_deallocate(
	void* p_mem   //!< [in] pointer to free.
)
{
	if ( p_mem == nullptr ) {
		return false;
	}

	bool                         ans           = false;
	internal::allocated_mem_top* p_top         = internal::allocated_mem_top::get_structure_addr( p_mem );
	auto                         slot_info_tmp = p_top->load_allocation_info<void>();
	if ( slot_info_tmp.mt_ == internal::mem_type::SMALL_MEM ) {
		auto slot_info = p_top->load_allocation_info<internal::memory_slot_group>();
		auto idx       = slot_info.p_mgr_->get_slot_idx( p_mem );
		if ( idx < 0 ) {
			internal::LogOutput( log_type::ERR, "invalid slot index." );
			return false;
		}
		internal::slot_link_info* p_slot = slot_info.p_mgr_->get_slot_pointer( static_cast<size_t>( idx ) );
		if ( &( p_slot->link_to_memory_slot_group_ ) != p_top ) {
			p_top->fetch_set( false );
		}
		internal::memory_slot_group_list* p_mgr = slot_info.p_mgr_->p_list_mgr_;
		ans                                     = p_mgr->deallocate( p_slot );
	} else if ( ( slot_info_tmp.mt_ == internal::mem_type::BIG_MEM ) || ( slot_info_tmp.mt_ == internal::mem_type::OVER_BIG_MEM ) ) {
		auto slot_info = p_top->load_allocation_info<internal::big_memory_slot>();
		if ( &( slot_info.p_mgr_->link_to_big_memory_slot_ ) != p_top ) {
			p_top->fetch_set( false );
		}
		ans = g_big_memory_slot_list.deallocate( slot_info.p_mgr_ );
	} else {
		internal::LogOutput( log_type::ERR, "unknown slot type." );
		return false;
	}

	return ans;
}

}   // namespace concurrent
}   // namespace alpha