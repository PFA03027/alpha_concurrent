/*!
 * @file	lf_mem_alloc_default_param.cpp
 * @brief	default parameter of semi lock-free memory allocater
 * @author	Teruaki Ata
 * @date	Created on 2022/02/13
 * @details
 *
 * This is the default parameter implemention. @n
 * If user would like to define own default parameter, please define by your value without compiling this file.
 *
 * Copyright (C) 2022 by alpha Teruaki Ata <PFA03027@nifty.com>
 */

#include "alconcurrent/lf_mem_alloc.hpp"
#include "alconcurrent/lf_mem_alloc_internal.hpp"

namespace alpha {
namespace concurrent {

#define CACHE_LINE_BYTES ( 64 )   //!< cache line bytes. This is configurable value
#define INITIAL_NUM_OF_SLOTS ( 32 )

const unsigned int           num_of_default_param_array = 11;   //!< array size of default parameter array
const param_chunk_allocation default_param_array[]      = {
    { CACHE_LINE_BYTES, INITIAL_NUM_OF_SLOTS },          // 1
    { CACHE_LINE_BYTES * 2, INITIAL_NUM_OF_SLOTS },      // 2
    { CACHE_LINE_BYTES * 4, INITIAL_NUM_OF_SLOTS },      // 3
    { CACHE_LINE_BYTES * 8, INITIAL_NUM_OF_SLOTS },      // 4
    { CACHE_LINE_BYTES * 16, INITIAL_NUM_OF_SLOTS },     // 5
    { CACHE_LINE_BYTES * 32, INITIAL_NUM_OF_SLOTS },     // 6
    { CACHE_LINE_BYTES * 64, INITIAL_NUM_OF_SLOTS },     // 7
    { CACHE_LINE_BYTES * 128, INITIAL_NUM_OF_SLOTS },    // 8
    { CACHE_LINE_BYTES * 256, INITIAL_NUM_OF_SLOTS },    // 9
    { CACHE_LINE_BYTES * 512, INITIAL_NUM_OF_SLOTS },    // 10
    { CACHE_LINE_BYTES * 1024, INITIAL_NUM_OF_SLOTS },   // 11
};                                                       //!< pointer to default parameter array

}   // namespace concurrent
}   // namespace alpha
