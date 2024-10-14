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

#include "alconcurrent/lf_mem_alloc_type.hpp"
#include "alconcurrent/lf_stack.hpp"

#include "../../../sample/perf_stack/perf_stack_NtoN.hpp"

///////////////////////////////////////////////////////////////////////////////
class TestLFSTACK_2_HighLoad : public ::testing::Test {
protected:
	void SetUp() override
	{
		alpha::concurrent::GetErrorWarningLogCountAndReset( nullptr, nullptr );
	}

	void TearDown() override
	{
		alpha::concurrent::internal::retire_mgr::stop_prune_thread();

		alpha::concurrent::internal::hazard_ptr_mgr::DestoryAll();

		int cw, ce;
		alpha::concurrent::GetErrorWarningLogCountAndReset( &ce, &cw );
		EXPECT_EQ( ce, 0 );
		EXPECT_EQ( cw, 0 );
	}
};

constexpr unsigned int   num_thread = 10;   // Tested until 128.
constexpr std::uintptr_t loop_num   = 20000000 / num_thread;

pthread_barrier_t barrier2;

/**
 * 各スレッドのメインルーチン。
 * カウントアップを繰り返す。
 */
long func_test_stack_list( alpha::concurrent::stack_list<long>* p_test_obj )
{

	pthread_barrier_wait( &barrier2 );

	typename alpha::concurrent::stack_list<long>::value_type v = 0;
	for ( std::uintptr_t i = 0; i < loop_num; i++ ) {
		p_test_obj->push( v );
#if ( __cplusplus >= 201703L /* check C++17 */ ) && defined( __cpp_structured_bindings )
		auto [pop_flag, vv] = p_test_obj->pop();
#else
		auto local_ret = p_test_obj->pop();
		auto pop_flag  = std::get<0>( local_ret );
		auto vv        = std::get<1>( local_ret );
#endif
		if ( !pop_flag ) {
			printf( "Bugggggggyyyy  func_test_fifo()!!!  %s\n", std::to_string( v ).c_str() );
			exit( 1 );
		}
		v = vv + 1;
	}

	return v;
}

constexpr size_t SUT_N    = 10;
constexpr size_t THREAD_N = 20;

#if 1
TEST_F( TestLFSTACK_2_HighLoad, TC_Profile1 )
{
	using TestType = std::size_t;
	// using TestType = int;

	std::cout << "--- pre-cpu kicking stack_list " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_stack_NtoN<alpha::concurrent::stack_list<TestType>, SUT_N>( THREAD_N, 1 );
}
TEST_F( TestLFSTACK_2_HighLoad, TC_Profile2 )
{
	using TestType = std::size_t;
	// using TestType = int;

	std::cout << "--- stack_list " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_stack_NtoN<alpha::concurrent::stack_list<TestType>, SUT_N>( THREAD_N, 10 );
}
#endif
#if 0
TEST_F( TestLFSTACK_2_HighLoad, TC_Profile3 )
{
	using TestType = std::size_t;
	// using TestType = int;

	std::cout << "--- pre-cpu kicking obsolate_stack_list " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_stack_NtoN<alpha::concurrent::obsolate_stack_list<TestType>, SUT_N>( THREAD_N, 1 );
}
TEST_F( TestLFSTACK_2_HighLoad, TC_Profile4 )
{
	using TestType = std::size_t;
	// using TestType = int;

	std::cout << "--- obsolate_stack_list " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_stack_NtoN<alpha::concurrent::obsolate_stack_list<TestType>, SUT_N>( THREAD_N, 10 );
}
#endif
