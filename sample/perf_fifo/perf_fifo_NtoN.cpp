/**
 * @file perf_fifo_NtoN.cpp.cpp
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

#include "alconcurrent/lf_fifo.hpp"

#include "../inc_common/perf_pushpop_NtoN.hpp"

// ===========================================================
constexpr std::size_t ReserveSize = 10000;

template <typename T>
class vec_fifo {
public:
	using value_type = T;

	vec_fifo( void )
	  : mtx_()
	  , vec_( ReserveSize )
	  , pop_idx( 0 )
	  , push_idx( 0 )
	{
	}

	void push( value_type x )
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
	alpha::concurrent::return_optional<value_type> pop( void )
	{
		std::lock_guard<std::mutex> lk( mtx_ );
		if ( push_idx == pop_idx ) {
			return alpha::concurrent::return_nullopt;
		}

		value_type ans = vec_[pop_idx];
		pop_idx++;
		if ( pop_idx >= ReserveSize ) {
			pop_idx = 0;
		}
		return alpha::concurrent::return_optional<value_type> { ans };
	}

private:
	std::mutex              mtx_;
	std::vector<value_type> vec_;
	std::size_t             pop_idx;
	std::size_t             push_idx;
};

template <typename T>
class list_fifo {
public:
	using value_type = T;

	list_fifo( void )
	{
	}

	void push( value_type x )
	{
		std::lock_guard<std::mutex> lk( mtx_ );
		l_.emplace_back( x );
	}
	alpha::concurrent::return_optional<value_type> pop( void )
	{
		std::lock_guard<std::mutex> lk( mtx_ );
		if ( l_.size() <= 0 ) {
			return alpha::concurrent::return_nullopt;
		}

		value_type ans = l_.front();
		l_.pop_front();
		return alpha::concurrent::return_optional<value_type> { ans };
	}

private:
	std::mutex            mtx_;
	std::list<value_type> l_;
};

// ===========================================================
using TestType = std::size_t;
// using TestType = int;

template <size_t SUT_N>
int nwoker_perf_test_stack_NtoN_sub( unsigned int nworker )
{
	std::cout << "--- fifo_list " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_pushpop_NtoN<alpha::concurrent::fifo_list<TestType>, SUT_N>( nworker * 2, 1 );
	nwoker_perf_test_pushpop_NtoN<alpha::concurrent::fifo_list<TestType>, SUT_N>( nworker, 1 );
	nwoker_perf_test_pushpop_NtoN<alpha::concurrent::fifo_list<TestType>, SUT_N>( nworker / 2, 1 );
	nwoker_perf_test_pushpop_NtoN<alpha::concurrent::fifo_list<TestType>, SUT_N>( 4, 1 );
	nwoker_perf_test_pushpop_NtoN<alpha::concurrent::fifo_list<TestType>, SUT_N>( 2, 1 );
	nwoker_perf_test_pushpop_NtoN<alpha::concurrent::fifo_list<TestType>, SUT_N>( 1, 1 );

	std::cout << "--- vec_fifo " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_pushpop_NtoN<vec_fifo<TestType>, SUT_N>( nworker * 2, 1 );
	nwoker_perf_test_pushpop_NtoN<vec_fifo<TestType>, SUT_N>( nworker, 1 );
	nwoker_perf_test_pushpop_NtoN<vec_fifo<TestType>, SUT_N>( nworker / 2, 1 );
	nwoker_perf_test_pushpop_NtoN<vec_fifo<TestType>, SUT_N>( 4, 1 );
	nwoker_perf_test_pushpop_NtoN<vec_fifo<TestType>, SUT_N>( 2, 1 );
	nwoker_perf_test_pushpop_NtoN<vec_fifo<TestType>, SUT_N>( 1, 1 );

	std::cout << "--- list_fifo " << std::to_string( SUT_N ) << " ---" << std::endl;
	nwoker_perf_test_pushpop_NtoN<list_fifo<TestType>, SUT_N>( nworker * 2, 1 );
	nwoker_perf_test_pushpop_NtoN<list_fifo<TestType>, SUT_N>( nworker, 1 );
	nwoker_perf_test_pushpop_NtoN<list_fifo<TestType>, SUT_N>( nworker / 2, 1 );
	nwoker_perf_test_pushpop_NtoN<list_fifo<TestType>, SUT_N>( 4, 1 );
	nwoker_perf_test_pushpop_NtoN<list_fifo<TestType>, SUT_N>( 2, 1 );
	nwoker_perf_test_pushpop_NtoN<list_fifo<TestType>, SUT_N>( 1, 1 );

	return 0;
}

int nwoker_perf_test_fifo_NtoN_main( unsigned int nworker )
{
	nwoker_perf_test_stack_NtoN_sub<1>( nworker );
	// std::cout << "*** number of retire_node: " << std::to_string( alpha::concurrent::internal::retire_node_abst::get_allocate_count() ) << " ***" << std::endl;
	nwoker_perf_test_stack_NtoN_sub<10>( nworker );
	// std::cout << "*** number of retire_node: " << std::to_string( alpha::concurrent::internal::retire_node_abst::get_allocate_count() ) << " ***" << std::endl;
	nwoker_perf_test_stack_NtoN_sub<100>( nworker );
	// std::cout << "*** number of retire_node: " << std::to_string( alpha::concurrent::internal::retire_node_abst::get_allocate_count() ) << " ***" << std::endl;

	return 0;
}