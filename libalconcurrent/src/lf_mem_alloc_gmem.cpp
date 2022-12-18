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
#include "alconcurrent/lf_mem_alloc_internal.hpp"

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
	std::size_t      n,               //!< [in] memory size to allocate
	caller_context&& caller_ctx_arg   //!< [in] caller context information
)
{
	return internal::get_g_gm_mem_instance().allocate( n, std::move( caller_ctx_arg ) );
}

void gmem_deallocate(
	void*            p_mem,           //!< [in] pointer to free.
	caller_context&& caller_ctx_arg   //!< [in] caller context information
)
{
	internal::get_g_gm_mem_instance().deallocate( p_mem, std::move( caller_ctx_arg ) );
	return;
}

}   // namespace concurrent
}   // namespace alpha
