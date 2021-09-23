/*!
 * @file	lf_mem_alloc_type.hpp
 * @brief
 * @author	alpha
 * @date	Created on 2021/05/30
 * @details
 *
 * Copyright (C) 2021 by alpha <e-mail address>
 */

#ifndef INC_ALCONCURRENT_LF_MEM_ALLOC_TYPE_HPP_
#define INC_ALCONCURRENT_LF_MEM_ALLOC_TYPE_HPP_

#include <cstdlib>

namespace alpha {
namespace concurrent {

/*!
 * @breif	configuration paramters when allocate chunk
 */
struct param_chunk_allocation {
	std::size_t size_of_one_piece_ = 0;   //!< size of one piece in a chunk
	std::size_t num_of_pieces_     = 0;   //!< number of pieces in a chunk
};

struct chunk_statistics {
	param_chunk_allocation alloc_conf_;
	std::size_t            chunk_num_;
	std::size_t            total_slot_cnt_;
	std::size_t            free_slot_cnt_;
	std::size_t            alloc_req_cnt_;
	std::size_t            error_alloc_req_cnt_;
	std::size_t            dealloc_req_cnt_;
	std::size_t            error_dealloc_req_cnt_;
	int                    alloc_collision_cnt_;
	int                    dealloc_collision_cnt_;

	std::string print( void );
};

}   // namespace concurrent
}   // namespace alpha

#endif /* INC_ALCONCURRENT_LF_MEM_ALLOC_TYPE_HPP_ */
