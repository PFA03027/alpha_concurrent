/**
 * @file perf_stack_comparison.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-06
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef SAMPLE_PERF_STACK_PERF_STACK_COMPARISON_HPP_
#define SAMPLE_PERF_STACK_PERF_STACK_COMPARISON_HPP_

#include <atomic>
#include <future>
#include <iostream>
#include <latch>
#include <list>
#include <mutex>
#include <new>
#include <vector>

// ===========================================================
constexpr std::size_t ReserveSize = 10000;

template <typename T>
class vec_mutex_stack {
public:
	using value_type = T;

	vec_mutex_stack( void )
	  : mtx_()
	  , vec_( ReserveSize )
	  , head_idx_( 0 )
	{
	}

	void push( value_type x )
	{
		std::lock_guard<std::mutex> lk( mtx_ );
		if ( head_idx_ >= ReserveSize ) {
			std::cout << "Overflow !" << std::endl;
			abort();
		}
		vec_[head_idx_] = x;
		head_idx_++;
	}
	std::tuple<bool, value_type> pop( void )
	{
		std::lock_guard<std::mutex> lk( mtx_ );
		if ( head_idx_ <= 0 ) {
			return std::tuple<bool, value_type> { false, 0 };
		}
		head_idx_--;

		value_type ans = vec_[head_idx_];
		return std::tuple<bool, value_type> { true, ans };
	}

private:
	std::mutex              mtx_;
	std::vector<value_type> vec_;
	std::size_t             head_idx_;
};

template <typename T>
class list_mutex_stack {
public:
	using value_type = T;

	list_mutex_stack( void )
	{
	}

	void push( value_type x )
	{
		std::lock_guard<std::mutex> lk( mtx_ );
		l_.emplace_back( x );
	}
	std::tuple<bool, value_type> pop( void )
	{
		std::lock_guard<std::mutex> lk( mtx_ );
		if ( l_.size() <= 0 ) {
			return std::tuple<bool, value_type> { false, 0 };
		}

		value_type ans = l_.back();
		l_.pop_back();
		return std::tuple<bool, value_type> { true, ans };
	}

private:
	std::mutex            mtx_;
	std::list<value_type> l_;
};

#endif
