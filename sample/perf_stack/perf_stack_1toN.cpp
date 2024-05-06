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
#include <latch>
#include <list>
#include <mutex>
#include <new>
#include <vector>

#include "alconcurrent/lf_stack.hpp"

#include "perf_stack_1toN.hpp"
#include "perf_stack_comparison.hpp"

// ===========================================================
using TestType = std::size_t;
// using TestType = int;

int nwoker_perf_test_stack_1toN_main( unsigned int nworker )
{
	std::cout << "--- x_stack_list ---" << std::endl;
	nwoker_perf_test_stack<alpha::concurrent::x_stack_list<TestType>>( 1 );

	std::cout << "--- alpha::concurrent::stack_list<> ---" << std::endl;
	nwoker_perf_test_stack<alpha::concurrent::stack_list<TestType>>( nworker * 2 );
	nwoker_perf_test_stack<alpha::concurrent::stack_list<TestType>>( nworker );
	nwoker_perf_test_stack<alpha::concurrent::stack_list<TestType>>( nworker / 2 );
	nwoker_perf_test_stack<alpha::concurrent::stack_list<TestType>>( 4 );
	nwoker_perf_test_stack<alpha::concurrent::stack_list<TestType>>( 1 );

	std::cout << "--- vec_mutex_stack ---" << std::endl;
	nwoker_perf_test_stack<vec_mutex_stack<TestType>>( nworker * 2 );
	nwoker_perf_test_stack<vec_mutex_stack<TestType>>( nworker );
	nwoker_perf_test_stack<vec_mutex_stack<TestType>>( nworker / 2 );
	nwoker_perf_test_stack<vec_mutex_stack<TestType>>( 4 );
	nwoker_perf_test_stack<vec_mutex_stack<TestType>>( 1 );

	std::cout << "--- list_mutex_stack ---" << std::endl;
	nwoker_perf_test_stack<list_mutex_stack<TestType>>( nworker * 2 );
	nwoker_perf_test_stack<list_mutex_stack<TestType>>( nworker );
	nwoker_perf_test_stack<list_mutex_stack<TestType>>( nworker / 2 );
	nwoker_perf_test_stack<list_mutex_stack<TestType>>( 4 );
	nwoker_perf_test_stack<list_mutex_stack<TestType>>( 1 );

	std::cout << "--- x_stack_list ---" << std::endl;
	nwoker_perf_test_stack<alpha::concurrent::x_stack_list<TestType>>( nworker * 2 );
	nwoker_perf_test_stack<alpha::concurrent::x_stack_list<TestType>>( nworker );
	nwoker_perf_test_stack<alpha::concurrent::x_stack_list<TestType>>( nworker / 2 );
	nwoker_perf_test_stack<alpha::concurrent::x_stack_list<TestType>>( 4 );
	nwoker_perf_test_stack<alpha::concurrent::x_stack_list<TestType>>( 1 );

	return EXIT_SUCCESS;
}
