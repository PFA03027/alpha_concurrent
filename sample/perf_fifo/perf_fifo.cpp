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

#include "alconcurrent/lf_fifo.hpp"

template <typename FIFOType>
std::size_t worker_task_stack(
	std::latch&       start_sync_latch,
	std::atomic_bool& loop_flag,
	FIFOType&         sut )
{
	std::size_t                   count = 0;
	typename FIFOType::value_type value = 0;

	sut.push( value );
	start_sync_latch.arrive_and_wait();
	while ( loop_flag.load( std::memory_order_acquire ) ) {
		auto [sf, pop_value] = sut.pop();
		if ( !sf ) {
			std::cout << "SUT has bug!!!" << std::endl;
			abort();
		}
		pop_value += 1;
		sut.push( pop_value );

		count++;
	}
	auto [sf, pop_value] = sut.pop();
	if ( !sf ) {
		std::cout << "SUT has bug in completion phase!!!" << std::endl;
		abort();
	}

	return count;
}

template <typename FIFOType>
int nwoker_perf_test_stack( unsigned int nworker, FIFOType& sut )
{
	std::cout << "number of worker thread is " << nworker << std::endl;

	std::latch       start_sync_latch( nworker + 1 );
	std::atomic_bool loop_flag( true );

	std::vector<std::future<std::size_t>> rets;
	rets.reserve( nworker );
	std::vector<std::thread> task_threads( nworker );
	for ( auto& t : task_threads ) {
		std::packaged_task<std::size_t()> task(
			[&start_sync_latch, &loop_flag, &sut]() {
				return worker_task_stack<FIFOType>( start_sync_latch, loop_flag, sut );
			} );   // 非同期実行する関数を登録する
		rets.emplace_back( task.get_future() );
		t = std::thread( std::move( task ) );
	}

	start_sync_latch.arrive_and_wait();
	sleep( 1 );
	loop_flag.store( false, std::memory_order_release );

	for ( auto& t : task_threads ) {
		if ( t.joinable() ) {
			t.join();
		}
	}
	std::size_t result = 0;
	for ( auto& r : rets ) {
		result += r.get();
	}

	std::cout << "result is " << result << std::endl;

	return EXIT_SUCCESS;
}

// ===========================================================
constexpr std::size_t ReserveSize = 10000;
using TestType                    = std::size_t;
// using TestType = int;

class vec_fifo {
public:
	using value_type = TestType;

	vec_fifo( void )
	  : mtx_()
	  , vec_( ReserveSize )
	  , pop_idx( 0 )
	  , push_idx( 0 )
	{
	}

	void push( TestType x )
	{
		std::lock_guard<std::mutex> lk( mtx_ );
		vec_[push_idx] = x;
		push_idx++;
		if ( push_idx >= ReserveSize ) {
			push_idx = 0;
		}
		if ( push_idx == pop_idx ) {
			std::cout << "Overflow !" << std::endl;
			abort();
		}
	}
	std::tuple<bool, TestType> pop( void )
	{
		std::lock_guard<std::mutex> lk( mtx_ );
		if ( push_idx == pop_idx ) {
			return std::tuple<bool, TestType> { false, 0 };
		}

		TestType ans = vec_[pop_idx];
		pop_idx++;
		if ( pop_idx >= ReserveSize ) {
			pop_idx = 0;
		}
		return std::tuple<bool, TestType> { true, ans };
	}

private:
	std::mutex            mtx_;
	std::vector<TestType> vec_;
	std::size_t           pop_idx;
	std::size_t           push_idx;
};

class list_fifo {
public:
	using value_type = TestType;

	list_fifo( void )
	{
	}

	void push( TestType x )
	{
		std::lock_guard<std::mutex> lk( mtx_ );
		l_.emplace_back( x );
	}
	std::tuple<bool, TestType> pop( void )
	{
		std::lock_guard<std::mutex> lk( mtx_ );
		if ( l_.size() <= 0 ) {
			return std::tuple<bool, TestType> { false, 0 };
		}

		TestType ans = l_.front();
		l_.pop_front();
		return std::tuple<bool, TestType> { true, ans };
	}

private:
	std::mutex          mtx_;
	std::list<TestType> l_;
};

int main( void )
{
	// std::cout << "std::hardware_destructive_interference_size = " << std::hardware_destructive_interference_size << std::endl;
	// std::cout << "std::hardware_destructive_interference_size = " << std::hardware_destructive_interference_size << std::endl;
	auto nworker = std::thread::hardware_concurrency();
	if ( nworker == 0 ) {
		std::cout << "hardware_concurrency is unknown, therefore let's select templary value. " << std::endl;
		nworker = 10;
	}
	alpha::concurrent::fifo_list<TestType> sut1( ReserveSize );
	vec_fifo                               sut2;
	list_fifo                              sut3;

	std::cout << "--- alpha::concurrent::fifo_list<> ---" << std::endl;
	nwoker_perf_test_stack<alpha::concurrent::fifo_list<TestType>>( nworker * 2, sut1 );
	nwoker_perf_test_stack<alpha::concurrent::fifo_list<TestType>>( nworker, sut1 );
	nwoker_perf_test_stack<alpha::concurrent::fifo_list<TestType>>( nworker / 2, sut1 );
	nwoker_perf_test_stack<alpha::concurrent::fifo_list<TestType>>( 1, sut1 );
	std::cout << "--- vec_fifo ---" << std::endl;
	nwoker_perf_test_stack<vec_fifo>( nworker * 2, sut2 );
	nwoker_perf_test_stack<vec_fifo>( nworker, sut2 );
	nwoker_perf_test_stack<vec_fifo>( nworker / 2, sut2 );
	nwoker_perf_test_stack<vec_fifo>( 1, sut2 );
	std::cout << "--- list_fifo ---" << std::endl;
	nwoker_perf_test_stack<list_fifo>( nworker * 2, sut3 );
	nwoker_perf_test_stack<list_fifo>( nworker, sut3 );
	nwoker_perf_test_stack<list_fifo>( nworker / 2, sut3 );
	nwoker_perf_test_stack<list_fifo>( 1, sut3 );

	return EXIT_SUCCESS;
}
