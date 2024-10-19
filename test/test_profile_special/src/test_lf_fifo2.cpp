/**
 * @file test_lf_stack2.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-03
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include <pthread.h>

#include <cstdint>
#include <deque>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "alconcurrent/lf_fifo.hpp"
#include "alconcurrent/lf_mem_alloc_type.hpp"
#include "alconcurrent/lf_stack.hpp"

#include "../../../sample/perf_stack/perf_stack_NtoN.hpp"

#include "test_lf_fixture.hpp"

///////////////////////////////////////////////////////////////////////////////
constexpr size_t SUT_N    = 10;
constexpr size_t THREAD_N = 20;

#if 1
TEST_F( TestLF_2_HighLoad, TC_FIFO_Profile1 )
{
	using TestType = std::size_t;
	// using TestType = int;

	std::cout << "--- pre-cpu kicking fifo_list " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_stack_NtoN<alpha::concurrent::fifo_list<TestType>, SUT_N>( THREAD_N, 1 );
}
TEST_F( TestLF_2_HighLoad, TC_FIFO_Profile2 )
{
	using TestType = std::size_t;
	// using TestType = int;

	std::cout << "--- fifo_list " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_stack_NtoN<alpha::concurrent::fifo_list<TestType>, SUT_N>( THREAD_N, 10 );
}
#endif
#if 0
TEST_F( TestLF_2_HighLoad, TC_FIFO_Profile3 )
{
	using TestType = std::size_t;
	// using TestType = int;

	std::cout << "--- pre-cpu kicking obsolate_fifo_list " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_stack_NtoN<alpha::concurrent::obsolate_fifo_list<TestType>, SUT_N>( THREAD_N, 1 );
}
TEST_F( TestLF_2_HighLoad, TC_FIFO_Profile4 )
{
	using TestType = std::size_t;
	// using TestType = int;

	std::cout << "--- obsolate_fifo_list " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_stack_NtoN<alpha::concurrent::obsolate_fifo_list<TestType>, SUT_N>( THREAD_N, 10 );
}
#endif
