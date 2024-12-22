/**
 * @file mem_big_memory_slot.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-22
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCCURRENT_SRC_MEM_BIG_MEMORY_SLOT_HPP_
#define ALCONCCURRENT_SRC_MEM_BIG_MEMORY_SLOT_HPP_

#include <atomic>
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

/**
 * @brief big memory slot
 *
 */
struct big_memory_slot {
	const uintptr_t               magic_number_;   //!< magic number that indicates big_memory_slot
	const size_t                  buffer_size_;    //!< size of buffer
	std::atomic<big_memory_slot*> ap_slot_next_;   //!< pointer to next big_memory_slot
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	btinfo_alloc_free btinfo_;   //!< back trace information
#endif
	allocated_mem_top link_to_big_memory_slot_;
	union {
		unsigned char    data_[0];                //!< buffer of big memory slot
		big_memory_slot* p_temprary_link_next_;   // for temporary link
	};

	static constexpr uintptr_t magic_number_value_ = 0x3434ABAB7878CDCDUL;

	static constexpr big_memory_slot* emplace_on_mem( void* p_mem, mem_type mt_arg, size_t buffer_size ) noexcept
	{
		return new ( p_mem ) big_memory_slot( mt_arg, buffer_size );
	}

	big_memory_slot* check_validity_to_ownwer_and_get( void ) const noexcept;
	constexpr size_t max_allocatable_size( void ) const noexcept
	{
		return buffer_size_ - ( sizeof( big_memory_slot ) - sizeof( big_memory_slot* ) );
	}

	static constexpr size_t calc_minimum_buffer_size( size_t requested_allocatable_size ) noexcept
	{
		return requested_allocatable_size + ( sizeof( big_memory_slot ) - sizeof( big_memory_slot* ) ) + 1;
	}

private:
	constexpr big_memory_slot( mem_type mt_arg, size_t buffer_size ) noexcept
	  : magic_number_( magic_number_value_ )
	  , buffer_size_( buffer_size )
	  , ap_slot_next_( nullptr )
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	  , btinfo_ {}
#endif
	  , link_to_big_memory_slot_( this, mt_arg, true )
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

using retrieved_big_slots_mgr = retrieved_slots_mgr_impl<big_memory_slot>;
static_assert( std::is_trivially_destructible<retrieved_big_slots_mgr>::value );

/**
 * @brief manager structure for the list of big_memory_slot
 *
 */
struct big_memory_slot_list {
	retrieved_big_slots_mgr unused_retrieved_slots_mgr_;      //!< manager for retrieved slots
	std::atomic<size_t>     unused_retrieved_memory_bytes_;   //!< count of slots in hazard

	static constexpr size_t defualt_limit_bytes_of_unused_retrieved_memory_ = 1024 * 1024 * 4;   // 4MB
	static size_t           limit_bytes_of_unused_retrieved_memory_;                             //!< limit bytes of unused retrieved memory for cache
	static size_t           too_big_memory_slot_buffer_size_threshold_;                          //!< threshold of buffer size to be too big memory slot

	constexpr big_memory_slot_list( void ) noexcept
	  : unused_retrieved_slots_mgr_()
	  , unused_retrieved_memory_bytes_( 0 )
	{
	}

	big_memory_slot* reuse_allocate( size_t requested_allocatable_size ) noexcept;
	void             deallocate( big_memory_slot* p ) noexcept;

	/**
	 * @brief request to allocate a memory_slot_group and push it to the head of memory_slot_group stack
	 *
	 */
	big_memory_slot* allocate_newly( size_t requested_allocatable_size ) noexcept;

	/**
	 * @brief free all memory_slot_group
	 *
	 */
	void clear_for_test( void ) noexcept;
};
static_assert( std::is_trivially_destructible<big_memory_slot_list>::value );

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
