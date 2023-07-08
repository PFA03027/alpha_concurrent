/**
 * @file mmap_allocator.hpp
 * @author Teruaki Ata <PFA03027@nifty.com>
 * @brief memory allocator by mmap()
 * @version 0.1
 * @date 2023-07-02
 *
 * @copyright Copyright (c) 2023 by Teruaki Ata <PFA03027@nifty.com>
 *
 */

#ifndef MMAP_ALLOCATOR_HPP_
#define MMAP_ALLOCATOR_HPP_

#include <cstdlib>

namespace alpha {
namespace concurrent {

////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace internal {

/**
 * @brief allocate result by allocate_by_mmap()
 *
 */
struct allocate_result {
	void*  p_allocated_addr_;   //!< allocated memory address. if nullptr, fail to allocate
	size_t allocated_size_;     //!< allocated memory size. if 0, fail to allocate
};

/**
 * @brief allocate memory by mmap()
 *
 * @param req_alloc_size requet memory size to allocate
 * @param align_size alignment size of the allocated memorry address
 * @return allocate_result
 */
allocate_result allocate_by_mmap( size_t req_alloc_size, size_t align_size );

/**
 * @brief deallocate memory by munmap()
 *
 * @param p_allocated_addr address that is return value of allocate_by_mmap()
 * @param allocated_size allocated memory size by allocate_by_mmap()
 * @retval 0 success
 * @retval -1 fail. please also check errno
 */
int deallocate_by_munmap( void* p_allocated_addr, size_t allocated_size );

// configuration value
constexpr size_t conf_max_mmap_alloc_size = 1024 * 1024 * 1024;   // 1G

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif   // MMAP_ALLOCATOR_HPP_
