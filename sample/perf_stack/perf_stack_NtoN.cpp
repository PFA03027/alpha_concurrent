/**
 * @file perf_stack.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-03-10
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 * hardware_concurrancy相当数のそれぞれのスレッドが、一定時間内に何回のループ処理が実行できるか？を計測することで性能を測定する。
 *
 * @note need C++20 to comple
 */

#include <atomic>
#include <future>
#include <iostream>
#include <list>
#include <mutex>
#include <new>
#include <vector>

#include "alconcurrent/lf_stack.hpp"

#include "perf_stack_NtoN.hpp"
#include "perf_stack_comparison.hpp"

// ===========================================================
using TestType = std::size_t;
// using TestType = int;

template <size_t SUT_N>
int nwoker_perf_test_stack_NtoN_sub( unsigned int nworker )
{
	std::cout << "--- alpha::concurrent::stack_list<> " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_stack_NtoN<alpha::concurrent::stack_list<TestType>, SUT_N>( nworker * 2, 1 );
	nwoker_perf_test_stack_NtoN<alpha::concurrent::stack_list<TestType>, SUT_N>( nworker, 1 );
	nwoker_perf_test_stack_NtoN<alpha::concurrent::stack_list<TestType>, SUT_N>( nworker / 2, 1 );
	nwoker_perf_test_stack_NtoN<alpha::concurrent::stack_list<TestType>, SUT_N>( 4, 1 );
	nwoker_perf_test_stack_NtoN<alpha::concurrent::stack_list<TestType>, SUT_N>( 1, 1 );

	std::cout << "--- x_stack_list " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_stack_NtoN<alpha::concurrent::x_stack_list<TestType>, SUT_N>( nworker * 2, 1 );
	nwoker_perf_test_stack_NtoN<alpha::concurrent::x_stack_list<TestType>, SUT_N>( nworker, 1 );
	nwoker_perf_test_stack_NtoN<alpha::concurrent::x_stack_list<TestType>, SUT_N>( nworker / 2, 1 );
	nwoker_perf_test_stack_NtoN<alpha::concurrent::x_stack_list<TestType>, SUT_N>( 4, 1 );
	nwoker_perf_test_stack_NtoN<alpha::concurrent::x_stack_list<TestType>, SUT_N>( 1, 1 );

	std::cout << "--- vec_mutex_stack " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_stack_NtoN<vec_mutex_stack<TestType>, SUT_N>( nworker * 2, 1 );
	nwoker_perf_test_stack_NtoN<vec_mutex_stack<TestType>, SUT_N>( nworker, 1 );
	nwoker_perf_test_stack_NtoN<vec_mutex_stack<TestType>, SUT_N>( nworker / 2, 1 );
	nwoker_perf_test_stack_NtoN<vec_mutex_stack<TestType>, SUT_N>( 4, 1 );
	nwoker_perf_test_stack_NtoN<vec_mutex_stack<TestType>, SUT_N>( 1, 1 );

	std::cout << "--- list_mutex_stack " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_stack_NtoN<list_mutex_stack<TestType>, SUT_N>( nworker * 2, 1 );
	nwoker_perf_test_stack_NtoN<list_mutex_stack<TestType>, SUT_N>( nworker, 1 );
	nwoker_perf_test_stack_NtoN<list_mutex_stack<TestType>, SUT_N>( nworker / 2, 1 );
	nwoker_perf_test_stack_NtoN<list_mutex_stack<TestType>, SUT_N>( 4, 1 );
	nwoker_perf_test_stack_NtoN<list_mutex_stack<TestType>, SUT_N>( 1, 1 );

	return 0;
}

int nwoker_perf_test_stack_NtoN_main( unsigned int nworker )
{
	nwoker_perf_test_stack_NtoN_sub<1>( nworker );
	std::cout << "*** number of retire_node: " << std::to_string( alpha::concurrent::internal::retire_node_abst::get_allocate_count() ) << " ***" << std::endl;
	nwoker_perf_test_stack_NtoN_sub<10>( nworker );
	std::cout << "*** number of retire_node: " << std::to_string( alpha::concurrent::internal::retire_node_abst::get_allocate_count() ) << " ***" << std::endl;
	nwoker_perf_test_stack_NtoN_sub<100>( nworker );
	std::cout << "*** number of retire_node: " << std::to_string( alpha::concurrent::internal::retire_node_abst::get_allocate_count() ) << " ***" << std::endl;

	return 0;
}