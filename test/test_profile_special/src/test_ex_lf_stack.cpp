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

#include "alconcurrent/experiment/ex_lf_stack.hpp"

#include "mem_small_memory_slot.hpp"

#include "../../../sample/inc_common/perf_pushpop_NtoN.hpp"

#include "test_lf_fixture.hpp"

///////////////////////////////////////////////////////////////////////////////
constexpr size_t SUT_N    = 100;
constexpr size_t THREAD_N = 20;

TEST_F( TestLF_2_HighLoad, TC_ExSTACK_Profile1 )
{
	using TestType = std::size_t;
	// using TestType = int;

	std::cout << "--- pre-cpu kicking stack_list " << std::to_string( 1 ) << " ---" << std::endl;
	bool ret = nwoker_perf_test_pushpop_NtoN<alpha::concurrent::ex_lf_stack<TestType>, 1>( 1, 1 );

	EXPECT_TRUE( ret );

	// alpha::concurrent::internal::memory_slot_group_list::dump_log( alpha::concurrent::log_type::DUMP, 'a', 1 );
}
TEST_F( TestLF_2_HighLoad, TC_ExSTACK_Profile2 )
{
	using TestType = std::size_t;
	// using TestType = int;

	std::cout << "--- pre-cpu kicking stack_list " << std::to_string( 2 ) << " ---" << std::endl;
	bool ret = nwoker_perf_test_pushpop_NtoN<alpha::concurrent::ex_lf_stack<TestType>, 2>( 2, 1 );

	EXPECT_TRUE( ret );

	// alpha::concurrent::internal::memory_slot_group_list::dump_log( alpha::concurrent::log_type::DUMP, 'a', 1 );
}
#if 0
TEST_F( TestLF_2_HighLoad, TC_ExSTACK_Profile3 )
{
	using TestType = std::size_t;
	// using TestType = int;

	std::cout << "--- pre-cpu kicking stack_list " << std::to_string( SUT_N ) << " ---" << std::endl;
	bool ret = nwoker_perf_test_pushpop_NtoN<alpha::concurrent::ex_lf_stack<TestType>, SUT_N>( THREAD_N, 1 );

	EXPECT_TRUE( ret );

	// alpha::concurrent::internal::memory_slot_group_list::dump_log( alpha::concurrent::log_type::DUMP, 'a', 1 );
}
#endif
