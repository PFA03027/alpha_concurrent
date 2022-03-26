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

#include <cstddef>

#include "alconcurrent/lf_mem_alloc.hpp"
#include "alconcurrent/lf_mem_alloc_internal.hpp"

namespace alpha {
namespace concurrent {

#define MINIMUM_ALLOC_SIZE   ( alignof( std::max_align_t ) )   //!< cache line bytes. This is configurable value
#define INITIAL_NUM_OF_SLOTS ( 32 )

const unsigned int           num_of_default_param_array = 11;   //!< array size of default parameter array
const param_chunk_allocation default_param_array[]      = {
    { MINIMUM_ALLOC_SIZE, INITIAL_NUM_OF_SLOTS },          // 1
    { MINIMUM_ALLOC_SIZE * 2, INITIAL_NUM_OF_SLOTS },      // 2
    { MINIMUM_ALLOC_SIZE * 4, INITIAL_NUM_OF_SLOTS },      // 3
    { MINIMUM_ALLOC_SIZE * 8, INITIAL_NUM_OF_SLOTS },      // 4
    { MINIMUM_ALLOC_SIZE * 16, INITIAL_NUM_OF_SLOTS },     // 5
    { MINIMUM_ALLOC_SIZE * 32, INITIAL_NUM_OF_SLOTS },     // 6
    { MINIMUM_ALLOC_SIZE * 64, INITIAL_NUM_OF_SLOTS },     // 7
    { MINIMUM_ALLOC_SIZE * 128, INITIAL_NUM_OF_SLOTS },    // 8
    { MINIMUM_ALLOC_SIZE * 256, INITIAL_NUM_OF_SLOTS },    // 9
    { MINIMUM_ALLOC_SIZE * 512, INITIAL_NUM_OF_SLOTS },    // 10
    { MINIMUM_ALLOC_SIZE * 1024, INITIAL_NUM_OF_SLOTS },   // 11
};                                                         //!< pointer to default parameter array

}   // namespace concurrent
}   // namespace alpha
