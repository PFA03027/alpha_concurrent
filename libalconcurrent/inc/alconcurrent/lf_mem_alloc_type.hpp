/*!
 * @file	lf_mem_alloc_type.hpp
 * @brief
 * @author	alpha
 * @date	Created on 2021/05/30
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef INC_ALCONCURRENT_LF_MEM_ALLOC_TYPE_HPP_
#define INC_ALCONCURRENT_LF_MEM_ALLOC_TYPE_HPP_

#include <cstdlib>

#include "alconcurrent/conf_logger.hpp"

namespace alpha {
namespace concurrent {

constexpr size_t default_slot_alignsize = sizeof( std::uintptr_t );

/*!
 * @brief	configuration paramters when allocate chunk
 */
struct param_chunk_allocation {
	size_t size_of_one_piece_ = 0;   //!< size of one piece in a chunk
	size_t num_of_pieces_     = 0;   //!< number of pieces in a chunk
};

/*!
 * @brief	chunk statistics information
 *
 * This is used for optimization for a paramters
 */
struct chunk_statistics {
	param_chunk_allocation alloc_conf_;        //!< chunk configuration
	std::size_t            chunk_num_;         //!< number of current allocated chunks
	std::size_t            valid_chunk_num_;   //!< number of valid chunks
	// std::size_t cur_thread_num_;    //!< current number of thread
	// std::size_t max_thread_num_;    //!< max number of thread
	std::size_t  total_slot_cnt_;          //!< total number of slots
	std::size_t  free_slot_cnt_;           //!< total number of free slots
	std::size_t  consum_cnt_;              //!< total number of used slots
	std::size_t  max_consum_cnt_;          //!< maximum number of used slots
	std::size_t  alloc_req_cnt_;           //!< number of allocation requests
	std::size_t  error_alloc_req_cnt_;     //!< number of allocation failure
	std::size_t  dealloc_req_cnt_;         //!< number of deallocation requests
	std::size_t  error_dealloc_req_cnt_;   //!< number of deallocation failure
	unsigned int alloc_collision_cnt_;     //!< number of allocation collision
	unsigned int dealloc_collision_cnt_;   //!< number of deallocation collision

	/*!
	 * @brief	make std::string of statistics information
	 */
	std::string print( void );
};

}   // namespace concurrent
}   // namespace alpha

#endif /* INC_ALCONCURRENT_LF_MEM_ALLOC_TYPE_HPP_ */
