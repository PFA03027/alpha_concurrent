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
#include <cstdint>
#include <cstdio>

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

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
static constexpr size_t page_size = 1024 * 4;   //  = sysconf( _SC_PAGE_SIZE );

struct alloc_params {
	size_t page_aligned_align_size_;
	size_t page_aligned_real_alloc_size_;
	size_t page_aligned_request_overfit_alloc_size_;
};

inline alloc_params calc_cur_system_alloc_params( size_t req_alloc_size, size_t align_size )
{
	if ( align_size < sizeof( void* ) ) {
		align_size = sizeof( void* );   // 最低のアライメントをポインタサイズに制約する
	}

	size_t num_alloc_pages     = req_alloc_size / page_size;
	size_t cur_real_alloc_size = page_size * ( num_alloc_pages + ( ( ( req_alloc_size % page_size ) == 0 ) ? 0 : 1 ) );

	size_t num_align_pages = align_size / page_size;
	size_t min_aligne_size = page_size * ( num_align_pages + ( ( ( align_size % page_size ) == 0 ) ? 0 : 1 ) );

	size_t overfit_base_size = ( req_alloc_size <= align_size ) ? ( align_size * 2 ) : ( align_size + req_alloc_size );
	size_t num_overfit_pages = overfit_base_size / page_size;
	size_t overfit_size      = page_size * ( num_overfit_pages + ( ( ( overfit_base_size % page_size ) == 0 ) ? 0 : 1 ) );

	return alloc_params { min_aligne_size, cur_real_alloc_size, overfit_size };
}

allocate_result allocate_by_mmap( size_t req_alloc_size, size_t align_size )
{
	if ( req_alloc_size > conf_max_mmap_alloc_size ) {
		// too big allocation request
		return allocate_result { nullptr, 0 };
	}
	if ( align_size < sizeof( void* ) ) {
		align_size = sizeof( void* );   // 最低のアライメントをポインタサイズに制約する
	}

	alloc_params page_aligned_params = calc_cur_system_alloc_params( req_alloc_size, align_size );
#ifndef DEBUG_LOG
	printf( "page_size = %zu = 0x%zx\n", page_size, page_size );
	printf( "page_aligned_overfit_alloc_size\t=       0x%zx\n", page_aligned_params.page_aligned_request_overfit_alloc_size_ );
#endif

	void* p_alloc_by_mmap = mmap( NULL, page_aligned_params.page_aligned_request_overfit_alloc_size_, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
	if ( MAP_FAILED == p_alloc_by_mmap ) {
		// auto cur_errno = errno;
		perror( "mmap is fail." );
		return allocate_result { nullptr, 0 };
	} else {
#ifndef DEBUG_LOG
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

#ifndef DEBUG_LOG
	printf( "p_pre_unmap_block\t\t= %p - %p\n", p_pre_unmap_block, reinterpret_cast<void*>( addr_unmap_pre_block + size_unmap_pre_block ) );
	printf( "p_alloc_expected\t\t= %p - %p\n", p_alloc_expected, reinterpret_cast<void*>( addr_alloc_expected + page_aligned_params.page_aligned_real_alloc_size_ ) );
	printf( "p_post_unmap_block\t\t= %p - %p\n", p_post_unmap_block, reinterpret_cast<void*>( addr_unmap_post_block + size_unmap_post_block ) );
#endif

	if ( size_unmap_pre_block != 0 ) {
		int munmap_ret1 = munmap( p_pre_unmap_block, static_cast<size_t>( size_unmap_pre_block ) );
		if ( munmap_ret1 != 0 ) {
			// auto cur_errno = errno;
			perror( "munmap of pre block is fail." );
		}
	}
	if ( size_unmap_post_block != 0 ) {
		int munmap_ret2 = munmap( p_post_unmap_block, static_cast<size_t>( size_unmap_post_block ) );
		if ( munmap_ret2 != 0 ) {
			// auto cur_errno = errno;
			perror( "munmap of post block is fail." );
		}
	}

	return allocate_result { p_alloc_expected, page_aligned_params.page_aligned_real_alloc_size_ };
}

int deallocate_by_munmap( void* p_allocated_addr, size_t allocated_size )
{
	return munmap( p_allocated_addr, static_cast<size_t>( allocated_size ) );
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
