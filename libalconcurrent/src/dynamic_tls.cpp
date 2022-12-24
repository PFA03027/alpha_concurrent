/*!
 * @file	dynamic_tls.cpp
 * @brief	Dynamic allocatable thread local storage
 * @author	Teruaki Ata
 * @date	Created on 2021/01/04
 * @details	This class provide thread local storage dynamic allocation.
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#include <limits.h>
#include <pthread.h>
#include <string.h>

#include <atomic>
#include <cerrno>
#include <cstdlib>

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/dynamic_tls.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

#define STRERROR_BUFF_SIZE ( 256 )

static std::atomic<int> cur_count_of_tls_keys( 0 );
static std::atomic<int> max_count_of_tls_keys( 0 );

void error_log_output( int errno_arg, const char* p_func_name )
{
	char buff[STRERROR_BUFF_SIZE];

#if defined( _WIN32 ) || defined( _WIN64 )
	int tr_ret                   = strerror_s( buff, STRERROR_BUFF_SIZE, errno_arg );
	buff[STRERROR_BUFF_SIZE - 1] = 0;

	if ( tr_ret == 0 ) {
		internal::LogOutput(
			log_type::ERR,
			"%s failed, num of used tls key: %d, max num of used tls key: %d, errno=%d, %s",
			p_func_name, get_num_of_tls_key(), get_max_num_of_tls_key(), errno_arg, buff );
	} else {
		internal::LogOutput(
			log_type::ERR,
			"%s failed, num of used tls key: %d, max num of used tls key: %d, errno=%d, and strerror_r() also fail %d",
			p_func_name, get_num_of_tls_key(), get_max_num_of_tls_key(), errno_arg, tr_ret );
	}
#else
#if ( _POSIX_C_SOURCE >= 200112L ) && !_GNU_SOURCE
	int tr_ret                   = strerror_r( errno_arg, buff, STRERROR_BUFF_SIZE - 1 );
	buff[STRERROR_BUFF_SIZE - 1] = 0;

	if ( tr_ret == 0 ) {
		internal::LogOutput(
			log_type::ERR,
			"%s failed, num of used tls key: %d, max num of used tls key: %d, errno=%d, %s",
			p_func_name, get_num_of_tls_key(), get_max_num_of_tls_key(), errno_arg, buff );
	} else {
		internal::LogOutput(
			log_type::ERR,
			"%s failed, num of used tls key: %d, max num of used tls key: %d, errno=%d, and strerror_r() also fail %d",
			p_func_name, get_num_of_tls_key(), get_max_num_of_tls_key(), errno_arg, tr_ret );
	}
#else
	char* errstr                 = strerror_r( errno_arg, buff, STRERROR_BUFF_SIZE - 1 );
	buff[STRERROR_BUFF_SIZE - 1] = 0;

	internal::LogOutput(
		log_type::ERR,
		"%s failed, num of used tls key: %d, max num of used tls key: %d, errno=%d, %s",
		p_func_name, get_num_of_tls_key(), get_max_num_of_tls_key(), errno_arg, errstr );
#endif
#endif

	return;
}

/*!
 * @brief	call pthread_key_create() and count up the number of dynamic thread local memory
 *
 *　pthread_key_create()での割り当て数を１つ増やす。pthread_key_create()を呼び出すときに併せて呼び出す。
 *　不具合解析など使用する。
 */
void dynamic_tls_pthread_key_create( pthread_key_t* p_key, void ( *destructor )( void* ) )
{
	int status = pthread_key_create( p_key, destructor );
	if ( status != 0 ) {
		error_log_output( status, "pthread_key_create()" );
		internal::LogOutput( log_type::ERR, "PTHREAD_KEYS_MAX: %d", PTHREAD_KEYS_MAX );
		std::abort();   // because of the critical error, let's exit. TODO: should throw std::runtime_error ?
	}

	cur_count_of_tls_keys++;

	int cur_max = max_count_of_tls_keys.load();
	if ( cur_max < cur_count_of_tls_keys.load() ) {
		max_count_of_tls_keys.compare_exchange_strong( cur_max, cur_count_of_tls_keys.load() );
	}
}

/*!
 * @brief	call pthread_key_delete() and count down the number of dynamic thread local memory
 *
 *　pthread_key_create()での割り当て数を１つ減らす。pthread_key_delete()を呼び出すときに併せて呼び出す。
 *　不具合解析など使用する。
 */
void dynamic_tls_pthread_key_delete( pthread_key_t key )
{
	int status = pthread_key_delete( key );
	if ( status != 0 ) {
		error_log_output( status, "pthread_key_delete()" );
		std::abort();   // because of the critical error, let's exit. TODO: should throw std::runtime_error ?
	}

	cur_count_of_tls_keys--;
}

/*!
 * @brief	get the number of dynamic thread local memory
 *
 *　pthread_key_create()での割り当て数を取得する。
 *　不具合解析など使用する。
 */
int get_num_of_tls_key( void )
{
	return cur_count_of_tls_keys.load();
}

/*!
 * @brief	get the max number of dynamic thread local memory
 *
 *　pthread_key_create()での割り当て数を取得する。
 *　不具合解析など使用する。
 */
int get_max_num_of_tls_key( void )
{
	return max_count_of_tls_keys.load();
}

}   // namespace internal

}   // namespace concurrent
}   // namespace alpha
