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

#include "lf_mem_alloc_internal.hpp"
#include "lf_mem_alloc_type.hpp"

namespace alpha {
namespace concurrent {

/*!
 * @breif	semi lock-free memory allocator based on multi chunk size list
 *
 * If requested size is over max size that parameter has, just call malloc()
 */
class general_mem_allocator {
public:
	/*!
	 * @breif	constructor
	 */
	general_mem_allocator(
		const param_chunk_allocation* p_param_array,   //!< [in] pointer to parameter array
		int                           num              //!< [in] array size
	);

	/*!
	 * @breif	constructor
	 */
	template <int N>
	general_mem_allocator(
		const param_chunk_allocation param_array[N]   //!< [in] param array
		)
	  : general_mem_allocator( param_array, N )
	{
	}

	/*!
	 * @breif	allocate memory
	 */
	void* allocate( std::size_t n );

	/*!
	 * @breif	deallocate memory
	 */
	void deallocate( void* p_mem );

	/*!
	 * @breif	get statistics
	 */
	std::list<chunk_statistics> get_statistics( void ) const;

private:
	struct param_chunk_comb {
		param_chunk_allocation                param_;          //!< parametor for chunk
		std::unique_ptr<internal::chunk_list> up_chunk_lst_;   //!< unique pointer to chunk list
	};

	int                                 pr_ch_size_;          // array size of chunk and param array
	std::unique_ptr<param_chunk_comb[]> up_param_ch_array_;   //!< unique pointer to chunk and param array
};

}   // namespace concurrent
}   // namespace alpha

#endif /* INC_ALCONCURRENT_LF_MEM_ALLOC_HPP_ */
