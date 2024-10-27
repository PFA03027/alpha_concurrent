/**
 * @file perf_stack_1toN.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-06
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef SAMPLE_PERF_STACK_PERF_STACK_NTON_HPP_
#define SAMPLE_PERF_STACK_PERF_STACK_NTON_HPP_

#include <algorithm>
#include <array>
#include <latch>
#include <random>
#include <thread>
#include <tuple>

template <typename FIFOType, size_t N>
std::tuple<std::size_t, typename FIFOType::value_type> worker_task_stack_NtoN(
	std::latch&              start_sync_latch,
	std::atomic_bool&        loop_flag,
	std::array<FIFOType, N>& sut )
{
	static std::random_device seed_gen;
	std::mt19937              engine( seed_gen() );
	std::size_t               count = 0;
	std::array<size_t, N>     cur_access_idxs_pop;
	std::array<size_t, N>     cur_access_idxs_push;

	for ( size_t i = 0; i < N; i++ ) {
		cur_access_idxs_pop[i]  = i;
		cur_access_idxs_push[i] = i;
		sut[i].push( 0 );
	}

	start_sync_latch.arrive_and_wait();
	while ( loop_flag.load( std::memory_order_acquire ) ) {
		// シャッフル
		std::shuffle( cur_access_idxs_pop.begin(), cur_access_idxs_pop.end(), engine );
		std::shuffle( cur_access_idxs_push.begin(), cur_access_idxs_push.end(), engine );
		for ( size_t i = 0; i < N; i++ ) {
			auto [sf, pop_value] = sut[cur_access_idxs_pop[i]].pop();
			if ( !sf ) {
				std::cout << "SUT has bug!!!" << std::endl;
				abort();
			}
			pop_value += 1;
			sut[cur_access_idxs_push[i]].push( pop_value );

			count++;
		}
	}
	size_t pop_value_sum = 0;
	for ( size_t i = 0; i < N; i++ ) {
		auto [sf, pop_value] = sut[cur_access_idxs_pop[i]].pop();
		if ( !sf ) {
			std::cout << "SUT has bug in completion phase!!!" << std::endl;
			abort();
		}
		pop_value_sum += pop_value;
	}

	return { count, pop_value_sum };
}

template <typename FIFOType, size_t N>
int nwoker_perf_test_stack_NtoN( unsigned int nworker, unsigned int exec_sec )
{
	std::array<FIFOType, N> sut;

	std::cout << "number of worker thread is " << nworker << ", N=" << std::to_string( N ) << std::endl;

	std::latch       start_sync_latch( nworker + 1 );
	std::atomic_bool loop_flag( true );
	using result_type = decltype( worker_task_stack_NtoN<FIFOType, N>( start_sync_latch, loop_flag, sut ) );

	std::vector<std::future<result_type>> rets;
	rets.reserve( nworker );
	std::vector<std::thread> task_threads( nworker );
	for ( auto& t : task_threads ) {
		std::packaged_task<result_type()> task(
			[&start_sync_latch, &loop_flag, &sut]() {
				return worker_task_stack_NtoN<FIFOType, N>( start_sync_latch, loop_flag, sut );
			} );   // 非同期実行する関数を登録する
		rets.emplace_back( task.get_future() );
		t = std::thread( std::move( task ) );
	}

	start_sync_latch.arrive_and_wait();
	sleep( exec_sec );
	loop_flag.store( false, std::memory_order_release );

	for ( auto& t : task_threads ) {
		if ( t.joinable() ) {
			t.join();
		}
	}
	std::size_t count_sum = 0;
	std::size_t total_sum = 0;
	for ( auto& r : rets ) {
		auto [count_ret, sum_ret] = r.get();
		count_sum += count_ret;
		total_sum += sum_ret;
	}

	std::cout << "result is count_sum: " << count_sum << "\t\ttotal sum: " << total_sum << "\t\t" << ( ( count_sum == total_sum ) ? "Good" : "FAILED" ) << std::endl;

	return 0;
}

int nwoker_perf_test_stack_NtoN_main( unsigned int nworker );

#endif
