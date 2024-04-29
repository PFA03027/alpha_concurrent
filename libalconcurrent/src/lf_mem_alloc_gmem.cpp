/*!
 * @file	lf_mem_alloc_gmem.cpp
 * @brief	default parameter of semi lock-free memory allocater
 * @author	Teruaki Ata
 * @date	Created on 2022/02/13
 * @details
 *
 * Copyright (C) 2022 by alpha Teruaki Ata <PFA03027@nifty.com>
 */

#include <cstddef>

#include "alconcurrent/internal/lf_mem_alloc_internal.hpp"
#include "alconcurrent/lf_mem_alloc.hpp"

namespace alpha {
namespace concurrent {

////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace internal {

constexpr size_t TMP_MINIMUM_ALLOC_SIZE   = alignof( std::max_align_t );   //!< cache line bytes. This is configurable value
constexpr size_t TMP_INITIAL_NUM_OF_SLOTS = 32;

static_general_mem_allocator<14> get_g_gm_mem_instance_singleton(
	param_chunk_allocation { TMP_MINIMUM_ALLOC_SIZE, TMP_INITIAL_NUM_OF_SLOTS },          // 1
	param_chunk_allocation { TMP_MINIMUM_ALLOC_SIZE * 2, TMP_INITIAL_NUM_OF_SLOTS },      // 2
	param_chunk_allocation { TMP_MINIMUM_ALLOC_SIZE * 4, TMP_INITIAL_NUM_OF_SLOTS },      // 3
	param_chunk_allocation { TMP_MINIMUM_ALLOC_SIZE * 8, TMP_INITIAL_NUM_OF_SLOTS },      // 4
	param_chunk_allocation { TMP_MINIMUM_ALLOC_SIZE * 16, TMP_INITIAL_NUM_OF_SLOTS },     // 5
	param_chunk_allocation { TMP_MINIMUM_ALLOC_SIZE * 32, TMP_INITIAL_NUM_OF_SLOTS },     // 6
	param_chunk_allocation { TMP_MINIMUM_ALLOC_SIZE * 64, TMP_INITIAL_NUM_OF_SLOTS },     // 7
	param_chunk_allocation { TMP_MINIMUM_ALLOC_SIZE * 128, TMP_INITIAL_NUM_OF_SLOTS },    // 8
	param_chunk_allocation { TMP_MINIMUM_ALLOC_SIZE * 256, TMP_INITIAL_NUM_OF_SLOTS },    // 9
	param_chunk_allocation { TMP_MINIMUM_ALLOC_SIZE * 512, TMP_INITIAL_NUM_OF_SLOTS },    // 10
	param_chunk_allocation { TMP_MINIMUM_ALLOC_SIZE * 1024, TMP_INITIAL_NUM_OF_SLOTS },   // 11
	param_chunk_allocation { TMP_MINIMUM_ALLOC_SIZE * 2048, TMP_INITIAL_NUM_OF_SLOTS },   // 12
	param_chunk_allocation { TMP_MINIMUM_ALLOC_SIZE * 4096, TMP_INITIAL_NUM_OF_SLOTS },   // 13
	param_chunk_allocation { TMP_MINIMUM_ALLOC_SIZE * 8192, TMP_INITIAL_NUM_OF_SLOTS }    // 14
);

}   // namespace internal

void* gmem_allocate(
	size_t n   //!< [in] memory size to allocate
)
{
	return internal::get_g_gm_mem_instance_singleton.allocate( n, default_slot_alignsize );
}

void* gmem_allocate(
	size_t n,          //!< [in] memory size to allocate
	size_t req_align   //!< [in] requested align size. req_align should be the power of 2
)
{
	return internal::get_g_gm_mem_instance_singleton.allocate( n, req_align );
}

void gmem_deallocate(
	void* p_mem   //!< [in] pointer to free.
)
{
	internal::get_g_gm_mem_instance_singleton.deallocate( p_mem );
	return;
}

void gmem_prune( void )
{
	internal::get_g_gm_mem_instance_singleton.prune();
	return;
}

general_mem_allocator_statistics gmem_get_statistics( void )
{
	return internal::get_g_gm_mem_instance_singleton.get_statistics();
}

}   // namespace concurrent
}   // namespace alpha
