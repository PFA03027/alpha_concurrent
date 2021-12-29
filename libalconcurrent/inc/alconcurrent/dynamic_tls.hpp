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
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <utility>

#include "conf_logger.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

/*!
 * @breif	call pthread_key_create() and count up the number of dynamic thread local memory
 *
 *　pthread_key_create()での割り当て数を１つ増やす。pthread_key_create()を呼び出すときに併せて呼び出す。
 *　不具合解析など使用する。
 */
void dynamic_tls_pthread_key_create( pthread_key_t* p_key, void ( *destructor )( void* ) );

/*!
 * @breif	call pthread_key_delete() and count down the number of dynamic thread local memory
 *
 *　pthread_key_create()での割り当て数を１つ減らす。pthread_key_delete()を呼び出すときに併せて呼び出す。
 *　不具合解析など使用する。
 */
void dynamic_tls_pthread_key_delete( pthread_key_t key );

/*!
 * @breif	get the number of dynamic thread local memory
 *
 *　pthread_key_create()での割り当て数を取得する。
 *　不具合解析など使用する。
 */
int get_num_of_tls_key( void );

/*!
 * @breif	get the max number of dynamic thread local memory
 *
 *　pthread_key_create()での割り当て数を取得する。
 *　不具合解析など使用する。
 */
int get_max_num_of_tls_key( void );

/*!
 * @breif	動的スレッドローカルストレージで使用する内部処理用クラス
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

	enum class ocupied_status {
		UNUSED,
		USING
	};

	value_pointer_type p_value;

	tls_data_container( TL_PRE_DESTRUCTOR& df )
	  : p_value( nullptr )
	  , status_( ocupied_status::USING )
	  , next_( nullptr )
	  , pre_exec_( df )
	{
		internal::LogOutput( log_type::DEBUG, "tls_data_container::constructor is called - %p", this );
	}

	~tls_data_container()
	{
		internal::LogOutput( log_type::DEBUG, "tls_data_container::destructor is called  - %p, p_value - %p", this, p_value );

		if ( p_value != nullptr ) {
			p_value->~T();
			std::free( p_value );

			p_value = nullptr;
		}
	}

	/*!
	 * @breif	スレッド終了時に管理しているスレッドローカルストレージを解放する。
	 */
	void release_owner( void )
	{
		if ( p_value != nullptr ) {
			pre_exec_( *p_value );

			p_value->~T();
			std::free( p_value );

			p_value = nullptr;
		}
		status_.store( ocupied_status::UNUSED, std::memory_order_release );
		return;
	}

	bool try_to_get_owner( void )
	{
		ocupied_status cur;
		cur = get_status();
		if ( cur != ocupied_status::UNUSED ) {
			return false;
		}

		return status_.compare_exchange_strong( cur, ocupied_status::USING );
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

private:
	std::atomic<ocupied_status>      status_;
	std::atomic<tls_data_container*> next_;
	TL_PRE_DESTRUCTOR&               pre_exec_;
};

}   // namespace internal

/*!
 * @breif	スレッド終了時にスレッドローカルストレージを解放する前に呼び出されるファンクタ
 *
 * @param [in]	T	ファンクタに引数として渡される型。実際の引数は、この型の参照T&となる。
 *
 * このファンクタ自身は、何もしないというファンクタとして動作する。
 *
 *
 * @breif A functor called before freeing thread-local storage at the end of a thread
 *
 * @param [in] T The type passed as an argument to the functor. The actual argument is a reference T & of this type.
 *
 * This funktor itself acts as a funktor that does nothing.
 */
template <typename T>
struct threadlocal_destructor_functor {
	using value_reference = T&;
	void operator()(
		value_reference data   //!< [in/out] ファンクタの処理対象となるインスタンスへの参照
	)
	{
		return;
	}
};

/*!
 * @breif	動的スレッドローカルストレージを実現するクラス
 *
 * スレッド終了時にスレッドローカルストレージをdestructする前に、コンストラクタで指定されたTL_PRE_DESTRUCTORのインスタンスが呼び出される。
 * このクラスのインスタンス変数自体がdestructされる場合は、呼び出されない。
 *
 * @param [in]	T	スレッドローカルストレージとして確保する型
 * @param [in]	TL_PRE_DESTRUCTOR	スレッド終了時にスレッドローカルストレージを解放(delete)する前に呼び出されるファンクタ
 *
 * @warn
 * デストラクタ処理と、スレッド終了処理によるdestr_fn()の同時呼び出しは、不定動作とする。　@n
 * よって、インスタンスの破棄とスレッドの終了処理を同時に発生しないようにすること。
 *
 * @note
 * lf_mem_allocクラスで使用するため、
 * コンポーネントの上下関係から、メモリの確保にはnewを使用せず、malloc/freeと配置newのみを使用する。
 *
 *
 * @breif Class that realizes dynamic thread local storage
 *
 * At the end of the thread, the instance of TL_PRE_DESTRUCTOR specified in the constructor is called before destructing the thread local storage.
 * If the instance variable of this class itself is destroyed, it will not be called.
 *
 * @param [in] T Type reserved as thread local storage
 * @param [in] TL_PRE_DESTRUCTOR A functor called before freeing(delete) thread-local storage at the end of a thread
 *
 * @warn
 * Simultaneous call of destr_fn () by destructor processing and thread termination processing is an indefinite operation. @n
 * Therefore, do not destroy the instance and terminate the thread at the same time.
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
	  : pre_exec_()
	  , head_( nullptr )
	{
		internal::dynamic_tls_pthread_key_create( &tls_key, destr_fn );
	}

	dynamic_tls( const TL_PRE_DESTRUCTOR& tl_dest_functor_arg )
	  : pre_exec_( tl_dest_functor_arg )
	  , head_( nullptr )
	{
		internal::dynamic_tls_pthread_key_create( &tls_key, destr_fn );
	}

	dynamic_tls( TL_PRE_DESTRUCTOR&& tl_dest_functor_arg )
	  : pre_exec_( std::move( tl_dest_functor_arg ) )
	  , head_( nullptr )
	{
		internal::dynamic_tls_pthread_key_create( &tls_key, destr_fn );
	}

	~dynamic_tls()
	{
		internal::LogOutput( log_type::DEBUG, "dynamic_tls::destructor is called" );

		internal::dynamic_tls_pthread_key_delete( tls_key );

		// メモリリークが無いように、スレッドローカルストレージを削除する
		// この処理と、スレッド終了処理によるdestr_fn()の同時呼び出しは、不定動作とする。
		tls_cont_pointer p_cur = head_.load( std::memory_order_acquire );
		while ( p_cur != nullptr ) {
			tls_cont_pointer p_nxt = p_cur->get_next();
			p_cur->~tls_cont();
			std::free( p_cur );
			p_cur = p_nxt;
		}
	}

	template <typename... Args>
	value_reference get_tls_instance( Args... args )
	{
		return get_tls_instance_pred( [&args...]() {
			void* p_tmp = std::malloc( sizeof( T ) );
			return new ( p_tmp ) T( args... );
		} );
	}

private:
	using tls_cont         = internal::tls_data_container<T, TL_PRE_DESTRUCTOR>;
	using tls_cont_pointer = tls_cont*;

	template <typename TFUNC>
	value_reference get_tls_instance_pred( TFUNC pred )
	{
		// pthread_getspecific()が、ロックフリーであることを祈る。
		tls_cont_pointer p_tls = reinterpret_cast<tls_cont_pointer>( pthread_getspecific( tls_key ) );
		if ( p_tls == nullptr ) {
			p_tls          = allocate_free_tls_container();
			p_tls->p_value = pred();

			int status;
			status = pthread_setspecific( tls_key, (void*)p_tls );
			if ( status < 0 ) {
				internal::LogOutput( log_type::ERR, "pthread_setspecific failed, errno %d", errno );
				pthread_exit( (void*)1 );
			}
		}

		return *( p_tls->p_value );
	}

	static void destr_fn( void* parm )
	{
		internal::LogOutput( log_type::DEBUG, "dynamic_tls::destr_fn is called              - %p", parm );

		if ( parm == nullptr ) return;   // なぜかnullptrで呼び出された。多分pthread内でのrace conditionのせい。どうしようもないので、諦める。

		tls_cont_pointer p_target_node = reinterpret_cast<tls_cont_pointer>( parm );

		p_target_node->release_owner();

		return;
	}

	tls_cont_pointer allocate_free_tls_container( void )
	{
		// 空きノードを探す。
		tls_cont_pointer p_ans = head_.load( std::memory_order_acquire );
		while ( p_ans != nullptr ) {
			if ( p_ans->get_status() == tls_cont::ocupied_status::UNUSED ) {
				if ( p_ans->try_to_get_owner() ) {
					internal::LogOutput( log_type::DEBUG, "node is allocated." );
					return p_ans;
				}
			}
			p_ans = p_ans->get_next();
		}

		void* p_tmp = std::malloc( sizeof( tls_cont ) );
		p_ans       = new ( p_tmp ) tls_cont( pre_exec_ );

		// リストの先頭に追加
		tls_cont_pointer p_cur_head = head_.load( std::memory_order_acquire );
		do {
			p_ans->set_next( p_cur_head );
		} while ( !head_.compare_exchange_strong( p_cur_head, p_ans ) );

		internal::LogOutput( log_type::DEBUG, "glist is added." );
		return p_ans;
	}

	pthread_key_t tls_key;   //!<	key for thread local storage of POSIX.

	TL_PRE_DESTRUCTOR             pre_exec_;   //!< functor to clean-up the resources when thread is terminated.
	std::atomic<tls_cont_pointer> head_;       //!< head of thread local data container list
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_DYNAMIC_TLS_HPP_ */
