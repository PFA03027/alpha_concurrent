/*!
 * @file	conf_logger.cpp
 * @brief	Logger I/F for this library
 * @author	Teruaki Ata
 * @date	Created on 2021/01/09
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#include <cxxabi.h>

#include <atomic>
#include <cstdlib>
#include <string>

#include "alconcurrent/conf_logger.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

class default_logger : public logger_if_abst {
public:
	constexpr default_logger( void ) = default;

	void output_log(
		const log_type,              //!< [in]	log type for logger
		const size_t max_buf_size,   //!< [in]	max string buffer size of 3rd parameter
		const char*  p_log_str       //!< [in]	pointer log string
		) noexcept override
	{
		printf( "%s\n", p_log_str );
		//		fflush( NULL );
		return;
	}
};

default_logger default_logger_inst;

logger_if_abst* p_concrete_logger_if = &default_logger_inst;   //!< TODO should we use std::atomic<T*> ?

static std::atomic<int> err_log_count( 0 );
static std::atomic<int> warn_log_count( 0 );

bool is_allowed_to_output(
	const log_type lt   //!< [in]	log type to check
)
{
	bool ans = false;
	switch ( lt ) {
		case log_type::ERR:
			ans = true;
			err_log_count++;
			break;
		case log_type::WARN:
			ans = true;
			warn_log_count++;
			break;
		case log_type::INFO:
#ifdef ALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_INFO
			ans = true;
#endif
			break;
		case log_type::DEBUG:
#ifdef ALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DEBUG
			ans = true;
#endif
			break;
		case log_type::TEST:
#ifdef ALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_TEST
			ans = true;
#endif
			break;
		case log_type::DUMP:
#ifdef ALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DUMP
			ans = true;
#endif
			break;
		default:
			break;
	}

	return ans;
}

}   // namespace internal

/////////////////////////////////////////////////////////////////////////////////////////////////////

static char* search_char( char* p_str, char c, size_t max )
{
	for ( size_t i = 0; i < max; i++ ) {
		if ( *p_str == c ) {
			return p_str;
		}
		p_str++;
	}

	return nullptr;
}

static std::string demangle_symbol( char* p_raw_symbol_str )
{
	if ( p_raw_symbol_str == nullptr ) {
		return std::string( "Fail to demangle: argument p_raw_symbol_str is nullptr" );
	}

	size_t demangled_sz  = 1024;
	char*  demangled_str = (char*)malloc( demangled_sz );
	if ( !demangled_str ) {
		perror( "malloc() returns NULL" );
		return std::string( "malloc() returns NULL for symbol demangle" );
	}

	//  strings[j] には "ファイル名 ( シンボル名 + オフセット) アドレス" という形式の文字列が格納されてるので
	//  "("　から "+"　の部分を探して，シンボル名を切り出す
	char* p_start  = search_char( p_raw_symbol_str, '(', demangled_sz );
	char* p_offset = search_char( p_start, '+', demangled_sz );

	std::string ans( p_raw_symbol_str );
	if ( p_start != nullptr && p_offset != nullptr ) {
		if ( p_offset - p_start > 1 ) {
			// シンボル名が見つかった場合は demangle する
			*p_start       = '\0';
			*p_offset      = '\0';
			char* p_symbol = ( p_start + 1 );
			char* ret      = abi::__cxa_demangle( p_symbol, demangled_str, &demangled_sz, NULL );
			if ( ret ) {
				demangled_str = ret;
			} else {
				ret = nullptr;
			}
			ans = std::string( p_raw_symbol_str );
			ans += "(";
			ans += ( ret != nullptr ) ? ret : "";
			ans += "+";
			ans += ( p_offset + 1 );
			ans += ")";
		}
	}
	free( demangled_str );

	return ans;
}

void bt_info::dump_to_log( log_type lt, char c, int id ) const noexcept
{
	if ( count_ == 0 ) {
		internal::LogOutput( lt, "[%d-%c] no back trace. this slot has not allocated yet.", id, c );
		return;
	}

	internal::LogOutput( lt, "[%d-%c] backtrace count value = %d", id, c, count_ );

	int    actual_count = ( count_ < 0 ) ? -count_ : count_;
	char** bt_strings   = backtrace_symbols( bt_, actual_count );
	for ( int i = 0; i < actual_count; i++ ) {
		auto symbol = demangle_symbol( bt_strings[i] );
		internal::LogOutput( lt, "[%d-%c] [%d] %s", id, c, i, symbol.c_str() );
	}
	std::free( bt_strings );

	return;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<logger_if_abst> SetLoggerIf( std::unique_ptr<logger_if_abst> up_logger_if_inst )
{
	static std::unique_ptr<logger_if_abst> up_keep_inst_of_logger_if( nullptr );

	internal::p_concrete_logger_if = nullptr;

	up_keep_inst_of_logger_if.swap( up_logger_if_inst );

	if ( up_keep_inst_of_logger_if == nullptr ) {
		internal::p_concrete_logger_if = &internal::default_logger_inst;
	} else {
		internal::p_concrete_logger_if = up_keep_inst_of_logger_if.get();
	}

	return up_logger_if_inst;
}

void GetErrorWarningLogCount(
	int* p_num_err,   //!< [out] the pointer to store the number of ERR type log output
	int* p_num_warn   //!< [out] the pointer to store the number of WARN type log output
)
{
	if ( p_num_err != nullptr ) {
		*p_num_err = internal::err_log_count.load( std::memory_order_acquire );
	}
	if ( p_num_warn != nullptr ) {
		*p_num_warn = internal::warn_log_count.load( std::memory_order_acquire );
	}
	return;
}

void GetErrorWarningLogCountAndReset(
	int* p_num_err,   //!< [out] the pointer to store the number of ERR type log output
	int* p_num_warn   //!< [out] the pointer to store the number of WARN type log output
)
{
	if ( p_num_err != nullptr ) {
		*p_num_err = internal::err_log_count.load( std::memory_order_acquire );
	}
	if ( p_num_warn != nullptr ) {
		*p_num_warn = internal::warn_log_count.load( std::memory_order_acquire );
	}

	internal::err_log_count.store( 0, std::memory_order_release );
	internal::warn_log_count.store( 0, std::memory_order_release );

	return;
}

}   // namespace concurrent
}   // namespace alpha
