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

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
