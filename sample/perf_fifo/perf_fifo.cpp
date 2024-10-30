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

#include <iostream>
#include <thread>

#include "../inc_common/perf_pushpop_NtoN.hpp"
#include "alconcurrent/lf_fifo.hpp"

int nwoker_perf_test_fifo_NtoN_main( unsigned int nworker );

int main( void )
{
	// std::cout << "std::hardware_destructive_interference_size = " << std::hardware_destructive_interference_size << std::endl;
	// std::cout << "std::hardware_destructive_interference_size = " << std::hardware_destructive_interference_size << std::endl;
	auto nworker = std::thread::hardware_concurrency();
	if ( nworker == 0 ) {
		std::cout << "hardware_concurrency is unknown, therefore let's select templary value. " << std::endl;
		nworker = 10;
	}

	nwoker_perf_test_fifo_NtoN_main( nworker );

	return EXIT_SUCCESS;
}
