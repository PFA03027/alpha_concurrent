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
 * Copyright (C) 2021 by alpha <e-mail address>
 */

#ifndef SRC_DYNAMIC_TLS_HPP_
#define SRC_DYNAMIC_TLS_HPP_

#include <pthread.h>

#include <atomic>
#include <memory>

#include "conf_logger.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

template <typename T>
class tls_data_container {
public:
	using value_type = T;

	enum class ocupied_status {
		UNUSED,
		USING
	};

	T* p_value;

	tls_data_container( void )
	  : p_value( nullptr )
	  , status_( ocupied_status::USING )
	  , next_( nullptr )
	{
		LogOutput( log_type::DEBUG, "tls_data_container::constructor is allocated - %p", this );
	}

	~tls_data_container()
	{
		LogOutput( log_type::DEBUG, "tls_data_container::destructor is called     - %p", this );
		delete p_value;
	}

	void release_owner( void )
	{
		delete p_value;
		p_value = nullptr;
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
};

}   // namespace internal

template <typename T>
class dynamic_tls {
public:
	using value_type      = T;
	using value_reference = T&;

	dynamic_tls( void )
	  : head_( nullptr )
	{
		int status = pthread_key_create( &tls_key, destr_fn );
		if ( status < 0 ) {
			LogOutput( log_type::ERR, "pthread_key_create failed, errno=%d", errno );
			exit( 1 );
		}
	}

	~dynamic_tls()
	{
		LogOutput( log_type::DEBUG, "dynamic_tls::destructor is called" );

		int status = pthread_key_delete( tls_key );
		if ( status < 0 ) {
			LogOutput( log_type::ERR, "pthread_key_delete failed, errno=%d", errno );
			exit( 1 );
		}

		// メモリリークが無いように、スレッドローカルストレージを削除する
		// この処理と、スレッド終了処理によるdestr_fn()の同時呼び出しは、不定動作とする。
		internal::tls_data_container<T>* p_cur = head_.load( std::memory_order_acquire );
		while ( p_cur != nullptr ) {
			internal::tls_data_container<T>* p_nxt = p_cur->get_next();
			delete p_cur;
			p_cur = p_nxt;
		}
	}

	template <typename TFUNC>
	value_reference get_tls_instance_pred( TFUNC pred )
	{
		// pthread_getspecific()が、ロックフリーであることを祈る。
		internal::tls_data_container<T>* p_tls = reinterpret_cast<internal::tls_data_container<T>*>( pthread_getspecific( tls_key ) );
		if ( p_tls == nullptr ) {
			p_tls = allocate_free_tls_container();
			if ( p_tls == nullptr ) {
				p_tls = new internal::tls_data_container<T>();
			}
			p_tls->p_value = pred();

			int status;
			status = pthread_setspecific( tls_key, (void*)p_tls );
			if ( status < 0 ) {
				LogOutput( log_type::ERR, "pthread_setspecific failed, errno %d", errno );
				pthread_exit( (void*)1 );
			}
		}

		return *( p_tls->p_value );
	}

	template <typename... Args>
	value_reference get_tls_instance( Args... args )
	{
		return get_tls_instance_pred( [&args...]() { return new T( args... ); } );
	}

private:
	static void destr_fn( void* parm )
	{
		LogOutput( log_type::DEBUG, "dynamic_tls::destr_fn is called              - %p", parm );

		if ( parm == nullptr ) return;   // なぜかnullptrで呼び出された。多分pthread内でのrace conditionのせい。

		internal::tls_data_container<T>* p_target_node = reinterpret_cast<internal::tls_data_container<T>*>( parm );
		p_target_node->release_owner();

		return;
	}

	internal::tls_data_container<T>* allocate_free_tls_container( void )
	{
		// 空きノードを探す。
		internal::tls_data_container<T>* p_ans = head_.load( std::memory_order_acquire );
		while ( p_ans != nullptr ) {
			if ( p_ans->get_status() == internal::tls_data_container<T>::ocupied_status::UNUSED ) {
				if ( p_ans->try_to_get_owner() ) {
					LogOutput( log_type::DEBUG, "node is allocated." );
					return p_ans;
				}
			}
			p_ans = p_ans->get_next();
		}

		LogOutput( log_type::DEBUG, "glist is added." );
		return p_ans;
	}

	pthread_key_t tls_key;   //!<	key for thread local storage of POSIX.

	std::atomic<internal::tls_data_container<T>*> head_;
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_DYNAMIC_TLS_HPP_ */
