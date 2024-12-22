/**
 * @file mem_small_memory_slot.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-22
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCCURRENT_SRC_MEM_SMALL_MEMORY_SLOT_HPP_
#define ALCONCCURRENT_SRC_MEM_SMALL_MEMORY_SLOT_HPP_

#include <atomic>
#include <mutex>
#include <type_traits>
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
#include <exception>
#endif

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/hazard_ptr.hpp"

#include "mem_common.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

struct memory_slot_group;

struct slot_link_info {
	std::atomic<slot_link_info*> ap_slot_next_;
	allocated_mem_top            link_to_memory_slot_group_;
	union {
		unsigned char   data_[0];
		slot_link_info* p_temprary_link_next_;
	};

	static constexpr slot_link_info* emplace_on_mem( unsigned char* p_mem, memory_slot_group* p_owner ) noexcept
	{
		return new ( p_mem ) slot_link_info( p_owner );
	}

	memory_slot_group* check_validity_to_ownwer_and_get( void ) noexcept;

	/**
	 * @brief Get the aligned allocated mem top object
	 *
	 * @param align_bytes alignment bytes. This value should be power of 2.
	 * @return allocated_mem_top*
	 */
	allocated_mem_top* get_aligned_allocated_mem_top( size_t align_bytes, size_t requested_allocation_size, size_t slot_size ) noexcept
	{
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
		if ( !is_power_of_2( align_bytes ) ) {
			std::terminate();
		}
#endif
		uintptr_t addr = reinterpret_cast<uintptr_t>( data_ );
		addr           = ( addr + ( align_bytes - 1 ) ) & ( ~( align_bytes - 1 ) );
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
		uintptr_t end_addr = reinterpret_cast<uintptr_t>( data_ ) + slot_size;
		if ( end_addr < ( addr + requested_allocation_size ) ) {
			std::terminate();
		}
#endif
		if ( addr == reinterpret_cast<uintptr_t>( data_ ) ) {
			return &link_to_memory_slot_group_;
		}
		return allocated_mem_top::emplace_on_mem( reinterpret_cast<unsigned char*>( addr ), link_to_memory_slot_group_ );
	}

private:
	constexpr slot_link_info( memory_slot_group* p_owner ) noexcept
	  : ap_slot_next_( nullptr )
	  , link_to_memory_slot_group_( p_owner, mem_type::SMALL_MEM, true )
	  , data_ {}
	{
	}

	static constexpr void* operator new( std::size_t s, void* p ) noexcept   // placement new
	{
		return p;
	}
	static constexpr void operator delete( void* ptr, void* ) noexcept   // placement delete
	{
		return;
	}
};

struct memory_slot_group_list;

/**
 * @brief manager structure for memory slot array
 *
 */
struct memory_slot_group {
	const uintptr_t                 magic_number_;         //!< magic number that indicates memory_slot_group
	const size_t                    buffer_size_;          //!< size of buffer
	memory_slot_group_list* const   p_list_mgr_;           //!< pointer to memory_slot_group_list
	const size_t                    one_slot_bytes_;       //!< bytes of one memory slot
	const size_t                    num_slots_;            //!< number of slot in this memory_slot_group
	unsigned char* const            p_slot_begin_;         //!< begin address of memory slot array
	unsigned char* const            p_slot_end_;           //!< end address of memory slot array
	std::atomic<memory_slot_group*> ap_next_group_;        //!< atomic pointer to next memory_slot_group as forward link list
	std::atomic<unsigned char*>     ap_unassigned_slot_;   //!< current unassinged address of memory slot
	unsigned char                   data_[0];              //!< buffer of back trace inforamtion array and memory slot array

	static constexpr uintptr_t magic_number_value_ = 0xABAB7878CDCD3434UL;
	static constexpr size_t    buffer_zone_bytes_  = 1UL;

	static constexpr memory_slot_group* emplace_on_mem( void* p_mem, memory_slot_group_list* p_list_mgr_arg, size_t buffer_size, size_t requested_allocatable_bytes_of_a_slot ) noexcept
	{
		return new ( p_mem ) memory_slot_group( p_list_mgr_arg, buffer_size, calc_one_slot_size( requested_allocatable_bytes_of_a_slot ) );
	}

	static constexpr size_t calc_minimum_buffer_size( size_t requested_allocatable_bytes_of_a_slot ) noexcept;
	static constexpr size_t calc_one_slot_size( size_t requested_allocatable_bytes_of_a_slot ) noexcept;

	ssize_t get_slot_idx( void* p ) const noexcept
	{
		unsigned char* const p_tmp = reinterpret_cast<unsigned char*>( p );
		if ( p_tmp < p_slot_begin_ ) return -1;
		if ( p_slot_end_ <= p_tmp ) return -1;

		uintptr_t addr_diff = reinterpret_cast<uintptr_t>( p_tmp ) - reinterpret_cast<uintptr_t>( p_slot_begin_ );
		uintptr_t slot_idx  = addr_diff / one_slot_bytes_;
		return static_cast<ssize_t>( slot_idx );
	}

	/**
	 * @brief Get the slot pointer object
	 *
	 * @param slot_idx index of a slot
	 * @return pointer to a slot that index is slot_idx.
	 *
	 * @warning this I/F does not check the valid range for slot_idx.
	 */
	slot_link_info* get_slot_pointer( size_t slot_idx ) noexcept
	{
		unsigned char* p_ans = p_slot_begin_ + ( slot_idx * one_slot_bytes_ );
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
		if ( p_slot_end_ <= p_ans ) {
			std::terminate();
		}
#endif
		return reinterpret_cast<slot_link_info*>( p_ans );
	}

	/**
	 * @brief assign a memory slot from unassigned slots
	 *
	 * @return pointer to a slot
	 * @retval nullptr because no unassigned slot, fail to assign a slot
	 * @retval non-nullptr success to assign a slot
	 */
	slot_link_info* assign_new_slot( void ) noexcept
	{
		unsigned char* p_allocated_slot = ap_unassigned_slot_.load( std::memory_order_acquire );
		do {
			if ( p_slot_end_ <= p_allocated_slot ) {
				return nullptr;
			}
		} while ( !ap_unassigned_slot_.compare_exchange_strong( p_allocated_slot, p_allocated_slot + one_slot_bytes_, std::memory_order_acq_rel ) );

#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
		btinfo_alloc_free& cur_btinfo = get_btinfo( get_slot_idx( p_allocated_slot ) );
		cur_btinfo.alloc_trace_       = bt_info::record_backtrace();
		cur_btinfo.free_trace_.invalidate();
#endif

		return slot_link_info::emplace_on_mem( p_allocated_slot, this );
	}

	/**
	 * @brief if all slots are assigned already, return true
	 *
	 * @return true all slots are assigned already
	 * @return false there is non-assigned slots
	 */
	bool is_assigned_all_slots( void ) const noexcept
	{
		return p_slot_end_ <= ap_unassigned_slot_.load( std::memory_order_acquire );
	}

#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	btinfo_alloc_free& get_btinfo( ssize_t slot_idx ) noexcept
	{
		return calc_begin_of_btinfo( data_ )[slot_idx];
	}
#endif

private:
	constexpr memory_slot_group( memory_slot_group_list* p_list_mgr_arg, size_t buffer_size, size_t calced_one_slot_bytes_arg ) noexcept
	  : magic_number_( magic_number_value_ )
	  , buffer_size_( buffer_size )
	  , p_list_mgr_( p_list_mgr_arg )
	  , one_slot_bytes_( calced_one_slot_bytes_arg )
	  , num_slots_( calc_number_of_slots( buffer_size, one_slot_bytes_ ) )
	  , p_slot_begin_( calc_begin_of_slots( data_, num_slots_, one_slot_bytes_ ) )
	  , p_slot_end_( calc_end_of_slots( data_, num_slots_, one_slot_bytes_ ) )
	  , ap_next_group_( nullptr )
	  , ap_unassigned_slot_( p_slot_begin_ )
	  , data_ {}
	{
	}

	static constexpr size_t calc_number_of_slots( size_t buffer_size, size_t one_slot_bytes_arg ) noexcept;
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	static constexpr btinfo_alloc_free* calc_begin_of_btinfo( unsigned char* data_top ) noexcept;
	static constexpr btinfo_alloc_free* calc_end_of_btinfo( unsigned char* data_top, size_t num_of_slots ) noexcept;
#endif
	static constexpr unsigned char* calc_begin_of_slots( unsigned char* data_top, size_t num_of_slots, size_t bytes_of_slot ) noexcept;
	static constexpr unsigned char* calc_end_of_slots( unsigned char* data_top, size_t num_of_slots, size_t bytes_of_slot ) noexcept;

	static constexpr void* operator new( std::size_t s, void* p ) noexcept   // placement new
	{
		return p;
	}
	static constexpr void operator delete( void* ptr, void* ) noexcept   // placement delete
	{
		return;
	}
};

constexpr size_t memory_slot_group::calc_one_slot_size( size_t requested_allocatable_bytes_of_a_slot ) noexcept
{
	size_t one_slot_bytes = ( requested_allocatable_bytes_of_a_slot < allocated_mem_top::min_alignment_size_ ) ? allocated_mem_top::min_alignment_size_ : requested_allocatable_bytes_of_a_slot;
	one_slot_bytes += sizeof( slot_link_info ) + buffer_zone_bytes_ + allocated_mem_top::min_alignment_size_ - 1;
	one_slot_bytes /= allocated_mem_top::min_alignment_size_;
	one_slot_bytes *= allocated_mem_top::min_alignment_size_;
	return one_slot_bytes;
}

constexpr size_t memory_slot_group::calc_minimum_buffer_size( size_t requested_allocatable_bytes_of_a_slot ) noexcept
{
	size_t ans = sizeof( memory_slot_group );
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	size_t btinfo_size = ( sizeof( btinfo_alloc_free ) + allocated_mem_top::min_alignment_size_ - 1 ) / allocated_mem_top::min_alignment_size_;
	btinfo_size *= allocated_mem_top::min_alignment_size_;
	ans += btinfo_size;
#endif
	ans += calc_one_slot_size( requested_allocatable_bytes_of_a_slot );
	return ans;
}

constexpr size_t memory_slot_group::calc_number_of_slots( size_t buffer_size, size_t one_slot_bytes_arg ) noexcept
{
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
	if ( buffer_size < ( sizeof( memory_slot_group ) + one_slot_bytes_arg
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	                     + sizeof( btinfo_alloc_free )
#endif
	                         ) ) {
		std::terminate();
	}
#endif

	size_t total_buff_for_slots = buffer_size - sizeof( memory_slot_group );
	size_t bytes_for_one_slot   = one_slot_bytes_arg;
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	bytes_for_one_slot += sizeof( btinfo_alloc_free );
#endif
	bytes_for_one_slot = ( bytes_for_one_slot + allocated_mem_top::min_alignment_size_ - 1 ) / allocated_mem_top::min_alignment_size_;
	bytes_for_one_slot *= allocated_mem_top::min_alignment_size_;

	size_t ans = total_buff_for_slots / bytes_for_one_slot;
	return ans;
}

#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
constexpr btinfo_alloc_free* memory_slot_group::calc_begin_of_btinfo( unsigned char* data_top ) noexcept
{
	uintptr_t data_top_addr = reinterpret_cast<uintptr_t>( data_top );
	return reinterpret_cast<btinfo_alloc_free*>( data_top_addr );
}
constexpr btinfo_alloc_free* memory_slot_group::calc_end_of_btinfo( unsigned char* data_top, size_t num_of_slots ) noexcept
{
	return &( calc_begin_of_btinfo( data_top )[num_of_slots] );
}
#endif

constexpr unsigned char* memory_slot_group::calc_begin_of_slots( unsigned char* data_top, size_t num_of_slots, size_t bytes_of_slot ) noexcept
{
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	uintptr_t data_top_addr = reinterpret_cast<uintptr_t>( calc_end_of_btinfo( data_top, num_of_slots ) );
#else
	uintptr_t data_top_addr = reinterpret_cast<uintptr_t>( data_top );
#endif
	data_top_addr = ( data_top_addr + allocated_mem_top::min_alignment_size_ - 1 ) / allocated_mem_top::min_alignment_size_;
	data_top_addr *= allocated_mem_top::min_alignment_size_;
	return reinterpret_cast<unsigned char*>( data_top_addr );
}
constexpr unsigned char* memory_slot_group::calc_end_of_slots( unsigned char* data_top, size_t num_of_slots, size_t bytes_of_slot ) noexcept
{
	unsigned char* p_ans = calc_begin_of_slots( data_top, num_of_slots, bytes_of_slot );
	p_ans += num_of_slots * bytes_of_slot;   // TODO: how to check overflow ?
	return p_ans;
}

using retrieved_small_slots_mgr = retrieved_slots_mgr_impl<slot_link_info>;
static_assert( std::is_trivially_destructible<retrieved_small_slots_mgr>::value );

/**
 * @brief manager structure for the list of memory_slot_group
 *
 */
struct memory_slot_group_list {
	const size_t                    allocatable_bytes_;                       //!< allocatable bytes per one slot
	const size_t                    limit_bytes_for_one_memory_slot_group_;   //!< max bytes for one memory_slot_group
	std::atomic<size_t>             next_allocating_buffer_bytes_;            //!< allocating buffer size of next allocation for memory_slot_group
	std::atomic<memory_slot_group*> ap_head_memory_slot_group_;               //!< pointer to head memory_slot_group of memory_slot_group stack
	std::atomic<memory_slot_group*> ap_cur_assigning_memory_slot_group_;      //!< pointer to current slot allocating memory_slot_group
	retrieved_small_slots_mgr       unused_retrieved_slots_mgr_;              //!< manager for retrieved slots

	constexpr memory_slot_group_list(
		const size_t allocatable_bytes_arg,                       //!< [in] max allocatable bytes by allocation
		const size_t limit_bytes_for_one_memory_slot_group_arg,   //!< [in] limitation to allocate one memory_slot_group
		const size_t init_buffer_bytes_of_memory_slot_group_arg   //!< [in] buffer size of one memory_slot_group when 1st allocation
		) noexcept
	  : allocatable_bytes_( allocatable_bytes_arg )
	  , limit_bytes_for_one_memory_slot_group_( limit_bytes_for_one_memory_slot_group_arg )
	  , next_allocating_buffer_bytes_( check_init_buffer_size( allocatable_bytes_arg, init_buffer_bytes_of_memory_slot_group_arg ) )
	  , ap_head_memory_slot_group_( nullptr )
	  , ap_cur_assigning_memory_slot_group_( nullptr )
	  , unused_retrieved_slots_mgr_()
	{
	}

	slot_link_info* allocate( void ) noexcept;
	bool            deallocate( slot_link_info* p ) noexcept;

	/**
	 * @brief request to allocate a memory_slot_group and push it to the head of memory_slot_group stack
	 *
	 */
	void request_allocate_memory_slot_group( void ) noexcept;

	/**
	 * @brief free all memory_slot_group
	 *
	 */
	void clear_for_test( void ) noexcept;

private:
	static constexpr size_t check_init_buffer_size( size_t requested_allocatable_bytes_of_a_slot, size_t request_init_buffer_size ) noexcept
	{
		size_t min_size_val = memory_slot_group::calc_minimum_buffer_size( requested_allocatable_bytes_of_a_slot );
		size_t ans          = ( min_size_val <= request_init_buffer_size ) ? request_init_buffer_size : min_size_val;
		return ans;
	}

	static constexpr size_t clac_next_expected_buffer_size( size_t cur_size, size_t limit_size ) noexcept
	{
		size_t ans = cur_size * 2;
		if ( limit_size < ans ) {
			ans = limit_size;
		}
		return ans;
	}

	slot_link_info* allocate_impl( void ) noexcept;
};

static_assert( std::is_trivially_destructible<memory_slot_group_list>::value );

inline slot_link_info* memory_slot_group_list::allocate( void ) noexcept
{
	slot_link_info* p_ans = allocate_impl();
	if ( p_ans == nullptr ) {
		return p_ans;
	}

#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	auto p_slot_owner = p_ans->check_validity_to_ownwer_and_get();
	if ( p_slot_owner == nullptr ) {
		LogOutput( log_type::WARN, "retrieved_slots_mgr_impl<SLOT_T>::request_reuse() invalid SLOT_T" );
		return nullptr;
	}
	btinfo_alloc_free& cur_btinfo = p_slot_owner->get_btinfo( p_slot_owner->get_slot_idx( p_ans ) );
	cur_btinfo.alloc_trace_       = bt_info::record_backtrace();
	cur_btinfo.free_trace_.invalidate();
#endif
	return p_ans;
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
