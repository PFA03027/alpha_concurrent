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

#include <memory>

namespace alpha {
namespace concurrent {

namespace internal {

class tls_key_container {
public:
	tls_key_container( pthread_key_t tls_key_arg )
	  : tls_key( tls_key_arg )
	{
		//		printf( "pthread_key_create key= %d\n", tls_key );
	}

	~tls_key_container()
	{
		int status = pthread_key_delete( tls_key );
		if ( status < 0 ) {
			printf( "pthread_key_delete failed, errno=%d\n", errno );
			exit( 1 );
		}
		//		printf( "pthread_key_delete key = %d\n", tls_key );
	}

private:
	pthread_key_t tls_key;   //!<	key for thread local storage of POSIX.
};

template <typename T>
class tls_data_container {
public:
	using value_type = T;

	T value;

	template <typename... Args>
	tls_data_container( const std::shared_ptr<tls_key_container>& tls_key_cont_arg, Args... args )
	  : value( args... )
	  , sp_tls_key_cont( tls_key_cont_arg )
	{
		//		printf( "tls_data_container::constructor is allocated - %p\n", this );
	}

	~tls_data_container()
	{
		//		printf( "tls_data_container::destructor is called     - %p\n", this );
	}

private:
	std::shared_ptr<tls_key_container> sp_tls_key_cont;   //!<	key for thread local storage of POSIX.
};

}   // namespace internal

template <typename T>
class dynamic_tls {
public:
	using value_type      = T;
	using value_reference = T&;

	dynamic_tls( void )
	{
		int status = pthread_key_create( &tls_key, destr_fn );
		if ( status < 0 ) {
			printf( "pthread_key_create failed, errno=%d", errno );
			exit( 1 );
		}

		sp_key_cont = std::make_shared<internal::tls_key_container>( tls_key );
	}

	~dynamic_tls()
	{
		//		printf( "dynamic_tls::destructor is called\n" );
		delete_tls_instance();
	}

	template <typename... Args>
	value_reference get_tls_instance( Args... args )
	{
		// pthread_getspecific()が、ロックフリーであることを祈る。
		internal::tls_data_container<T>* p_tls = reinterpret_cast<internal::tls_data_container<T>*>( pthread_getspecific( tls_key ) );
		if ( p_tls == nullptr ) {
			p_tls = new internal::tls_data_container<T>( sp_key_cont, args... );

			int status;
			status = pthread_setspecific( tls_key, (void*)p_tls );
			if ( status < 0 ) {
				printf( "pthread_setspecific failed, errno %d", errno );
				pthread_exit( (void*)1 );
			}
		}

		return p_tls->value;
	}

private:
	static void destr_fn( void* parm )
	{
		//		printf( "dynamic_tls::destr_fn is called              - %p\n", parm );

		if ( parm == nullptr ) return;   // なぜかnullptrで呼び出された。多分pthread内でのrace conditionのせい。

		internal::tls_data_container<T>* p_target_node = reinterpret_cast<internal::tls_data_container<T>*>( parm );
		delete p_target_node;

		return;
	}

	void delete_tls_instance( void )
	{
		// pthread_getspecific()が、ロックフリーであることを祈る。
		internal::tls_data_container<T>* p_tls = reinterpret_cast<internal::tls_data_container<T>*>( pthread_getspecific( tls_key ) );
		if ( p_tls == nullptr ) return;

		// keyが消される前に、nullptr登録を終える。
		int status = pthread_setspecific( tls_key, nullptr );
		if ( status < 0 ) {
			printf( "pthread_setspecific failed for delete thread local instance, errno %d", errno );
		}

		delete p_tls;

		return;
	}

	pthread_key_t tls_key;   //!<	key for thread local storage of POSIX.

	std::shared_ptr<internal::tls_key_container> sp_key_cont;   //!<	shared pointer to key container
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_DYNAMIC_TLS_HPP_ */
