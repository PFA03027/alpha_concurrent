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

// ===========================================================
#ifdef __cpp_lib_hardware_interference_size
constexpr size_t default_align_size = std::hardware_destructive_interference_size;   // it is better to be equal to std::hardware_destructive_interference_size
#else
constexpr size_t default_align_size = 64;   // it is better to be equal to std::hardware_destructive_interference_size
#endif

template <typename T>
class fixsize_lf_stack {
	// static constexpr std::memory_order store_order = std::memory_order_seq_cst;
	// static constexpr std::memory_order load_order  = std::memory_order_seq_cst;
	// static constexpr std::memory_order rmw_order   = std::memory_order_seq_cst;
	static constexpr std::memory_order store_order = std::memory_order_release;
	static constexpr std::memory_order load_order  = std::memory_order_acquire;
	static constexpr std::memory_order rmw_order   = std::memory_order_acq_rel;

public:
	using value_type = T;

	fixsize_lf_stack( void )
	  : spin_cnt_( 0 )
	  , ap_head_valid_stack_( nullptr )
	  , ap_head_free_stack_( nullptr )
	{
		for ( size_t idx = 1; idx < ReserveSize; idx++ ) {
			array_nodes_[idx - 1].ap_next_.store( &( array_nodes_[idx] ), store_order );
		}
		array_nodes_[ReserveSize - 1].ap_next_.store( nullptr, store_order );

		ap_head_free_stack_.store( &( array_nodes_[0] ), store_order );
	}

	void push( value_type x )
	{
		node_type* p = stack_pop( ap_head_free_stack_, spin_cnt_ );
		p->val_.store( x, store_order );
		stack_push( ap_head_valid_stack_, p, spin_cnt_ );
	}
	std::tuple<bool, value_type> pop( void )
	{
		node_type* p = stack_pop( ap_head_valid_stack_, spin_cnt_ );
		if ( p == nullptr ) {
			return std::tuple<bool, value_type>( false, value_type {} );
		}
		value_type ans = p->val_.load( load_order );
		stack_push( ap_head_free_stack_, p, spin_cnt_ );
		return std::tuple<bool, value_type>( true, ans );
	}

	std::atomic<std::size_t> spin_cnt_;

private:
	struct node_type {
		alignas( default_align_size ) std::atomic<value_type> val_;   // implicitly requires default constructible.
		alignas( default_align_size ) std::atomic<node_type*> ap_next_;
		alignas( default_align_size ) std::atomic<std::size_t> ac_cnt_;   // accesser counter to protect

		class ac_ctrl_raii {
		public:
			ac_ctrl_raii( void )
			  : p_refing_( nullptr )
			{
			}
			ac_ctrl_raii( node_type* p_nt )
			  : p_refing_( p_nt )
			{
				if ( p_refing_ == nullptr ) return;
				p_refing_->ac_cnt_.fetch_add( 1, rmw_order );
			}
			~ac_ctrl_raii()
			{
				if ( p_refing_ == nullptr ) return;
				p_refing_->ac_cnt_.fetch_sub( 1, rmw_order );
			}
			ac_ctrl_raii( const ac_ctrl_raii& src )
			  : p_refing_( src.p_refing_ )
			{
				if ( p_refing_ == nullptr ) return;
				p_refing_->ac_cnt_.fetch_add( 1, rmw_order );
			}
			ac_ctrl_raii( ac_ctrl_raii&& src )
			  : p_refing_( src.p_refing_ )
			{
				src.p_refing_ = nullptr;
			}
			ac_ctrl_raii& operator=( const ac_ctrl_raii& src )
			{
				if ( this == &src ) return *this;
				if ( p_refing_ == src.p_refing_ ) return *this;

				if ( p_refing_ != nullptr ) {
					p_refing_->ac_cnt_.fetch_sub( 1, rmw_order );
				}
				if ( src.p_refing_ != nullptr ) {
					src.p_refing_->ac_cnt_.fetch_add( 1, rmw_order );
				}
				p_refing_ = src.p_refing_;
				return *this;
			}
			ac_ctrl_raii& operator=( ac_ctrl_raii&& src )
			{
				if ( this == &src ) return *this;
				if ( p_refing_ == src.p_refing_ ) {
					if ( src.p_refing_ != nullptr ) {
						src.p_refing_->ac_cnt_.fetch_sub( 1, rmw_order );
						src.p_refing_ = nullptr;
					}
				} else {
					if ( p_refing_ != nullptr ) {
						p_refing_->ac_cnt_.fetch_sub( 1, rmw_order );
					}
					p_refing_     = src.p_refing_;
					src.p_refing_ = nullptr;
				}
				return *this;
			}

		private:
			node_type* p_refing_;
		};

		node_type( void )
		  : val_ {}
		  , ap_next_( nullptr )
		  , ac_cnt_( 0 )
		{
		}

		ac_ctrl_raii protect( void )
		{
			return ac_ctrl_raii( this );
		}
	};

	static void stack_push( std::atomic<node_type*>& stack_head, node_type* p, std::atomic<std::size_t>& spin_cnt )
	{
#if 0
		std::size_t cnt = 0;
		while ( p->ac_cnt_.load( load_order ) != 0 ) {
			cnt++;
		}   // spin lock...
		spin_cnt.fetch_add( cnt );
#else
		while ( p->ac_cnt_.load( load_order ) != 0 ) {}   // spin lock...
#endif
		node_type* p_cur_head = stack_head.load( load_order );
		do {
			p->ap_next_.store( p_cur_head, store_order );
		} while ( !stack_head.compare_exchange_weak( p_cur_head, p, rmw_order ) );
		return;
	}

	static node_type* stack_pop( std::atomic<node_type*>& stack_head, std::atomic<std::size_t>& spin_cnt )
	{
		node_type* p_cur_head               = nullptr;
		node_type* p_updated_head_candidate = nullptr;
		p_cur_head                          = stack_head.load( load_order );
		{
			typename node_type::ac_ctrl_raii ac;
			do {
				while ( true ) {
					if ( p_cur_head == nullptr ) return nullptr;
					ac                        = p_cur_head->protect();
					node_type* p_desired_head = p_cur_head;
					if ( stack_head.compare_exchange_weak( p_cur_head, p_desired_head, rmw_order ) ) {
						break;   // weakで、ABA問題に対処している。。。つもりだが、x64ではダメかも。
					}
				}
				p_updated_head_candidate = p_cur_head->ap_next_.load( load_order );
			} while ( !stack_head.compare_exchange_weak( p_cur_head, p_updated_head_candidate, rmw_order ) );
		}
		return p_cur_head;
	}

	node_type array_nodes_[ReserveSize];
	alignas( default_align_size ) std::atomic<node_type*> ap_head_valid_stack_;
	alignas( default_align_size ) std::atomic<node_type*> ap_head_free_stack_;
};

// ===========================================================
using TestType = std::size_t;
// using TestType = int;

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
	vec_mutex_stack<TestType>               sut2;
	list_mutex_stack<TestType>              sut3;
	fixsize_lf_stack<TestType>              sut4;

	std::cout << "--- fixsize_lf_stack ---" << std::endl;
	nwoker_perf_test_stack<fixsize_lf_stack<TestType>>( nworker * 2, sut4 );
	std::cout << sut4.spin_cnt_.load() << std::endl;
	sut4.spin_cnt_.store( 0 );
	nwoker_perf_test_stack<fixsize_lf_stack<TestType>>( nworker, sut4 );
	std::cout << sut4.spin_cnt_.load() << std::endl;
	sut4.spin_cnt_.store( 0 );
	nwoker_perf_test_stack<fixsize_lf_stack<TestType>>( nworker / 2, sut4 );
	std::cout << sut4.spin_cnt_.load() << std::endl;
	sut4.spin_cnt_.store( 0 );
	nwoker_perf_test_stack<fixsize_lf_stack<TestType>>( 1, sut4 );
	std::cout << sut4.spin_cnt_.load() << std::endl;
	sut4.spin_cnt_.store( 0 );
	std::cout << "--- alpha::concurrent::stack_list<> ---" << std::endl;
	nwoker_perf_test_stack<alpha::concurrent::stack_list<TestType>>( nworker * 2, sut1 );
	nwoker_perf_test_stack<alpha::concurrent::stack_list<TestType>>( nworker, sut1 );
	nwoker_perf_test_stack<alpha::concurrent::stack_list<TestType>>( nworker / 2, sut1 );
	nwoker_perf_test_stack<alpha::concurrent::stack_list<TestType>>( 1, sut1 );
	std::cout << "--- vec_mutex_stack ---" << std::endl;
	nwoker_perf_test_stack<vec_mutex_stack<TestType>>( nworker * 2, sut2 );
	nwoker_perf_test_stack<vec_mutex_stack<TestType>>( nworker, sut2 );
	nwoker_perf_test_stack<vec_mutex_stack<TestType>>( nworker / 2, sut2 );
	nwoker_perf_test_stack<vec_mutex_stack<TestType>>( 1, sut2 );
	std::cout << "--- list_mutex_stack ---" << std::endl;
	nwoker_perf_test_stack<list_mutex_stack<TestType>>( nworker * 2, sut3 );
	nwoker_perf_test_stack<list_mutex_stack<TestType>>( nworker, sut3 );
	nwoker_perf_test_stack<list_mutex_stack<TestType>>( nworker / 2, sut3 );
	nwoker_perf_test_stack<list_mutex_stack<TestType>>( 1, sut3 );

	return EXIT_SUCCESS;
}
