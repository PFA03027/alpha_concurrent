/**
 * @file lf_mem_alloc_basic_allocator.hpp
 * @author Teruaki Ata <PFA03027@nifty.com>
 * @brief common allocator/deallocator
 * @version 0.1
 * @date 2023-08-06
 *
 * @copyright Copyright (c) 2023 Teruaki Ata <PFA03027@nifty.com>
 *
 */

#ifndef LF_MEM_ALLOC_BASIC_ALLOCATOR_HPP_
#define LF_MEM_ALLOC_BASIC_ALLOCATOR_HPP_

#include "mmap_allocator.hpp"
#include <cstdlib>

namespace alpha {
namespace concurrent {

namespace internal {

struct basic_mem_allocator {
	static inline allocate_result allocate( size_t n )
	{
		return allocate_result { std::malloc( n ), n };
	}
	static inline void deallocate( void* p_mem, size_t n )
	{
		std::free( p_mem );
	}
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
