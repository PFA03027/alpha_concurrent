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

#if 0
#include "alconcurrent/conf_logger.hpp"
#include "mmap_allocator.hpp"
#include <cerrno>
#include <cstdlib>

namespace alpha {
namespace concurrent {

namespace internal {

struct basic_mem_allocator {
	static inline allocate_result allocate( size_t n, size_t req_align_size )
	{
		return allocate_by_mmap( n, req_align_size );
	}
	static inline void deallocate( void* p_mem, size_t allocated_size )
	{
		int ret = deallocate_by_munmap( p_mem, allocated_size );
		if ( ret != 0 ) {
			auto er_errno = errno;
			char buff[128];
#if ( _POSIX_C_SOURCE >= 200112L ) && !_GNU_SOURCE
			strerror_r( er_errno, buff, 128 );
			internal::LogOutput( log_type::ERR, "munmap is fail with %s", buff );
#else
			const char* ret_str = strerror_r( er_errno, buff, 128 );
			internal::LogOutput( log_type::ERR, "munmap is fail with %s", ret_str );
#endif
		}
	}
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
#else
#include "mmap_allocator.hpp"
#include <cstdlib>

namespace alpha {
namespace concurrent {

namespace internal {

struct basic_mem_allocator {
	static inline allocate_result allocate( size_t n, size_t req_align_size )
	{
		return allocate_result { std::malloc( n ), n };
	}
	static inline void deallocate( void* p_mem, size_t allocated_size )
	{
		std::free( p_mem );
	}
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
#endif

#endif
