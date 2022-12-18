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

namespace alpha {
namespace concurrent {

/*!
 * @brief	configuration paramters when allocate chunk
 */
struct param_chunk_allocation {
	unsigned int size_of_one_piece_ = 0;   //!< size of one piece in a chunk
	unsigned int num_of_pieces_     = 0;   //!< number of pieces in a chunk
};

struct chunk_statistics {
	param_chunk_allocation alloc_conf_;
	std::size_t            chunk_num_;
	std::size_t            total_slot_cnt_;
	std::size_t            free_slot_cnt_;
	std::size_t            consum_cnt_;
	std::size_t            max_consum_cnt_;
	std::size_t            alloc_req_cnt_;
	std::size_t            error_alloc_req_cnt_;
	std::size_t            dealloc_req_cnt_;
	std::size_t            error_dealloc_req_cnt_;
	unsigned int           alloc_collision_cnt_;
	unsigned int           dealloc_collision_cnt_;

	std::string print( void );
};

struct caller_context {
	const char* p_caller_src_fname_;   //!< caller side source file name
	int         caller_lineno_;        //!< caller side line number
	const char* p_caller_func_name_;   //!< function name calling this I/F
};

#ifdef __GNUC__
#define ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG                  \
	alpha::concurrent::caller_context                            \
	{                                                            \
		__builtin_FILE(), __builtin_LINE(), __builtin_FUNCTION() \
	}
#else
#define ALCONCURRENT_DEFAULT_CALLER_CONTEXT_ARG \
	alpha::concurrent::caller_context           \
	{                                           \
		nullptr, 0, nullptr                     \
	}
#endif

}   // namespace concurrent
}   // namespace alpha

#endif /* INC_ALCONCURRENT_LF_MEM_ALLOC_TYPE_HPP_ */
