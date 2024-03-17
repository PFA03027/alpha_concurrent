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

template <typename FIFOType>
std::tuple<std::size_t, typename FIFOType::value_type> worker_task_stack(
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

	return { count, pop_value };
}

template <typename FIFOType>
int nwoker_perf_test_stack( unsigned int nworker, FIFOType& sut )
{
	std::cout << "number of worker thread is " << nworker << std::endl;

	std::latch       start_sync_latch( nworker + 1 );
	std::atomic_bool loop_flag( true );
	using result_type = decltype( worker_task_stack<FIFOType>( start_sync_latch, loop_flag, sut ) );

	std::vector<std::future<result_type>> rets;
	rets.reserve( nworker );
	std::vector<std::thread> task_threads( nworker );
	for ( auto& t : task_threads ) {
		std::packaged_task<result_type()> task(
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
	std::size_t count_sum = 0;
	std::size_t total_sum = 0;
	for ( auto& r : rets ) {
		auto [count_ret, sum_ret] = r.get();
		count_sum += count_ret;
		total_sum += sum_ret;
	}

	std::cout << "result is count_sum: " << count_sum << "\t\ttotal sum: " << total_sum << "\t\t" << ( ( count_sum == total_sum ) ? "Good" : "FAILED" ) << std::endl;

	return EXIT_SUCCESS;
}

// ===========================================================
constexpr std::size_t ReserveSize = 10000;
using TestType                    = std::size_t;
// using TestType = int;

class vec_stack {
public:
	using value_type = TestType;

	vec_stack( void )
	  : mtx_()
	  , vec_( ReserveSize )
	  , head_idx_( 0 )
	{
	}

	void push( TestType x )
	{
		std::lock_guard<std::mutex> lk( mtx_ );
		if ( head_idx_ >= ReserveSize ) {
			std::cout << "Overflow !" << std::endl;
			abort();
		}
		vec_[head_idx_] = x;
		head_idx_++;
	}
	std::tuple<bool, TestType> pop( void )
	{
		std::lock_guard<std::mutex> lk( mtx_ );
		if ( head_idx_ <= 0 ) {
			return std::tuple<bool, TestType> { false, 0 };
		}
		head_idx_--;

		TestType ans = vec_[head_idx_];
		return std::tuple<bool, TestType> { true, ans };
	}

private:
	std::mutex            mtx_;
	std::vector<TestType> vec_;
	std::size_t           head_idx_;
};

class list_stack {
public:
	using value_type = TestType;

	list_stack( void )
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

		TestType ans = l_.back();
		l_.pop_back();
		return std::tuple<bool, TestType> { true, ans };
	}

private:
	std::mutex          mtx_;
	std::list<TestType> l_;
};

class fixsize_lf_stack {
public:
private:
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
	alpha::concurrent::stack_list<TestType> sut1( ReserveSize );
	vec_stack                               sut2;
	list_stack                              sut3;

	std::cout << "--- alpha::concurrent::stack_list<> ---" << std::endl;
	nwoker_perf_test_stack<alpha::concurrent::stack_list<TestType>>( nworker * 2, sut1 );
	nwoker_perf_test_stack<alpha::concurrent::stack_list<TestType>>( nworker, sut1 );
	nwoker_perf_test_stack<alpha::concurrent::stack_list<TestType>>( nworker / 2, sut1 );
	nwoker_perf_test_stack<alpha::concurrent::stack_list<TestType>>( 1, sut1 );
	std::cout << "--- vec_stack ---" << std::endl;
	nwoker_perf_test_stack<vec_stack>( nworker * 2, sut2 );
	nwoker_perf_test_stack<vec_stack>( nworker, sut2 );
	nwoker_perf_test_stack<vec_stack>( nworker / 2, sut2 );
	nwoker_perf_test_stack<vec_stack>( 1, sut2 );
	std::cout << "--- list_stack ---" << std::endl;
	nwoker_perf_test_stack<list_stack>( nworker * 2, sut3 );
	nwoker_perf_test_stack<list_stack>( nworker, sut3 );
	nwoker_perf_test_stack<list_stack>( nworker / 2, sut3 );
	nwoker_perf_test_stack<list_stack>( 1, sut3 );

	return EXIT_SUCCESS;
}
