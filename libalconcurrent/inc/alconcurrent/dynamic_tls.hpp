/*!
 * @file	dynamic_tls.hpp
 * @brief	Dynamic allocatable thread local storage
 * @author	Teruaki Ata
 * @date	Created on 2021/01/04
 * @details	This class provide thread local storage dynamic allocation.
 *
 * This is requires POSIX thread local storage APIs.
 *
 * C++11で、thread_localが使用可能となるが、g++でコンパイルした場合、スレッド終了時のdestructor処理実行時に、
 * すでにメモリ領域が破壊されている場合があるため、destructor処理の正常動作が期待できない。
 * (スレッド終了時のpthread_key_create()で登録したデストラクタ関数の呼ばれる振る舞いから、thread_localのメモリ領域破棄とdestructorの実行が並行して行われている模様)
 *
 * また、クラスメンバ変数や、動的に確保することはできないため、POSIX thread local storage APIを使用して、動的に確保できるスレッドローカルストレージクラスを用意する。
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef ALCONCCURRENT_INC_DYNAMIC_TLS_HPP_
#define ALCONCCURRENT_INC_DYNAMIC_TLS_HPP_

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "conf_logger.hpp"

namespace alpha {
namespace concurrent {

extern std::recursive_mutex dynamic_tls_global_exclusive_control_for_destructions;   //!< to avoid rece condition b/w thread local destruction and normal destruction globally

namespace internal {

constexpr size_t       ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE          = 1024 * 2;   // configuration value
constexpr unsigned int ALCONCURRENT_CONF_DYNAMIC_TLS_DESTUCT_ITERATE_MAX = 10;

struct dynamic_tls_key;
using dynamic_tls_key_t = dynamic_tls_key*;   //!< pointer to dynamic_tls_key as a key

struct dynamic_tls_status_info {
	unsigned int num_key_array_cnt_;
	unsigned int num_content_head_;
	unsigned int next_base_idx_;
};

enum class op_ret {
	INVALID,        //!< p_data_ data is invalid
	SUCCESS,        //!< p_data_ data is valid
	INVALID_KEY,    //!< p_data_ data is invalid by key is invalid
	OUT_OF_RANGE,   //!< p_data_ data is invalid by key is out of range
	UNEXPECT_ERR    //!< p_data_ data is invalid by unexpected error
};
struct get_result {
	op_ret    stat_;     //!< 取得処理に成功した場合にop_ret::SUCCESSとなる。それ以外の場合は、何らかのエラーによる失敗を示す。
	uintptr_t p_data_;   //!< 取得処理に成功した場合に、取得した値が保持される。取得に失敗した場合は、不定値となる。

	constexpr get_result( void )
	  : stat_( op_ret::INVALID )
	  , p_data_( 0U )
	{
	}
	constexpr get_result( op_ret stat_arg, uintptr_t p_data_arg )
	  : stat_( stat_arg )
	  , p_data_( p_data_arg )
	{
	}
};

class dynamic_tls_key_scoped_accessor {
public:
	dynamic_tls_key_scoped_accessor( dynamic_tls_key_t key, op_ret stat, void* p );
#ifdef ALCONCURRENT_CONF_ENABLE_INDIVIDUAL_KEY_EXCLUSIVE_ACCESS
	~dynamic_tls_key_scoped_accessor();
#endif

	op_ret     set_value( uintptr_t data );
	get_result get_value( void );

	const op_ret stat_;   //!< 取得処理に成功した場合にop_ret::SUCCESSとなる。それ以外の場合は、何らかのエラーによる失敗を示す。

private:
	dynamic_tls_key_t key_;   //!< アクセス時の補助データとして使用する。
	void* const       p_;     //!< 内部のスレッドローカルデータ構造体へのポインタ

	friend dynamic_tls_key_scoped_accessor dynamic_tls_getspecific_accessor( dynamic_tls_key_t key );
};

/*!
 * @brief	create a key of dynamic thread local storage and count up the number of dynamic thread local memory
 *
 *　動的thread local storageを割り当て、割り当て数を１つ増やす。
 *　割り当て数は不具合解析など使用する。
 */
dynamic_tls_key_t dynamic_tls_key_create( void* p_param, uintptr_t ( *allocator )( void* p_param ), void ( *deallocator )( uintptr_t tls_data, void* p_param ) );

/*!
 * @brief	release a key of dynamic thread local storage and count down the number of dynamic thread local memory
 *
 *　動的thread local storageの割り当てを解除し、割り当て数を１つ減らす。
 *　割り当て数は不具合解析など使用する。
 */
void dynamic_tls_key_release( dynamic_tls_key_t key );

/*!
 * @brief	set a value to a thread local storage
 *
 */
op_ret dynamic_tls_setspecific( dynamic_tls_key_t key, uintptr_t tls_data );

/*!
 * @brief	get a value from a thread local storage
 *
 */
get_result dynamic_tls_getspecific( dynamic_tls_key_t key );

/*!
 * @brief	get an accessor to a thread local storage
 *
 */
dynamic_tls_key_scoped_accessor dynamic_tls_getspecific_accessor( dynamic_tls_key_t key );

/**
 * @brief get dynamic thread local storage status
 *
 * @return dynamic_tls_status_info
 */
dynamic_tls_status_info dynamic_tls_get_status( void );

/*!
 * @brief	get the number of dynamic thread local memory
 *
 *　pthread_key_create()での割り当て数を取得する。
 *　不具合解析など使用する。
 */
int get_num_of_tls_key( void );

/*!
 * @brief	get the max number of dynamic thread local memory
 *
 *　pthread_key_create()での割り当て数の最大値を取得する。
 *　不具合解析など使用する。
 */
int get_max_num_of_tls_key( void );

/**
 * @brief thread count information container for dynamic thread local variable class
 *
 */
struct dynamic_tls_thread_cnt {
	constexpr dynamic_tls_thread_cnt( void )
	  : cur_thread_count_( 0 )
	  , max_thread_count_( 0 )
	{
	}
	constexpr dynamic_tls_thread_cnt( const dynamic_tls_thread_cnt& ) = default;
	constexpr dynamic_tls_thread_cnt( dynamic_tls_thread_cnt&& )      = default;
#if ( __cpp_constexpr >= 201304 )
	constexpr dynamic_tls_thread_cnt& operator=( const dynamic_tls_thread_cnt& ) = default;
	constexpr dynamic_tls_thread_cnt& operator=( dynamic_tls_thread_cnt&& )      = default;
#else
	dynamic_tls_thread_cnt& operator=( const dynamic_tls_thread_cnt& ) = default;
	dynamic_tls_thread_cnt& operator=( dynamic_tls_thread_cnt&& )      = default;
#endif

	void count_up( void )
	{
		auto cur     = cur_thread_count_.fetch_add( 1, std::memory_order_acq_rel ) + 1;
		auto cur_max = max_thread_count_.load( std::memory_order_acquire );
		if ( cur > cur_max ) {
			max_thread_count_.compare_exchange_strong( cur_max, cur, std::memory_order_acq_rel );
		}
	}

	void count_down( void )
	{
		cur_thread_count_.fetch_sub( 1, std::memory_order_acq_rel );
	}

	std::atomic_int cur_thread_count_;   //!< current thread count
	std::atomic_int max_thread_count_;   //!< max thread count
};

}   // namespace internal

/*!
 * @brief	スレッド生成時、終了時にスレッドローカルストレージに対して処理を行うハンドラクラス
 *
 * @tparam 	T	ファンクタに引数として渡される型。
 *
 * このファンクタ自身は、スレッド生成時には、型Tのクラスをデフォルト構築し、スレッド終了時には何もしないというファンクタとして動作する。
 *
 *
 * @brief Handler class that processes thread local storage at thread creation and termination
 *
 * @tparam The type passed as an argument to the T functor.
 *
 * This functor itself operates as a functor that constructs a class of type T by default when a thread is created and does nothing when the thread terminates.
 */
template <typename T>
struct threadlocal_default_handler {
	/**
	 * @brief 型Tのオブジェクトを生成するファクトリ関数
	 *
	 * 型Tのオブジェクトはデフォルトコンストラクタで生成される
	 */
	uintptr_t allocate( void )
	{
		return reinterpret_cast<uintptr_t>( new T() );
	}

	/*!
	 * @brief スレッド終了時に、allocateで生成されたオブジェクトの破棄処理を行う関数
	 */
	void deallocate(
		uintptr_t p_data   //!< [in] allocateで生成されたオブジェクトへのポインタ
	)
	{
		delete reinterpret_cast<T*>( p_data );
	}
};

/*!
 * @brief	動的スレッドローカルストレージを実現するクラス
 *
 * スレッド終了時にスレッドローカルストレージをdestructする前に、コンストラクタで指定されたTL_PRE_DESTRUCTORのインスタンスが呼び出される。
 * このクラスのインスタンス変数自体がdestructされる場合は、呼び出されない。
 *
 * @tparam [in]	T	スレッドローカルストレージとして確保する型
 * @tparam [in]	TL_HANDLER	スレッド終了時、およびインスタンス自身がdestructされる時に、呼び出されるファンクタ。例えばTが何らかのクラスへのポインタ型である場合、その解放処理制御に使用する。
 *
 * @warn
 * デストラクタ処理と、スレッド終了処理によるdestr_fn()の同時呼び出しは、不定動作とする。　@n
 * よって、インスタンスの破棄とスレッドの終了処理を同時に発生しないようにすること。
 *
 * @warn
 * Tの実体の長さが可変であるようなクラス/構造体は未サポートのため使用不可。
 *
 * @note
 * lf_mem_allocクラスで使用するため、
 * コンポーネントの上下関係から、メモリの確保にはnewを使用せず、alloc_only_chamber allocatorと配置newのみを使用する。
 *
 * @brief Class that realizes dynamic thread local storage
 *
 * At the end of the thread, the instance of TL_HANDLER specified in the constructor is called before destructing the thread local storage.
 * If the instance variable of this class itself is destroyed, it will not be called.
 *
 * @tparam [in] T Type reserved as thread local storage
 * @tparam [in] TL_HANDLER A functor that is called at the end of a thread and when the instance itself is destroyed
 *
 * @warn
 * Simultaneous call of dealloc_fn () by destructor processing and thread termination processing is an indefinite operation. @n
 * Therefore, do not destroy the instance and terminate the thread at the same time.
 *
 * @warn
 * Not support T that Actual data size is variable
 *
 * @note
 * Because it is used in the lf_mem_alloc class
 * Due to the hierarchical relationship of components, new is not used to allocate memory, only alloc_only_chamber allocator and placement new are used.
 */
template <typename T, typename TL_HANDLER = threadlocal_default_handler<T>>
class dynamic_tls {
public:
	using value_type      = T;
	using value_reference = T&;

	class scoped_accessor {
	public:
		T& ref( void )
		{
			return *p_;
		}

	private:
		scoped_accessor( internal::dynamic_tls_key_scoped_accessor&& ac_arg )
		  : accessor_( ac_arg )
		  , p_( nullptr )
		{
			internal::get_result ret = accessor_.get_value();
			if ( ret.stat_ != internal::op_ret::SUCCESS ) {
				throw std::bad_alloc();
			}
			p_ = reinterpret_cast<T*>( ret.p_data_ );
		}

		internal::dynamic_tls_key_scoped_accessor accessor_;
		T*                                        p_;

		friend dynamic_tls;
	};

	constexpr dynamic_tls( void )
	  : tls_key_( nullptr )
	  , tl_handler_()
	  , th_cnt_()
	{
		static_assert( std::is_standard_layout<dynamic_tls>::value, "dynamic_tls should be standard-layout type" );
	}

	constexpr dynamic_tls( const TL_HANDLER& tl_dest_functor_arg )
	  : tls_key_( nullptr )
	  , tl_handler_( tl_dest_functor_arg )
	  , th_cnt_()
	{
		static_assert( std::is_standard_layout<dynamic_tls>::value, "dynamic_tls should be standard-layout type" );
	}

	constexpr dynamic_tls( TL_HANDLER&& tl_dest_functor_arg )
	  : tls_key_( nullptr )
	  , tl_handler_( std::move( tl_dest_functor_arg ) )
	  , th_cnt_()
	{
		static_assert( std::is_standard_layout<dynamic_tls>::value, "dynamic_tls should be standard-layout type" );
	}

	~dynamic_tls()
	{
		internal::LogOutput( log_type::DEBUG, "dynamic_tls::destructor is called" );

		std::lock_guard<std::recursive_mutex> lg( dynamic_tls_global_exclusive_control_for_destructions );

		auto tmp_key = tls_key_.load( std::memory_order_acquire );
		if ( tmp_key == nullptr ) return;
		internal::dynamic_tls_key_release( tmp_key );
		tls_key_.store( nullptr, std::memory_order_release );
	}

	/**
	 * @brief gets a reference of thread local data
	 *
	 * If a call of this I/F by a thread is first, this I/F will allocate a thread local memory by TL_HANDLER
	 *
	 * @return a reference of thread local data
	 *
	 * @exception if fail to allocate thread local data, throw std::bad_alloc
	 */
	value_reference get_tls_instance( void )
	{
		auto tmp_ret = internal::dynamic_tls_getspecific( tls_key_chk_and_get() );
		if ( tmp_ret.stat_ != internal::op_ret::SUCCESS ) {
			throw std::bad_alloc();
		}

		// printf( "XXXXXXXXXXX tmp_ret.p_data_ = %lx\n", tmp_ret.p_data_ );
		return *( reinterpret_cast<value_pointer>( tmp_ret.p_data_ ) );
	}

	/**
	 * @brief gets an accessor of thread local data
	 *
	 * @return an accessor of thread local data
	 *
	 * @exception if fail to allocate thread local data, throw std::bad_alloc
	 */
	scoped_accessor get_tls_accessor( void )
	{
		return internal::dynamic_tls_getspecific_accessor( tls_key_chk_and_get() );
	}

	/**
	 * @brief get thread count information
	 *
	 * @return 1st: current thread count
	 * @return 2nd: max thread count
	 */
	std::pair<int, int> get_thread_count_info( void ) const
	{
		return std::pair<int, int>(
			th_cnt_.cur_thread_count_.load( std::memory_order_acquire ),
			th_cnt_.max_thread_count_.load( std::memory_order_acquire ) );
	}

private:
	using value_pointer = T*;

	inline internal::dynamic_tls_key_t tls_key_chk_and_get( void )
	{
		internal::dynamic_tls_key_t ans = tls_key_.load( std::memory_order_acquire );
		if ( ans != nullptr ) return ans;

		auto alloc_ans = internal::dynamic_tls_key_create( this, alloc_fn, dealloc_fn );
		if ( tls_key_.compare_exchange_strong( ans, alloc_ans, std::memory_order_acq_rel ) ) return alloc_ans;
		dynamic_tls_key_release( alloc_ans );

		ans = tls_key_.load( std::memory_order_acquire );
		if ( ans != nullptr ) return ans;

		// ans == nullptr
		internal::LogOutput( log_type::ERR, "dynamic tls key creation and deletion has happened in same time. this will be happened by user side bug." );
		throw std::runtime_error( "dynamic tls key creation and deletion has happened in same time. this will be happened by user side bug like dynamic_tls::XXX calling and destructing this instance." );
	}

	static uintptr_t alloc_fn( void* p_param )
	{
		internal::LogOutput( log_type::DEBUG, "dynamic_tls::alloc_fn is called              - p_param=%p", p_param );
		dynamic_tls* p_this_param = reinterpret_cast<dynamic_tls*>( p_param );

		if ( p_this_param == nullptr ) {
			internal::LogOutput( log_type::ERR, "dynamic_tls::alloc_fn is called but p_param is nullptr" );
			return 0U;   // なぜかnullptrで呼び出された。多分pthread内でのrace conditionのせい。どうしようもないので、諦める。
		}

		p_this_param->th_cnt_.count_up();
		return p_this_param->tl_handler_.allocate();
	}

	static void dealloc_fn( uintptr_t p_obj, void* p_param )
	{
		internal::LogOutput( log_type::DEBUG, "dynamic_tls::dealloc_fn is called              - p_obj=%p, p_param=%p", p_param );
		dynamic_tls* p_this_param = reinterpret_cast<dynamic_tls*>( p_param );

		if ( p_obj == 0U ) {
			internal::LogOutput( log_type::DEBUG, "dynamic_tls::dealloc_fn is called but p_obj is nullptr" );
			return;   // なぜかnullptrで呼び出された。多分pthread内でのrace conditionのせい。どうしようもないので、諦める。
		}
		if ( p_param == nullptr ) {
			internal::LogOutput( log_type::ERR, "dynamic_tls::dealloc_fn is called but p_param is nullptr" );
			return;   // なぜかnullptrで呼び出された。多分pthread内でのrace conditionのせい。どうしようもないので、諦める。
		}

		p_this_param->tl_handler_.deallocate( p_obj );
		p_this_param->th_cnt_.count_down();
		return;
	}

	std::atomic<internal::dynamic_tls_key_t> tls_key_;   //!<	key for thread local storage of POSIX.

	TL_HANDLER tl_handler_;   //!< functor to clean-up the resources when thread is terminated.

	internal::dynamic_tls_thread_cnt th_cnt_;   //!< thread count information
};

/**
 * @brief ポインタ型は部分特殊化を行う
 *
 * @tparam T
 * @tparam TL_HANDLER
 */
template <typename T, typename TL_HANDLER>
class dynamic_tls<T*, TL_HANDLER> {
public:
	using value_type = T*;

	class scoped_accessor {
	public:
		T* get( void )
		{
			internal::get_result ret = accessor_.get_value();
			if ( ret.stat_ != internal::op_ret::SUCCESS ) {
				throw std::bad_alloc();
			}
			return reinterpret_cast<T*>( ret.p_data_ );
		}

		void set( T* storing_data )
		{
			internal::op_ret ret = accessor_.set_value( reinterpret_cast<uintptr_t>( storing_data ) );
			if ( ret != internal::op_ret::SUCCESS ) {
				throw std::bad_alloc();
			}
		}

	private:
		scoped_accessor( internal::dynamic_tls_key_scoped_accessor&& ac_arg )
		  : accessor_( ac_arg )
		{
			if ( accessor_.stat_ != internal::op_ret::SUCCESS ) {
				throw std::bad_alloc();
			}
		}

		internal::dynamic_tls_key_scoped_accessor accessor_;

		friend dynamic_tls;
	};

	constexpr dynamic_tls( void )
	  : tls_key_( nullptr )
	  , tl_handler_()
	  , th_cnt_()
	{
		static_assert( std::is_standard_layout<dynamic_tls>::value, "dynamic_tls should be standard-layout type" );
	}

	constexpr dynamic_tls( const TL_HANDLER& tl_dest_functor_arg )
	  : tls_key_( nullptr )
	  , tl_handler_( tl_dest_functor_arg )
	  , th_cnt_()
	{
		static_assert( std::is_standard_layout<dynamic_tls>::value, "dynamic_tls should be standard-layout type" );
	}

	constexpr dynamic_tls( TL_HANDLER&& tl_dest_functor_arg )
	  : tls_key_( nullptr )
	  , tl_handler_( std::move( tl_dest_functor_arg ) )
	  , th_cnt_()
	{
		static_assert( std::is_standard_layout<dynamic_tls>::value, "dynamic_tls should be standard-layout type" );
	}

	~dynamic_tls()
	{
		// internal::LogOutput( log_type::DEBUG, "dynamic_tls<T*>::destructor is called" );

		std::lock_guard<std::recursive_mutex> lg( dynamic_tls_global_exclusive_control_for_destructions );

		auto tmp_key = tls_key_.load( std::memory_order_acquire );
		if ( tmp_key == nullptr ) return;
		internal::dynamic_tls_key_release( tmp_key );
		tls_key_.store( nullptr, std::memory_order_release );
	}

	/**
	 * @brief gets a value of thread local data
	 *
	 * If a call of this I/F by a thread is first, this I/F will allocate a thread local data by TL_HANDLER
	 *
	 * @return value of thread local data
	 *
	 * @exception if fail to allocate thread local data, throw std::bad_alloc
	 */
	value_type get_tls_instance( void )
	{
		auto tmp_ret = internal::dynamic_tls_getspecific( tls_key_chk_and_get() );
		if ( tmp_ret.stat_ != internal::op_ret::SUCCESS ) {
			throw std::bad_alloc();
		}

		// printf( "YYYYYYYYYYY tmp_ret.p_data_ = %lx\n", tmp_ret.p_data_ );
		return reinterpret_cast<value_type>( tmp_ret.p_data_ );
	}

	/**
	 * @brief Set the value to thread local data
	 *
	 * @param p_data the value to be set
	 *
	 * @exception if fail to allocate thread local data, throw std::bad_alloc
	 */
	void set_value_to_tls_instance( value_type p_data )
	{
		auto tmp_ret = internal::dynamic_tls_setspecific( tls_key_chk_and_get(), reinterpret_cast<uintptr_t>( p_data ) );
		if ( tmp_ret != internal::op_ret::SUCCESS ) {
			throw std::bad_alloc();
		}
		return;
	}

	/**
	 * @brief gets an accessor of thread local data
	 *
	 * @return an accessor of thread local data
	 *
	 * @exception if fail to allocate thread local data, throw std::bad_alloc
	 */
	scoped_accessor get_tls_accessor( void )
	{
		return internal::dynamic_tls_getspecific_accessor( tls_key_chk_and_get() );
	}

	/**
	 * @brief get thread count information
	 *
	 * @return 1st: current thread count
	 * @return 2nd: max thread count
	 */
	std::pair<int, int> get_thread_count_info( void ) const
	{
		return std::pair<int, int>(
			th_cnt_.cur_thread_count_.load( std::memory_order_acquire ),
			th_cnt_.max_thread_count_.load( std::memory_order_acquire ) );
	}

private:
	inline internal::dynamic_tls_key_t tls_key_chk_and_get( void )
	{
		internal::dynamic_tls_key_t ans = tls_key_.load( std::memory_order_acquire );
		if ( ans != nullptr ) return ans;

		auto alloc_ans = internal::dynamic_tls_key_create( this, alloc_fn, dealloc_fn );
		if ( tls_key_.compare_exchange_strong( ans, alloc_ans, std::memory_order_acq_rel ) ) return alloc_ans;
		dynamic_tls_key_release( alloc_ans );

		ans = tls_key_.load( std::memory_order_acquire );
		if ( ans != nullptr ) return ans;

		// ans == nullptr
		internal::LogOutput( log_type::ERR, "dynamic tls key creation and deletion has happened in same time. this will be happened by user side bug." );
		throw std::runtime_error( "dynamic tls key creation and deletion has happened in same time. this will be happened by user side bug like dynamic_tls::XXX calling and destructing this instance." );
	}

	static uintptr_t alloc_fn( void* p_param )
	{
		internal::LogOutput( log_type::DEBUG, "dynamic_tls<T*>::alloc_fn is called              - p_param=%p", p_param );
		dynamic_tls* p_this_param = reinterpret_cast<dynamic_tls*>( p_param );

		if ( p_this_param == nullptr ) {
			internal::LogOutput( log_type::ERR, "dynamic_tls<T*>::alloc_fn is called but p_param is nullptr" );
			return 0U;   // なぜかnullptrで呼び出された。多分pthread内でのrace conditionのせい。どうしようもないので、諦める。
		}

		p_this_param->th_cnt_.count_up();
		return p_this_param->tl_handler_.allocate();
	}

	static void dealloc_fn( uintptr_t p_obj, void* p_param )
	{
		internal::LogOutput( log_type::DEBUG, "dynamic_tls<T*>::dealloc_fn is called              - p_obj=0x%lx, p_param=%p", p_obj, p_param );
		dynamic_tls* p_this_param = reinterpret_cast<dynamic_tls*>( p_param );

		if ( p_obj == 0U ) {
			internal::LogOutput( log_type::DEBUG, "dynamic_tls<T*>::dealloc_fn is called but p_obj is nullptr" );
			return;   // なぜかnullptrで呼び出された。多分pthread内でのrace conditionのせい。どうしようもないので、諦める。
		}
		if ( p_param == nullptr ) {
			internal::LogOutput( log_type::ERR, "dynamic_tls<T*>::dealloc_fn is called but p_param is nullptr" );
			return;   // なぜかnullptrで呼び出された。多分pthread内でのrace conditionのせい。どうしようもないので、諦める。
		}

		p_this_param->tl_handler_.deallocate( p_obj );
		p_this_param->th_cnt_.count_down();
		return;
	}

	std::atomic<internal::dynamic_tls_key_t> tls_key_;   //!<	key for thread local storage of POSIX.

	TL_HANDLER tl_handler_;   //!< functor to clean-up the resources when thread is terminated.

	internal::dynamic_tls_thread_cnt th_cnt_;   //!< thread count information
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_DYNAMIC_TLS_HPP_ */
