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

/**
 * @brief retrieved slots kept by stack
 *
 * とりあえずの、仮実装。後で、スレッドローカル変数を使って、ロックフリー化する。
 */
struct retrieved_big_memory_slots_mgr {
	hazard_ptr_handler<big_memory_slot> hph_head_unused_memory_slot_stack_;   //!< pointer to head unused memory slot stack
	mutable std::mutex                  mtx_;
	big_memory_slot*                    p_head_unused_memory_slot_stack_in_hazard_;   //!< pointer to head unused memory slot stack
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	std::atomic<size_t> count_in_not_hazard_;
	std::atomic<size_t> count_in_hazard_;
#endif

	constexpr retrieved_big_memory_slots_mgr( void ) noexcept
	  : hph_head_unused_memory_slot_stack_( nullptr )
	  , mtx_()
	  , p_head_unused_memory_slot_stack_in_hazard_( nullptr )
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	  , count_in_not_hazard_( 0 )
	  , count_in_hazard_( 0 )
#endif
	{
	}

	void             retrieve( big_memory_slot* p ) noexcept;
	big_memory_slot* request_reuse( size_t requested_size ) noexcept;

private:
	using hazard_pointer = typename hazard_ptr_handler<big_memory_slot>::hazard_pointer;

	void             retrieve_impl( big_memory_slot* p ) noexcept;
	big_memory_slot* request_reuse_impl( void ) noexcept;
};
static_assert( std::is_trivially_destructible<retrieved_big_memory_slots_mgr>::value );

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
