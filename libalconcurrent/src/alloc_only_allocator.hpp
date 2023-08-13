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

/**
 * @brief dump log of pre-defined global allocating_only()
 *
 */
void allocating_only_dump_to_log( log_type lt, char c, int id );

// configuration value
constexpr size_t conf_pre_mmap_size = 1024 * 1024;

////////////////////////////////////////////////////////////////////////////////////////////////////////
// internal I/F
class alloc_chamber;

class alloc_chamber_head {
public:
	constexpr alloc_chamber_head( bool need_release_munmap_arg, size_t pre_alloc_size_arg = conf_pre_mmap_size )
	  : head_( nullptr )
	  , need_release_munmap_( need_release_munmap_arg )
	  , pre_alloc_size_( pre_alloc_size_arg )
	{
	}

	~alloc_chamber_head();

	void* allocate( size_t req_size, size_t req_align );

	void detect_unexpected_deallocate( void* );

	void dump_to_log( log_type lt, char c, int id );

private:
	void* try_allocate( size_t req_size, size_t req_align );
	void  push_alloc_mem( void* p_alloced_mem, size_t allocated_size );
	void  munmap_alloc_chamber( alloc_chamber* p_ac );

	std::atomic<alloc_chamber*> head_;                  //!< alloc_chamberのスタックリスト上のheadのalloc_chamber
	bool                        need_release_munmap_;   //!< true: when destructing, munmap memory
	size_t                      pre_alloc_size_;        //!< mmapで割り当てる基本サイズ
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif   // ALLOC_ONLY_ALLOCATOR_HPP_
