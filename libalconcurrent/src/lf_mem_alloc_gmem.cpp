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
	std::size_t n   //!< [in] memory size to allocate
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
	,
	const char* caller_src_fname,   //!< [in] caller side source file name
	const int   caller_lineno,      //!< [in] caller side line number
	const char* caller_func_name    //!< [in] function name calling this I/F
#endif
)
{
	return internal::get_g_gm_mem_instance().allocate(
		n
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
		,
		caller_src_fname, caller_lineno, caller_func_name
#endif
	);
}

void gmem_deallocate(
	void* p_mem   //!< [in] pointer to free.
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
	,
	const char* caller_src_fname,   //!< [in] caller side source file name
	const int   caller_lineno,      //!< [in] caller side line number
	const char* caller_func_name    //!< [in] function name calling this I/F
#endif
)
{
	internal::get_g_gm_mem_instance().deallocate(
		p_mem
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
		,
		caller_src_fname, caller_lineno, caller_func_name
#endif
	);
	return;
}

}   // namespace concurrent
}   // namespace alpha
