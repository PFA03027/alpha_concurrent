/**
 * @file alloc_only_allocator.hpp
 * @author Teruaki Ata <PFA03027@nifty.com>
 * @brief I/F header file of allocation only allocator
 * @version 0.1
 * @date 2023-07-02
 *
 * @copyright Copyright (c) 2023 Teruaki Ata <PFA03027@nifty.com>
 *
 */

#ifndef ALLOC_ONLY_ALLOCATOR_HPP_
#define ALLOC_ONLY_ALLOCATOR_HPP_

#include <cstdint>
#include <cstdlib>

#include <atomic>

#include "alconcurrent/conf_logger.hpp"

namespace alpha {
namespace concurrent {

////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace internal {

// external I/F
constexpr size_t default_align_size = 32;

/**
 * @brief allocate memory that requester side does not free
 *
 * memory allocated by this I/F could not free.
 *
 * @param req_size memory size to allocate
 * @param req_align allocated memory address alignment
 * @return void* pointer to the allocated memory
 */
void* allocating_only( size_t req_size, size_t req_align = default_align_size );

/**
 * @brief to detect unexpected deallocation calling
 *
 * normally nothing to do
 * If the library compile with ALCONCURRENT_CONF_DETECT_UNEXPECTED_DEALLOC_CALLING, this function throw std::runtime_error.
 *
 * @param p_mem
 */
void allocating_only_deallocate( void* p_mem );

// configuration value
constexpr size_t conf_pre_mmap_size = 1024 * 1024;

////////////////////////////////////////////////////////////////////////////////////////////////////////
// internal I/F
class alloc_chamber;

class alloc_chamber_head {
public:
	static alloc_chamber_head& get_inst( void );

	void push_alloc_mem( void* p_alloced_mem, size_t allocated_size );

	void* allocate( size_t req_size, size_t req_align );

	void dump_to_log( log_type lt, char c, int id );

private:
	constexpr alloc_chamber_head( void )
	  : head_( nullptr )
	{
	}

	std::atomic<alloc_chamber*> head_;   //!< alloc_chamberのスタックリスト上のheadのalloc_chamber

	static alloc_chamber_head          singleton_;
	static thread_local alloc_chamber* p_forcusing_chamber_;
};

inline alloc_chamber_head& alloc_chamber_head::get_inst( void )
{
	return singleton_;
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif   // ALLOC_ONLY_ALLOCATOR_HPP_
