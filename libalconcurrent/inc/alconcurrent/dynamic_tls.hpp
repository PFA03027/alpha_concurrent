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

#ifndef SRC_DYNAMIC_TLS_HPP_
#define SRC_DYNAMIC_TLS_HPP_

#include <pthread.h>

#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <utility>

#include "conf_logger.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

#define ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE          ( 1024 )
#define ALCONCURRENT_CONF_DYNAMIC_TLS_DESTUCT_ITERATE_MAX ( 10 )

struct dynamic_tls_key {
	enum class alloc_stat {
		NOT_USED = 0,
		RELEASING,
		USED
	};

	unsigned int            idx_;         //!< index of key
	std::atomic<alloc_stat> is_used_;     //!< flag whether this key is used or not.
	void ( *tls_destructor_ )( void* );   //!< thread local storage destructor

	dynamic_tls_key( void )
	  : idx_( 0 )
	  , is_used_( alloc_stat::NOT_USED )
	  , tls_destructor_( nullptr )
	{
	}
};

using dynamic_tls_key_t = dynamic_tls_key*;   //!< pointer to dynamic_tls_key as a key

struct dynamic_tls_status_info {
	unsigned int num_content_head_;
	unsigned int next_base_idx_;
};

/*!
 * @brief	create a key of dynamic thread local storage and count up the number of dynamic thread local memory
 *
 *　動的thread local storageを割り当て、割り当て数を１つ増やす。
 *　割り当て数は不具合解析など使用する。
 */
void dynamic_tls_key_create( dynamic_tls_key_t* p_key, void ( *destructor )( void* ) );

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
bool dynamic_tls_setspecific( dynamic_tls_key_t key, void* pointer );

/*!
 * @brief	get a value from a thread local storage
 *
 */
void* dynamic_tls_getspecific( dynamic_tls_key_t key );

dynamic_tls_status_info dynamic_tls_get_status( void );

/*!
 * @brief	call pthread_key_create() and count up the number of dynamic thread local memory
 *
 *　pthread_key_create()での割り当て数を１つ増やす。pthread_key_create()を呼び出すときに併せて呼び出す。
 *　不具合解析など使用する。
 */
void dynamic_tls_pthread_key_create( pthread_key_t* p_key, void ( *destructor )( void* ) );

/*!
 * @brief	call pthread_key_delete() and count down the number of dynamic thread local memory
 *
 *　pthread_key_create()での割り当て数を１つ減らす。pthread_key_delete()を呼び出すときに併せて呼び出す。
 *　不具合解析など使用する。
 */
void dynamic_tls_pthread_key_delete( pthread_key_t key );

/**
 * @brief get the value of thread local storage
 *
 * @return the value of thread local storage
 * @retval nullptr normally, this value means it is not assigned a value.
 */
void* dynamic_tls_pthread_getspecific( pthread_key_t key );

/**
 * @brief set a value to thread local storage
 */
void dynamic_tls_pthread_setspecific( pthread_key_t key, void* p_tls_arg );

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
 *　pthread_key_create()での割り当て数を取得する。
 *　不具合解析など使用する。
 */
int get_max_num_of_tls_key( void );

/**
 * @brief thread count information container for dynamic thread local variable class
 *
 */
struct dynamic_tls_thread_cnt {
	dynamic_tls_thread_cnt( void )
	  : cur_thread_count_( 0 )
	  , max_thread_count_( 0 )
	{
	}
	dynamic_tls_thread_cnt( const dynamic_tls_thread_cnt& )            = default;
	dynamic_tls_thread_cnt( dynamic_tls_thread_cnt&& )                 = default;
	dynamic_tls_thread_cnt& operator=( const dynamic_tls_thread_cnt& ) = default;
	dynamic_tls_thread_cnt& operator=( dynamic_tls_thread_cnt&& )      = default;

	void count_up( void )
	{
		auto cur     = cur_thread_count_.fetch_add( 1 ) + 1;
		auto cur_max = max_thread_count_.load( std::memory_order_acquire );
		if ( cur > cur_max ) {
			max_thread_count_.compare_exchange_strong( cur_max, cur );
		}
	}

	void count_down( void )
	{
		cur_thread_count_.fetch_sub( 1 );
	}

	std::atomic_int cur_thread_count_;   //!< current thread count
	std::atomic_int max_thread_count_;   //!< max thread count
};

/*!
 * @brief	動的スレッドローカルストレージで使用する内部処理用クラス
 *
 * @note
 * lf_mem_allocクラスで使用するため、
 * コンポーネントの上下関係から、メモリの確保にはnewを使用せず、malloc/freeと配置newのみを使用する。
 */
template <typename T, typename TL_PRE_DESTRUCTOR>
class tls_data_container {
public:
	using value_type         = T;
	using value_pointer_type = value_type*;

	enum class ocupied_status : int {
		UNUSED,
		UNUSED_INITIALIZED,
		USING
	};

	tls_data_container( dynamic_tls_thread_cnt* p_th_cnt_arg, TL_PRE_DESTRUCTOR& df )
	  : p_value( nullptr )
	  , p_th_cnt_( p_th_cnt_arg )
	  , status_( ocupied_status::USING )
	  , next_( nullptr )
	  , pre_exec_( df )
	{
		internal::LogOutput( log_type::DEBUG, "tls_data_container::constructor is called - %p", this );
		assert( p_th_cnt_ != nullptr );
	}

	~tls_data_container()
	{
		internal::LogOutput( log_type::DEBUG, "tls_data_container::destructor is called  - %p, p_value - %p", this, p_value );

		if ( p_value != nullptr ) {
			pre_exec_.destruct( *p_value );
			p_value->~T();
			std::free( p_value );

			next_.store( nullptr, std::memory_order_release );
			status_.store( ocupied_status::UNUSED, std::memory_order_release );
			p_value = nullptr;
		}
	}

	/*!
	 * @brief	スレッド終了時に管理しているスレッドローカルストレージを解放する。
	 */
	void release_owner( void )
	{
		if ( p_value != nullptr ) {
			bool ret = pre_exec_.release( *p_value );

			if ( ret ) {
				p_value->~T();
				std::free( p_value );

				p_value = nullptr;
			}
		}
		if ( p_value != nullptr ) {
			status_.store( ocupied_status::UNUSED_INITIALIZED, std::memory_order_release );
		} else {
			status_.store( ocupied_status::UNUSED, std::memory_order_release );
		}
		return;
	}

	bool try_to_get_owner( void )
	{
		ocupied_status cur;
		cur = get_status();
		if ( cur == ocupied_status::USING ) {
			return false;
		}

		bool ans = status_.compare_exchange_strong( cur, ocupied_status::USING );

		return ans;
	}

	ocupied_status get_status( void )
	{
		return status_.load( std::memory_order_acquire );
	}

	tls_data_container* get_next( void )
	{
		return next_.load( std::memory_order_acquire );
	}

	void set_next( tls_data_container* p_new_next )
	{
		next_.store( p_new_next, std::memory_order_release );
		return;
	}

	bool next_CAS( tls_data_container** pp_expect_ptr, tls_data_container* p_desired_ptr )
	{
		return next_.compare_exchange_weak( *pp_expect_ptr, p_desired_ptr );
	}

	value_pointer_type p_value;

	dynamic_tls_thread_cnt* const p_th_cnt_;

private:
	std::atomic<ocupied_status>      status_;
	std::atomic<tls_data_container*> next_;
	TL_PRE_DESTRUCTOR&               pre_exec_;
};

}   // namespace internal

/*!
 * @brief	スレッド終了時にスレッドローカルストレージを解放する前に呼び出されるファンクタ
 *
 * @param [in]	T	ファンクタに引数として渡される型。実際の引数は、この型の参照T&となる。
 *
 * このファンクタ自身は、何もしないというファンクタとして動作する。
 *
 *
 * @brief A functor called before freeing thread-local storage at the end of a thread
 *
 * @param [in] T The type passed as an argument to the functor. The actual argument is a reference T & of this type.
 *
 * This funktor itself acts as a funktor that does nothing.
 */
template <typename T>
struct threadlocal_destructor_functor {
	using value_reference = T&;

	/*!
	 * @brief スレッド終了時に呼び出されるI/F
	 *
	 * @retval true 呼び出し元に対し、引数のリファレンスの実体が存在するメモリ領域を解放するように指示する。
	 * @retval false 呼び出し元に対し、引数のリファレンスの実体が存在するメモリ領域を保持するように指示する。
	 */
	bool release(
		value_reference data   //!< [in/out] ファンクタの処理対象となるインスタンスへの参照
	)
	{
		return true;
	}

	/*!
	 * @brief インスタンス削除時に呼び出されるI/F
	 */
	void destruct(
		value_reference data   //!< [in/out] ファンクタの処理対象となるインスタンスへの参照
	)
	{
		return;
	}
};

/*!
 * @brief	動的スレッドローカルストレージを実現するクラス
 *
 * スレッド終了時にスレッドローカルストレージをdestructする前に、コンストラクタで指定されたTL_PRE_DESTRUCTORのインスタンスが呼び出される。
 * このクラスのインスタンス変数自体がdestructされる場合は、呼び出されない。
 *
 * @tparam [in]	T	スレッドローカルストレージとして確保する型
 * @tparam [in]	TL_PRE_DESTRUCTOR	スレッド終了時、およびインスタンス自身がdestructされる時に、呼び出されるファンクタ。例えばTが何らかのクラスへのポインタ型である場合、その解放処理制御に使用する。
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
 * コンポーネントの上下関係から、メモリの確保にはnewを使用せず、malloc/freeと配置newのみを使用する。
 *
 * @brief Class that realizes dynamic thread local storage
 *
 * At the end of the thread, the instance of TL_PRE_DESTRUCTOR specified in the constructor is called before destructing the thread local storage.
 * If the instance variable of this class itself is destroyed, it will not be called.
 *
 * @tparam [in] T Type reserved as thread local storage
 * @tparam [in] TL_PRE_DESTRUCTOR A functor that is called at the end of a thread and when the instance itself is destroyed
 *
 * @warn
 * Simultaneous call of destr_fn () by destructor processing and thread termination processing is an indefinite operation. @n
 * Therefore, do not destroy the instance and terminate the thread at the same time.
 *
 * @warn
 * Not support T that Actual data size is variable
 *
 * @note
 * Because it is used in the lf_mem_alloc class
 * Due to the hierarchical relationship of components, new is not used to allocate memory, only malloc/free and placement new are used.
 */
template <typename T, typename TL_PRE_DESTRUCTOR = threadlocal_destructor_functor<T>>
class dynamic_tls {
public:
	using value_type      = T;
	using value_reference = T&;

	dynamic_tls( void )
	  : pre_exec_of_cleanup_()
	  , head_( nullptr )
	{
		internal::dynamic_tls_pthread_key_create( &tls_key, destr_fn );
	}

	dynamic_tls( const TL_PRE_DESTRUCTOR& tl_dest_functor_arg )
	  : pre_exec_of_cleanup_( tl_dest_functor_arg )
	  , head_( nullptr )
	{
		internal::dynamic_tls_pthread_key_create( &tls_key, destr_fn );
	}

	dynamic_tls( TL_PRE_DESTRUCTOR&& tl_dest_functor_arg )
	  : pre_exec_of_cleanup_( std::move( tl_dest_functor_arg ) )
	  , head_( nullptr )
	{
		internal::dynamic_tls_pthread_key_create( &tls_key, destr_fn );
	}

	~dynamic_tls()
	{
		internal::LogOutput( log_type::DEBUG, "dynamic_tls::destructor is called" );

		internal::dynamic_tls_pthread_key_delete( tls_key );

		// メモリリークが無いように、スレッドローカルストレージを削除する
		// この処理と、スレッド終了処理によるdestr_fn()の同時呼び出しは、未定義動作とする。
		tls_cont_pointer p_cur = head_.load( std::memory_order_acquire );
		bool             ret   = head_.compare_exchange_strong( p_cur, nullptr );
		if ( !ret ) {
			internal::LogOutput( log_type::WARN, "dynamic_tls::destructor is called in race condition" );
			return;
		}

		while ( p_cur != nullptr ) {
			tls_cont_pointer p_nxt = p_cur->get_next();

			// ノード自身のデストラクタ処理を行う。
			p_cur->~tls_cont();

			// ノードのメモリ領域を解放する。
			std::free( p_cur );

			p_cur = p_nxt;
		}
	}

	/**
	 * @brief gets a reference of thread local data with the constructor paramters for first call of a thread
	 *
	 * If a call of this I/F by a thread is first, this I/F will allocate a thread local memory for T and
	 * construct it by it's constructor with Args and args.
	 *
	 * @tparam Args	constructor paramter types of T
	 *
	 * @return a reference of thread local data
	 */
	template <typename... Args>
	value_reference get_tls_instance(
		Args... args   //!< [in] constructor paramters of T
	)
	{
		return get_tls_instance_pred_impl( [&args...]() -> T* {
			void* p_tmp = std::malloc( sizeof( T ) );
			return new ( p_tmp ) T( args... );
		} );
	}

	/**
	 * @brief gets a reference of thread local data with a procedure functor and the constructor paramters for first call of a thread
	 *
	 * @tparam TFUNC	functor to get an inialized value of T
	 * TFUNC should have a operator(): void operator()(T& v) @n
	 * v is a reference that is constructed with Args and args.
	 *
	 * @return a reference of thread local data
	 *
	 * If a call of this I/F by a thread is first, this I/F will do below steps to construct initial value.
	 * @li Step#1 Allocates a thread local memory for T by malloc.
	 * @li Step#2 call T's constructor with Args and args.
	 * @li Step#3 call pred with a reference of a instace of T.
	 *
	 * @note
	 * If TL_PRE_DESTRUCTOR does not release thread local data area of T, there is a possibility that thread local data area has some value by re-using.
	 * In this case, pred is not called.
	 */
	template <typename TFUNC, typename... Args>
	value_reference get_tls_instance_pred(
		TFUNC pred,    //!< [in] functor to get an inialized value of T
		Args... args   //!< [in] constructor paramters of T
	)
	{
		return get_tls_instance_pred_impl( [&pred, &args...]() -> T* {
			void* p_tmp = std::malloc( sizeof( T ) );
			T*    p_ans = new ( p_tmp ) T( args... );
			pred( *p_ans );
			return p_ans;
		} );
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
	using tls_cont         = internal::tls_data_container<T, TL_PRE_DESTRUCTOR>;
	using tls_cont_pointer = tls_cont*;

	tls_cont_pointer allocate_free_tls_container( void )
	{
		// 空きノードを探す。
		tls_cont_pointer p_ans = head_.load( std::memory_order_acquire );
		while ( p_ans != nullptr ) {
			if ( p_ans->get_status() != tls_cont::ocupied_status::USING ) {
				if ( p_ans->try_to_get_owner() ) {
					internal::LogOutput( log_type::DEBUG, "node is allocated." );
					return p_ans;
				}
			}
			p_ans = p_ans->get_next();
		}

		void* p_tmp = std::malloc( sizeof( tls_cont ) );
		p_ans       = new ( p_tmp ) tls_cont( &th_cnt_, pre_exec_of_cleanup_ );

		// リストの先頭に追加
		tls_cont_pointer p_cur_head = head_.load( std::memory_order_acquire );
		do {
			p_ans->set_next( p_cur_head );
		} while ( !head_.compare_exchange_strong( p_cur_head, p_ans ) );

		internal::LogOutput( log_type::DEBUG, "glist is added." );
		return p_ans;
	}

	template <typename TFUNC>
	value_reference get_tls_instance_pred_impl( TFUNC pred )
	{
		// pthread_getspecific()が、ロックフリーであることを祈る。
		tls_cont_pointer p_tls = reinterpret_cast<tls_cont_pointer>( internal::dynamic_tls_pthread_getspecific( tls_key ) );
		if ( p_tls == nullptr ) {
			p_tls = allocate_free_tls_container();
			if ( p_tls->p_value == nullptr ) {   // 確保済みの領域の場合は、pred()を呼び出さず、そのまま再利用する。
				p_tls->p_value = pred();
			}

			// 新しいスレッド用のスレッドローカルストレージの割り当てが行われたため、スレッド数のカウントをアップする。
			th_cnt_.count_up();

			internal::dynamic_tls_pthread_setspecific( tls_key, (void*)p_tls );
		}

		return *( p_tls->p_value );
	}

	static void destr_fn( void* param )
	{
		internal::LogOutput( log_type::DEBUG, "dynamic_tls::destr_fn is called              - %p", param );

		if ( param == nullptr ) return;   // なぜかnullptrで呼び出された。多分pthread内でのrace conditionのせい。どうしようもないので、諦める。

		tls_cont_pointer p_target_node = reinterpret_cast<tls_cont_pointer>( param );

		p_target_node->release_owner();

		p_target_node->p_th_cnt_->count_down();

		return;
	}

	pthread_key_t tls_key;   //!<	key for thread local storage of POSIX.

	TL_PRE_DESTRUCTOR             pre_exec_of_cleanup_;   //!< functor to clean-up the resources when thread is terminated.
	std::atomic<tls_cont_pointer> head_;                  //!< head of thread local data container list

	internal::dynamic_tls_thread_cnt th_cnt_;   //!< thread count information
};

// this class is possible to set a value to inialize, but requires that T needs to have a default constructor and copy constructor.
template <typename T, typename TL_PRE_DESTRUCTOR = threadlocal_destructor_functor<T>>
class dynamic_tls_wi : public dynamic_tls<T, TL_PRE_DESTRUCTOR> {
public:
	using value_type      = typename dynamic_tls<T, TL_PRE_DESTRUCTOR>::value_type;
	using value_reference = typename dynamic_tls<T, TL_PRE_DESTRUCTOR>::value_reference;

	dynamic_tls_wi( void )
	  : dynamic_tls_wi<T, TL_PRE_DESTRUCTOR>() {}

	dynamic_tls_wi( const T& init_v )
	  : dynamic_tls<T, TL_PRE_DESTRUCTOR>()
	  , init_value_( init_v ) {}

	dynamic_tls_wi( const TL_PRE_DESTRUCTOR& tl_dest_functor_arg )
	  : dynamic_tls<T, TL_PRE_DESTRUCTOR>( tl_dest_functor_arg ) {}

	dynamic_tls_wi( TL_PRE_DESTRUCTOR&& tl_dest_functor_arg )
	  : dynamic_tls<T, TL_PRE_DESTRUCTOR>( std::move( tl_dest_functor_arg ) ) {}

	dynamic_tls_wi( const T& init_v, const TL_PRE_DESTRUCTOR& tl_dest_functor_arg )
	  : dynamic_tls<T, TL_PRE_DESTRUCTOR>( tl_dest_functor_arg )
	  , init_value_( init_v ) {}

	dynamic_tls_wi( const T& init_v, TL_PRE_DESTRUCTOR&& tl_dest_functor_arg )
	  : dynamic_tls<T, TL_PRE_DESTRUCTOR>( std::move( tl_dest_functor_arg ) )
	  , init_value_( init_v ) {}

	/**
	 * @brief gets a reference of thread local data copy-constructed by a copy of init_v for first call of a thread
	 *
	 * If a call of this I/F by a thread is first, this I/F will allocate a thread local memory for T and
	 * construct it by copy constructor with a copy of init_v
	 *
	 * @return a reference of thread local data
	 */
	value_reference get_tls_instance_wi( void )
	{
		return get_tls_instance( [this]() -> T* {
			void* p_tmp = std::malloc( sizeof( T ) );
			return new ( p_tmp ) T( this->init_value_ );
		} );
	}

private:
	const T init_value_;   //!< a value that it constructs thread local mememory storage with
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_DYNAMIC_TLS_HPP_ */
