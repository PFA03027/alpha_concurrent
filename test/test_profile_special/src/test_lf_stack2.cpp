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
#include "alconcurrent/lf_stack.hpp"

#include "mem_small_memory_slot.hpp"

#include "../../../sample/inc_common/perf_pushpop_NtoN.hpp"

#include "test_lf_fixture.hpp"

///////////////////////////////////////////////////////////////////////////////
constexpr size_t SUT_N    = 100;
constexpr size_t THREAD_N = 20;

#if 0
TEST_F( TestLF_2_HighLoad, TC_STACK_Profile1 )
{
	using TestType = std::size_t;
	// using TestType = int;

	std::cout << "--- pre-cpu kicking stack_list " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_pushpop_NtoN<alpha::concurrent::stack_list<TestType>, SUT_N>( THREAD_N, 1 );

	// alpha::concurrent::internal::memory_slot_group_list::dump_log( alpha::concurrent::log_type::DUMP, 'a', 1 );
}
TEST_F( TestLF_2_HighLoad, TC_STACK_Profile2 )
{
	using TestType = std::size_t;
	// using TestType = int;

	std::cout << "--- pre-cpu kicking stack_list " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_pushpop_NtoN<alpha::concurrent::stack_list<TestType>, 2>( 2, 1 );

	// alpha::concurrent::internal::memory_slot_group_list::dump_log( alpha::concurrent::log_type::DUMP, 'a', 1 );
}
#endif
#if 0
TEST_F( TestLF_2_HighLoad, TC_STACK_Profile3 )
{
	using TestType = std::size_t;
	// using TestType = int;

	std::cout << "--- stack_list " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_pushpop_NtoN<alpha::concurrent::stack_list<TestType>, SUT_N>( THREAD_N, 10 );
}
TEST_F( TestLF_2_HighLoad, TC_STACK_Profile4 )
{
	using TestType = std::size_t;
	// using TestType = int;

	std::cout << "--- pre-cpu kicking obsolate_stack_list " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_pushpop_NtoN<alpha::concurrent::obsolate_stack_list<TestType>, SUT_N>( THREAD_N, 1 );
}
TEST_F( TestLF_2_HighLoad, TC_STACK_Profile5 )
{
	using TestType = std::size_t;
	// using TestType = int;

	std::cout << "--- obsolate_stack_list " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_pushpop_NtoN<alpha::concurrent::obsolate_stack_list<TestType>, SUT_N>( THREAD_N, 10 );
}
#endif
