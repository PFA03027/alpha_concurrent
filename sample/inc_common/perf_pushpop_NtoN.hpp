/**
 * @file perf_pushpop_NtoN.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-10-30
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef SAMPLE_PERF_INC_COMMON_PERF_PUSHPOP_NTON_HPP_
#define SAMPLE_PERF_INC_COMMON_PERF_PUSHPOP_NTON_HPP_

#include <algorithm>
#include <array>
#include <future>
#include <latch>
#include <random>
#include <thread>
#include <tuple>

template <typename FIFOType, size_t N>
void one_cycle_pushpop(
	std::array<FIFOType, N>& sut,
	std::array<size_t, N>&   cur_access_idxs_pop,
	std::array<size_t, N>&   cur_access_idxs_push,
	std::size_t&             count )
{
	for ( size_t i = 0; i < N; i++ ) {
		auto ret = sut[cur_access_idxs_pop[i]].pop();
		if ( !ret.has_value() ) {
			std::cout << "SUT has bug!!!" << std::endl;
			abort();
		}
		auto pop_value = ret.value();
		pop_value += 1;
		sut[cur_access_idxs_push[i]].push( pop_value );

		count++;
	}
}

template <typename FIFOType, size_t N>
std::tuple<std::size_t, typename FIFOType::value_type> worker_task_pushpop_NtoN(
	std::latch&              start_sync_latch,
	std::atomic_bool&        loop_flag,
	std::array<FIFOType, N>& sut )
{
	std::random_device    seed_gen;
	std::mt19937          engine( seed_gen() );
	std::size_t           count = 0;
	std::array<size_t, N> cur_access_idxs_pop;
	std::array<size_t, N> cur_access_idxs_push;

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

		one_cycle_pushpop( sut, cur_access_idxs_pop, cur_access_idxs_push, count );
	}
	size_t pop_value_sum = 0;
	for ( size_t i = 0; i < N; i++ ) {
		auto ret = sut[cur_access_idxs_pop[i]].pop();
		if ( !ret.has_value() ) {
			std::cout << "SUT has bug in completion phase!!!" << std::endl;
			abort();
		}
		pop_value_sum += ret.value();
	}

	return { count, pop_value_sum };
}

template <typename FIFOType, size_t N>
bool nwoker_perf_test_pushpop_NtoN( unsigned int nworker, unsigned int exec_sec )
{
	std::array<FIFOType, N> sut;

	std::cout << "[Conflictable Parallel] number of worker thread is " << nworker << ", N=" << std::to_string( N ) << " \t=-> ";

	std::latch       start_sync_latch( nworker + 1 );
	std::atomic_bool loop_flag( true );
	using result_type = decltype( worker_task_pushpop_NtoN<FIFOType, N>( start_sync_latch, loop_flag, sut ) );

	std::vector<std::future<result_type>> rets;
	rets.reserve( nworker );
	std::vector<std::thread> task_threads( nworker );
	for ( auto& t : task_threads ) {
		std::packaged_task<result_type()> task(
			[&start_sync_latch, &loop_flag, &sut]() {
				return worker_task_pushpop_NtoN<FIFOType, N>( start_sync_latch, loop_flag, sut );
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

	return ( count_sum == total_sum );
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

template <typename FIFOType>
std::tuple<std::size_t, typename FIFOType::value_type> worker_task_pushpop_One(
	std::latch&       start_sync_latch,
	std::atomic_bool& loop_flag,
	FIFOType&         sut )
{
	std::size_t count = 0;

	sut.push( 0 );

	start_sync_latch.arrive_and_wait();
	while ( loop_flag.load( std::memory_order_acquire ) ) {
		// シャッフル
		auto [sf, pop_value] = sut.pop();
		if ( !sf ) {
			std::cout << "SUT has bug!!!" << std::endl;
			abort();
		}
		pop_value += 1;
		sut.push( pop_value );

		count++;
	}
	size_t pop_value_sum = 0;

	auto [sf, pop_value] = sut.pop();
	if ( !sf ) {
		std::cout << "SUT has bug in completion phase!!!" << std::endl;
		abort();
	}
	pop_value_sum += pop_value;

	return { count, pop_value_sum };
}

template <typename FIFOType, size_t N>
bool nwoker_perf_test_pushpop_NParallel( unsigned int exec_sec )
{
	std::cout << "[Pure Parallel]         number of worker thread is " << std::to_string( N ) << ", N=" << std::to_string( N ) << " \t=-> ";

	std::latch       start_sync_latch( N + 1 );
	std::atomic_bool loop_flag( true );

	using result_type = std::tuple<std::size_t, typename FIFOType::value_type>;
	struct thread_data {
		FIFOType                 sut_;
		std::thread              worker_;
		std::future<result_type> ret_;
	};

	std::array<thread_data, N> tasks_and_threads;
	for ( auto& t : tasks_and_threads ) {
		std::packaged_task<result_type( std::latch&, std::atomic_bool&, FIFOType& )> task(
			[]( std::latch& sync_latch, std::atomic_bool& lf, FIFOType& sut ) {
				return worker_task_pushpop_One<FIFOType>( sync_latch, lf, sut );
			} );   // 非同期実行する関数を登録する
		t.ret_    = task.get_future();
		t.worker_ = std::thread( std::move( task ), std::ref( start_sync_latch ), std::ref( loop_flag ), std::ref( t.sut_ ) );
	}

	start_sync_latch.arrive_and_wait();
	sleep( exec_sec );
	loop_flag.store( false, std::memory_order_release );

	std::size_t count_sum = 0;
	std::size_t total_sum = 0;
	for ( auto& t : tasks_and_threads ) {
		auto [count_ret, sum_ret] = t.ret_.get();
		count_sum += count_ret;
		total_sum += sum_ret;
	}

	std::cout << "result is count_sum: " << count_sum << "\t\ttotal sum: " << total_sum << "\t\t" << ( ( count_sum == total_sum ) ? "Good" : "FAILED" ) << std::endl;

	for ( auto& t : tasks_and_threads ) {
		if ( t.worker_.joinable() ) {
			t.worker_.join();
		}
	}

	return ( count_sum == total_sum );
}

#endif
