/*!
 * @file	lf_mem_alloc_gmem.cpp
 * @brief	default parameter of semi lock-free memory allocater
 * @author	Teruaki Ata
 * @date	Created on 2022/02/13
 * @details
 *
 * Copyright (C) 2022 by alpha Teruaki Ata <PFA03027@nifty.com>
 */

#include "alconcurrent/lf_mem_alloc.hpp"
#include "lf_mem_alloc_internal.hpp"

namespace alpha {
namespace concurrent {

////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace internal {

inline general_mem_allocator& get_g_gm_mem_instance( void )
{
	static general_mem_allocator singleton( default_param_array, num_of_default_param_array );
	return singleton;
}

}   // namespace internal

void* gmem_allocate(
	std::size_t n   //!< [in] memory size to allocate
)
{
	return internal::get_g_gm_mem_instance().allocate( n );
}

void gmem_deallocate(
	void* p_mem   //!< [in] pointer to free.
)
{
	internal::get_g_gm_mem_instance().deallocate( p_mem );
	return;
}

void gmem_prune( void )
{
	internal::get_g_gm_mem_instance().prune();
	return;
}

std::list<chunk_statistics> gmem_get_statistics( void )
{
	return internal::get_g_gm_mem_instance().get_statistics();
}

}   // namespace concurrent
}   // namespace alpha
