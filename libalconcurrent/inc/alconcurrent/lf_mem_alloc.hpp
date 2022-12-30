/*!
 * @file	lf_mem_alloc.hpp
 * @brief	semi lock-free memory allocator
 * @author	Teruaki Ata
 * @date	Created on 2021/05/12
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef INC_ALCONCURRENT_LF_MEM_ALLOC_HPP_
#define INC_ALCONCURRENT_LF_MEM_ALLOC_HPP_

#include <atomic>
#include <cstdlib>
#include <list>
#include <memory>

#include "conf_logger.hpp"

#include "lf_mem_alloc_type.hpp"

namespace alpha {
namespace concurrent {

extern const unsigned int           num_of_default_param_array;   //!< array size of default parameter array
extern const param_chunk_allocation default_param_array[];        //!< pointer to default parameter array

namespace internal {
class chunk_list;
}

/*!
 * @brief	semi lock-free memory allocator based on multi chunk size list
 *
 * If requested size is over max size that parameter has, just call malloc()
 */
class general_mem_allocator {
public:
	/*!
	 * @brief	constructor
	 */
	general_mem_allocator( void );

	/*!
	 * @brief	constructor
	 */
	general_mem_allocator(
		const param_chunk_allocation* p_param_array,   //!< [in] pointer to parameter array
		unsigned int                  num              //!< [in] array size
	);

	/*!
	 * @brief	destructor
	 */
	~general_mem_allocator();

	/*!
	 * @brief	allocate memory
	 */
	void* allocate(
		std::size_t      n,                                                         //!< [in] memory size to allocate
		caller_context&& caller_ctx_arg = ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG   //!< [in] caller context information
	);

	/*!
	 * @brief	deallocate memory
	 */
	void deallocate(
		void*            p_mem,                                                     //!< [in] pointer to allocated memory by allocate()
		caller_context&& caller_ctx_arg = ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG   //!< [in] caller context information
	);

	/*!
	 * @brief	free the deletable buffers
	 */
	void prune( void );

	/*!
	 * @brief	set parameter
	 *
	 * If already set paramter by constructor or this I/F, this call is ignored.
	 *
	 * @warn
	 * This I/F is NOT thread safe. @n
	 * This should be called before launching any threads. Therefore mostly debug or sample code purpose only.
	 */
	void set_param(
		const param_chunk_allocation* p_param_array,   //!< [in] pointer to parameter array
		unsigned int                  num              //!< [in] array size
	);

	/*!
	 * @brief	get statistics
	 *
	 * @note
	 * This I/F does not lock allocat/deallocate itself, but this I/F execution is not lock free.
	 */
	std::list<chunk_statistics> get_statistics( void ) const;

private:
	struct param_chunk_comb {
		param_chunk_allocation                param_;          //!< parametor for chunk
		std::unique_ptr<internal::chunk_list> up_chunk_lst_;   //!< unique pointer to chunk list
	};

	unsigned int                        pr_ch_size_;          //!< array size of chunk and param array
	std::unique_ptr<param_chunk_comb[]> up_param_ch_array_;   //!< unique pointer to chunk and param array

	std::atomic_bool exclusive_ctl_of_prune_;   //!< exclusive control for prune()
};

/*!
 * @brief	allocate memory
 *
 * This I/F allocates a memory from a global general_mem_allocator instance. @n
 * The allocated memory must free by gmem_deallocate().
 *
 * @note
 * This uses default_param_array and num_of_default_param_array as initial allocation parameter
 */
void* gmem_allocate(
	std::size_t      n,                                                         //!< [in] memory size to allocate
	caller_context&& caller_ctx_arg = ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG   //!< [in] caller context information
);

/*!
 * @brief	deallocate memory
 *
 * This I/F free a memory area that is allocated by gmem_allocate().
 *
 * @note
 * If p_mem is not a memory that is not allocated by gmem_allocate(), this I/F will try to free by calling free().
 */
void gmem_deallocate(
	void*            p_mem,                                                     //!< [in] pointer to free.
	caller_context&& caller_ctx_arg = ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG   //!< [in] caller context information
);

/*!
 * @brief	free the deletable buffers
 */
void gmem_prune( void );

/*!
 * @brief	get statistics from a global general_mem_allocator instance
 *
 * @note
 * This I/F does not lock allocat/deallocate itself, but this I/F execution is not lock free.
 */
std::list<chunk_statistics> gmem_get_statistics( void );

/*!
 * @brief get backtrace information
 *
 * this is for debug purpose.
 *
 * @return backtrace information
 * @return 1st element: check result. if success, slot_chk_result::correct_ is true
 * @return 2nd element: backtrace when this memory is allocated.
 * @return 3rd element: backtrace when this memory is free.
 */
std::tuple<bool,
           alpha::concurrent::bt_info,
           alpha::concurrent::bt_info>
get_backtrace_info(
	void* p_mem   //!< [in] pointer to allocated memory by allocate()
);

/*!
 * @brief output backtrace information to log output
 */
void output_backtrace_info(
	const log_type lt,     //!< [in] log type
	void*          p_mem   //!< [in] pointer to allocated memory by allocate()
);

bool test_platform_std_atomic_lockfree_condition( void );

}   // namespace concurrent
}   // namespace alpha

#endif /* INC_ALCONCURRENT_LF_MEM_ALLOC_HPP_ */
