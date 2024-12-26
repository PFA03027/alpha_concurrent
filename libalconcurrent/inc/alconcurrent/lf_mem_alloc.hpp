/*!
 * @file	lf_mem_alloc.hpp
 * @brief	semi lock-free memory allocator
 * @author	Teruaki Ata
 * @date	Created on 2021/05/12
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef ALCONCCURRENT_INC_LF_MEM_ALLOC_HPP_
#define ALCONCCURRENT_INC_LF_MEM_ALLOC_HPP_

#include <cstddef>
#include <cstdint>

namespace alpha {
namespace concurrent {

/*!
 * @brief	allocate memory
 *
 * This I/F allocates a memory from a global general_mem_allocator instance. @n
 * The allocated memory must free by gmem_deallocate().
 *
 * @note
 * This uses default_param_array and num_of_default_param_array as initial allocation parameter
 *
 * @exception
 * If req_align is not power of 2, throw std::logic_error.
 */
void* gmem_allocate(
	size_t n,                                //!< [in] memory size to allocate
	size_t req_align = sizeof( uintptr_t )   //!< [in] requested align size. req_align should be the power of 2
);

/*!
 * @brief	deallocate memory
 *
 * This I/F free a memory area that is allocated by gmem_allocate().
 *
 * @note
 * If p_mem is not a memory that is not allocated by gmem_allocate(), this I/F will try to free by calling free().
 */
bool gmem_deallocate(
	void* p_mem   //!< [in] pointer to free.
);

}   // namespace concurrent
}   // namespace alpha

#endif /* INC_ALCONCURRENT_LF_MEM_ALLOC_HPP_ */
