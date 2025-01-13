/**
 * @file mmap_allocator.cpp
 * @author Teruaki Ata <PFA03027@nifty.com>
 * @brief memory allocator by mmap()
 * @version 0.1
 * @date 2023-07-02
 *
 * @copyright Copyright (c) 2023 by Teruaki Ata <PFA03027@nifty.com>
 *
 */

#include <cerrno>
#include <cstdint>
#include <cstdio>

#include <atomic>

#include <sys/mman.h>
#include <unistd.h>

#include "alconcurrent/conf_logger.hpp"
#include "mmap_allocator.hpp"

namespace alpha {
namespace concurrent {

////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace internal {

// static size_t get_cur_system_page_size( void )
// {
// 	auto   v   = sysconf( _SC_PAGE_SIZE );
// 	size_t ans = static_cast<size_t>( v );
// 	if ( v <= 0 ) {
// 		ans = 1024 * 4;   // もしシステムコールがNGとなった場合のフォールバッグ。ただ、本質的には無駄が多い。
// 	}
// 	return ans;
// }

// static const size_t page_size = get_cur_system_page_size();
static constexpr size_t page_size = 1024 * 4;   //  = sysconf( _SC_PAGE_SIZE ); It assumes that the values ​​are powers of 2.

std::atomic<size_t> cur_total_allocation_size( 0 );
std::atomic<size_t> max_total_allocation_size( 0 );

struct alloc_params {
	size_t page_aligned_align_size_;
	size_t page_aligned_real_alloc_size_;
	size_t page_aligned_request_overfit_alloc_size_;
};

inline alloc_params calc_cur_system_alloc_params( size_t req_alloc_size, size_t align_size ) noexcept
{
	// 適切なalignment値を計算する。
	size_t corrected_align_size = page_size;
	if ( align_size <= page_size ) {
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
		if ( !is_power_of_2( align_size ) ) {
			LogOutput( log_type::ERR, "req_align(%zu) of allocate_by_mmap() is not power of 2.", align_size );
		}
#endif
	} else {
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
		if ( ( align_size % page_size ) == 0 ) {
			corrected_align_size = align_size;
		} else {
			LogOutput( log_type::ERR, "req_align(%zu) of allocate_by_mmap() is not multiple of 4096.", align_size );
			corrected_align_size = page_size;
		}
#else
		corrected_align_size = align_size;
#endif
	}
	size_t min_aligne_size = corrected_align_size;

	size_t num_alloc_pages     = ( req_alloc_size + ( page_size - 1 ) ) / page_size;
	size_t cur_real_alloc_size = page_size * num_alloc_pages;

	size_t num_overfit_pages = ( req_alloc_size + ( min_aligne_size - 1 ) ) / page_size;
	size_t overfit_size      = page_size * num_overfit_pages;

	return alloc_params { min_aligne_size, cur_real_alloc_size, overfit_size };
}

allocate_result allocate_by_mmap( size_t req_alloc_size, size_t align_size ) noexcept
{
	if ( req_alloc_size > conf_max_mmap_alloc_size ) {
		// too big allocation request
		return allocate_result { nullptr, 0 };
	}

	alloc_params page_aligned_params = calc_cur_system_alloc_params( req_alloc_size, align_size );
#ifdef DEBUG_LOG
	printf( "page_size = %zu = 0x%zx\n", page_size, page_size );
	printf( "page_aligned_overfit_alloc_size\t=       0x%zx\n", page_aligned_params.page_aligned_request_overfit_alloc_size_ );
#endif

#ifdef ALCONCURRENT_CONF_ENABLE_MALLOC_INSTEAD_OF_MMAP
	void* p_alloc_expected = malloc( page_aligned_params.page_aligned_request_overfit_alloc_size_ );
#else
	void* p_alloc_by_mmap = mmap( NULL, page_aligned_params.page_aligned_request_overfit_alloc_size_, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
	if ( MAP_FAILED == p_alloc_by_mmap ) {
		// auto cur_errno = errno;
		perror( "mmap is fail." );
		return allocate_result { nullptr, 0 };
	} else {
#ifdef DEBUG_LOG
		printf( "p_alloc_by_mmap\t\t\t= %p\n", p_alloc_by_mmap );
#endif
	}

	uintptr_t addr_alloc_by_mmap = reinterpret_cast<uintptr_t>( p_alloc_by_mmap );
	uintptr_t alloc_block_count  = addr_alloc_by_mmap / page_aligned_params.page_aligned_align_size_;
	uintptr_t alloc_block_remain = addr_alloc_by_mmap % page_aligned_params.page_aligned_align_size_;

	uintptr_t addr_alloc_expected   = page_aligned_params.page_aligned_align_size_ * ( alloc_block_count + ( ( alloc_block_remain == 0 ) ? 0 : 1 ) );
	uintptr_t addr_unmap_pre_block  = addr_alloc_by_mmap;
	uintptr_t size_unmap_pre_block  = ( alloc_block_remain == 0 ) ? 0 : ( page_aligned_params.page_aligned_align_size_ - alloc_block_remain );
	uintptr_t addr_unmap_post_block = addr_alloc_expected + page_aligned_params.page_aligned_real_alloc_size_;
	uintptr_t size_unmap_post_block = page_aligned_params.page_aligned_request_overfit_alloc_size_ - ( size_unmap_pre_block + page_aligned_params.page_aligned_real_alloc_size_ );

	void* p_alloc_expected   = reinterpret_cast<void*>( addr_alloc_expected );
	void* p_pre_unmap_block  = reinterpret_cast<void*>( addr_unmap_pre_block );
	void* p_post_unmap_block = reinterpret_cast<void*>( addr_unmap_post_block );

#ifdef DEBUG_LOG
	printf( "p_pre_unmap_block\t\t= %p - %p\n", p_pre_unmap_block, reinterpret_cast<void*>( addr_unmap_pre_block + size_unmap_pre_block ) );
	printf( "p_alloc_expected\t\t= %p - %p\n", p_alloc_expected, reinterpret_cast<void*>( addr_alloc_expected + page_aligned_params.page_aligned_real_alloc_size_ ) );
	printf( "p_post_unmap_block\t\t= %p - %p\n", p_post_unmap_block, reinterpret_cast<void*>( addr_unmap_post_block + size_unmap_post_block ) );
#endif

	if ( size_unmap_pre_block != 0 ) {
		int munmap_ret1 = munmap( p_pre_unmap_block, static_cast<size_t>( size_unmap_pre_block ) );
		if ( munmap_ret1 != 0 ) {
			// auto cur_errno = errno;
			perror( "munmap of pre block is fail." );
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
			exit( 1 );
#endif
		}
	}
	if ( size_unmap_post_block != 0 ) {
		int munmap_ret2 = munmap( p_post_unmap_block, static_cast<size_t>( size_unmap_post_block ) );
		if ( munmap_ret2 != 0 ) {
			// auto cur_errno = errno;
			perror( "munmap of post block is fail." );
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
			exit( 1 );
#endif
		}
	}
#endif

	size_t new_cur_size = cur_total_allocation_size.fetch_add( page_aligned_params.page_aligned_real_alloc_size_, std::memory_order_acq_rel );
	new_cur_size += page_aligned_params.page_aligned_real_alloc_size_;
	size_t cur_max = max_total_allocation_size.load( std::memory_order_acquire );
	if ( new_cur_size > cur_max ) {
		max_total_allocation_size.compare_exchange_strong( cur_max, new_cur_size );
	}
	return allocate_result { p_alloc_expected, page_aligned_params.page_aligned_real_alloc_size_ };
}

int deallocate_by_munmap( void* p_allocated_addr, size_t allocated_size ) noexcept
{
	cur_total_allocation_size.fetch_sub( allocated_size, std::memory_order_acq_rel );
#ifdef ALCONCURRENT_CONF_ENABLE_MALLOC_INSTEAD_OF_MMAP
	free( p_allocated_addr );
	return 0;
#else
	return munmap( p_allocated_addr, static_cast<size_t>( allocated_size ) );
#endif
}

alloc_mmap_status get_alloc_mmap_status( void ) noexcept
{
	return alloc_mmap_status {
		cur_total_allocation_size.load( std::memory_order_acquire ),
		max_total_allocation_size.load( std::memory_order_acquire ) };
}

void print_of_mmap_allocator( void )
{
	auto   cur_data = get_alloc_mmap_status();
	size_t cur_size = cur_data.active_size_;
	size_t cur_max  = cur_data.max_size_;

	printf( "page_size               = %16zu = 0x%016zx\n", page_size, page_size );
	printf( "current allocation size = %16zu = 0x%016zx %.2fG %.2fM %.0fK\n", cur_size, cur_size,
	        static_cast<double>( cur_size ) / static_cast<double>( 1024 * 1024 * 1024 ),
	        static_cast<double>( cur_size ) / static_cast<double>( 1024 * 1024 ),
	        static_cast<double>( cur_size ) / static_cast<double>( 1024 )
	        //
	);
	printf( "max allocation size     = %16zu = 0x%016zx %.2fG %.2fM %.0fK\n",
	        cur_max,
	        cur_max,
	        static_cast<double>( cur_max ) / static_cast<double>( 1024 * 1024 * 1024 ),
	        static_cast<double>( cur_max ) / static_cast<double>( 1024 * 1024 ),
	        static_cast<double>( cur_max ) / static_cast<double>( 1024 )
	        //
	);
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
