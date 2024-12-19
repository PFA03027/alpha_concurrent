/**
 * @file alcc_mem_slotgroup.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief the definition of memory slot, memory slot group and memory slot list
 * @version 0.1
 * @date 2024-12-18
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCCURRENT_SRC_MEM_SLOTGROUP_HPP_
#define ALCONCCURRENT_SRC_MEM_SLOTGROUP_HPP_

#include <atomic>
#include <memory>
#include <type_traits>
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
#include <exception>
#endif

#include "alconcurrent/conf_logger.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

/**
 * @brief memory management type identifier
 *
 */
enum class mem_type : uint8_t {
	NON_USED     = 0,   //!< not assigned yet
	SMALL_MEM    = 1,   //!< memory allocation type is small memory type
	BIG_MEM      = 2,   //!< memory allocation type is big memory type
	OVER_BIG_MEM = 3,   //!< memory allocation type is over big memory type
};

struct memory_slot_group_list;

/**
 * @brief utility structure of unziped data of addr_w_mem_flag_ in allocated_mem_top
 *
 */
template <typename T>
struct unziped_allocation_info {
	T*       p_mgr_;
	mem_type mt_;
	bool     is_used_;
};

/**
 * @brief allocated memory information
 *
 */
struct allocated_mem_top {
	std::atomic<uintptr_t> addr_w_mem_flag_;   //!< 0..1: memory allocation type, 2: using/released flag, 3..63: address information
	unsigned char          data_[0];           //!< top for allocated memory

	static constexpr size_t min_alignment_size_ = sizeof( std::atomic<uintptr_t> );

	template <typename U>
	static constexpr allocated_mem_top* emplace_on_mem( unsigned char* p_mem, U* p_mgr_arg, mem_type mt_arg, bool is_used_arg ) noexcept
	{
		return new ( p_mem ) allocated_mem_top( p_mgr_arg, mt_arg, is_used_arg );
	}

	static allocated_mem_top* get_structure_addr( void* p ) noexcept
	{
		uintptr_t addr = reinterpret_cast<uintptr_t>( p );
		addr -= static_cast<uintptr_t>( sizeof( std::atomic<uintptr_t> ) );
		return reinterpret_cast<allocated_mem_top*>( addr );
	}

	template <typename U>
	unziped_allocation_info<U> load_allocation_info( std::memory_order mo = std::memory_order_acquire ) noexcept
	{
		uintptr_t                  addr_w_info = addr_w_mem_flag_.load( mo );
		unziped_allocation_info<U> ans;
		ans.p_mgr_   = reinterpret_cast<U*>( addr_w_info & ~( static_cast<uintptr_t>( 7L ) ) );
		ans.mt_      = static_cast<mem_type>( addr_w_info & static_cast<uintptr_t>( 3L ) );
		ans.is_used_ = ( ( addr_w_info & static_cast<uintptr_t>( 4L ) ) != 0 ) ? true : false;
		return ans;
	}

private:
	template <typename U>
	constexpr allocated_mem_top( U* p_mgr_arg, mem_type mt_arg, bool is_used_arg ) noexcept
	  : addr_w_mem_flag_( zip_allocation_info( p_mgr_arg, mt_arg, is_used_arg ) )
	  , data_ {}
	{
	}

	template <typename U>
	constexpr uintptr_t zip_allocation_info( U* p_mgr_arg, mem_type mt_arg, bool is_used_arg ) noexcept
	{
		return reinterpret_cast<uintptr_t>( p_mgr_arg ) |
		       static_cast<uintptr_t>( mt_arg ) |
		       ( is_used_arg ? static_cast<uintptr_t>( 4L ) : static_cast<uintptr_t>( 0 ) );
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

static_assert( std::is_trivially_destructible<allocated_mem_top>::value );

/**
 * @brief back trace of allocation and free points
 *
 */
struct btinfo_alloc_free {
	bt_info alloc_trace_;
	bt_info free_trace_;
};

/**
 * @brief manager structure for memory slot array
 *
 */
struct memory_slot_group {
	const uintptr_t                 magic_number_;         //!< magic number that indicates memory_slot_group
	memory_slot_group_list* const   p_list_mgr_;           //!< pointer to memory_slot_group_list
	const size_t                    one_slot_bytes_;       //!< bytes of one memory slot
	const size_t                    num_slots_;            //!< number of slot in this memory_slot_group
	unsigned char* const            p_slot_begin_;         //!< begin address of memory slot array
	unsigned char* const            p_slot_end_;           //!< end address of memory slot array
	std::atomic<memory_slot_group*> ap_next_group_;        //!< atomic pointer to next memory_slot_group as forward link list
	std::atomic<unsigned char*>     ap_unassigned_slot_;   //!< current unassinged address of memory slot
	unsigned char                   data_[0];              //!< buffer of back trace inforamtion array and memory slot array

	static constexpr uintptr_t magic_number_value_ = 0xABAB7878CDCD3434UL;

	static constexpr memory_slot_group* emplace_on_mem( unsigned char* p_mem, memory_slot_group_list* p_list_mgr_arg, size_t buffer_size, size_t requested_allocatable_bytes_of_a_slot ) noexcept
	{
		return new ( p_mem ) memory_slot_group( p_list_mgr_arg, buffer_size, calc_one_slot_size( requested_allocatable_bytes_of_a_slot ) );
	}

	static constexpr size_t calc_minimum_buffer_size( size_t requested_allocatable_bytes_of_a_slot ) noexcept;

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
	unsigned char* get_slot_pointer( size_t slot_idx ) noexcept
	{
		unsigned char* p_ans = p_slot_begin_ + ( slot_idx * one_slot_bytes_ );
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
		if ( p_slot_end_ <= p_ans ) {
			std::terminate();
		}
#endif
		return p_ans;
	}

	/**
	 * @brief assign a memory slot from unassigned slots
	 *
	 * @return pointer to a slot
	 * @retval nullptr because no unassigned slot, fail to assign a slot
	 * @retval non-nullptr success to assign a slot
	 */
	unsigned char* assign_new_slot( void ) noexcept
	{
		unsigned char* p_allocated_slot = ap_unassigned_slot_.load( std::memory_order_acquire );
		do {
			if ( p_slot_end_ <= p_allocated_slot ) {
				p_allocated_slot = nullptr;
				break;
			}
		} while ( !ap_unassigned_slot_.compare_exchange_strong( p_allocated_slot, p_allocated_slot + one_slot_bytes_, std::memory_order_acq_rel ) );
		return p_allocated_slot;
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

	static constexpr size_t calc_one_slot_size( size_t requested_allocatable_bytes_of_a_slot ) noexcept;
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
	size_t one_slot_bytes = sizeof( allocated_mem_top ) + requested_allocatable_bytes_of_a_slot + allocated_mem_top::min_alignment_size_ - 1;
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

/**
 * @brief manager structure for the list of memory_slot_group
 *
 */
struct memory_slot_group_list {
	const size_t                    allocatable_bytes_;                       //!< allocatable bytes per one slot
	const size_t                    max_buffer_bytes_of_memory_slot_group_;   //!< max bytes for one memory_slot_group
	std::atomic<size_t>             next_allocating_buffer_bytes_;            //!< allocating buffer size of next allocation for memory_slot_group
	std::atomic<memory_slot_group*> ap_head_memory_slot_group_;               //!< pointer to head memory_slot_group of memory_slot_group stack
	std::atomic<memory_slot_group*> ap_cur_assigning_memory_slot_group_;      //!< pointer to current slot allocating memory_slot_group
	std::atomic<allocated_mem_top*> ap_head_unused_memory_slot_stack_;        //!< pointer to head unused memory slot stack

	constexpr memory_slot_group_list(
		const size_t allocatable_bytes_arg,
		const size_t max_buffer_bytes_of_memory_slot_group_arg,
		const size_t init_buffer_bytes_of_memory_slot_group_arg ) noexcept
	  : allocatable_bytes_( allocatable_bytes_arg )
	  , max_buffer_bytes_of_memory_slot_group_( max_buffer_bytes_of_memory_slot_group_arg )
	  , next_allocating_buffer_bytes_( check_init_buffer_size( allocatable_bytes_arg, init_buffer_bytes_of_memory_slot_group_arg ) )
	  , ap_head_memory_slot_group_( nullptr )
	  , ap_cur_assigning_memory_slot_group_( nullptr )
	  , ap_head_unused_memory_slot_stack_( nullptr )
	{
	}

	void* allocate( size_t alignment = allocated_mem_top::min_alignment_size_ ) noexcept;
	void  deallocate( void* p ) noexcept;

private:
	static constexpr size_t check_init_buffer_size( size_t requested_allocatable_bytes_of_a_slot, size_t request_init_buffer_size ) noexcept
	{
		size_t min_size_val = memory_slot_group::calc_minimum_buffer_size( requested_allocatable_bytes_of_a_slot );
		size_t ans          = ( min_size_val <= request_init_buffer_size ) ? request_init_buffer_size : min_size_val;
		return ans;
	}
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
