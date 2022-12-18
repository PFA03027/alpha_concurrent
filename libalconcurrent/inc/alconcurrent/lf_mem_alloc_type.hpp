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

/*!
 * @brief	configuration paramters when allocate chunk
 */
struct param_chunk_allocation {
	unsigned int size_of_one_piece_ = 0;   //!< size of one piece in a chunk
	unsigned int num_of_pieces_     = 0;   //!< number of pieces in a chunk
};

/*!
 * @brief	chunk statistics information
 *
 * This is used for optimization for a paramters
 */
struct chunk_statistics {
	param_chunk_allocation alloc_conf_;              //!< chunk configuration
	std::size_t            chunk_num_;               //!< number of current allocated chunks
	std::size_t            valid_chunk_num_;         //!< number of valid chunks
	std::size_t            total_slot_cnt_;          //!< total number of slots
	std::size_t            free_slot_cnt_;           //!< total number of free slots
	std::size_t            consum_cnt_;              //!< total number of used slots
	std::size_t            max_consum_cnt_;          //!< maximum number of used slots
	std::size_t            alloc_req_cnt_;           //!< number of allocation requests
	std::size_t            error_alloc_req_cnt_;     //!< number of allocation failure
	std::size_t            dealloc_req_cnt_;         //!< number of deallocation requests
	std::size_t            error_dealloc_req_cnt_;   //!< number of deallocation failure
	unsigned int           alloc_collision_cnt_;     //!< number of allocation collision
	unsigned int           dealloc_collision_cnt_;   //!< number of deallocation collision

	/*!
	 * @brief	make std::string of statistics information
	 */
	std::string print( void );
};

/*!
 * @brief	caller context
 */
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

#define ALCONCURRENT_CONF_MAX_RECORD_BACKTRACE_SIZE ( 100 )   //!< size of backtrace buffer size

/*!
 * @brief	caller context
 */
struct bt_info {
	int   count_;                                             //!< backtrace data size. Zero: no data, Plus value: call stack information is valid, Minus value: information of previous allocation
	void* bt_[ALCONCURRENT_CONF_MAX_RECORD_BACKTRACE_SIZE];   //!< call stack of backtrace

	bt_info( void )
	  : count_( 0 )
	{
	}

	~bt_info()                           = default;
	bt_info( const bt_info& )            = default;
	bt_info( bt_info&& )                 = default;
	bt_info& operator=( const bt_info& ) = default;
	bt_info& operator=( bt_info&& )      = default;

	void dump_to_log( log_type lt, int id );
};

}   // namespace concurrent
}   // namespace alpha

#endif /* INC_ALCONCURRENT_LF_MEM_ALLOC_TYPE_HPP_ */
