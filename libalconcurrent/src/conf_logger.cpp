/*!
 * @file	conf_logger.cpp
 * @brief	Logger I/F for this library
 * @author	Teruaki Ata
 * @date	Created on 2021/01/09
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#include <atomic>

#include "alconcurrent/conf_logger.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

class default_logger : public logger_if_abst {
	void output_log(
		const log_type,              //!< [in]	log type for logger
		const size_t max_buf_size,   //!< [in]	max string buffer size of 3rd parameter
		const char*  p_log_str       //!< [in]	pointer log string
		) override
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
#ifdef CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_INFO
			ans = true;
#endif
			break;
		case log_type::DEBUG:
#ifdef CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DEBUG
			ans = true;
#endif
			break;
		case log_type::TEST:
#ifdef CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_TEST
			ans = true;
#endif
			break;
		case log_type::DUMP:
#ifdef CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DUMP
			ans = true;
#endif
			break;
		default:
			break;
	}

	return ans;
}

}   // namespace internal

void SetLoggerIf( std::unique_ptr<logger_if_abst> up_logger_if_inst )
{
	static std::unique_ptr<logger_if_abst> up_keep_inst_of_logger_if( nullptr );

	internal::p_concrete_logger_if = nullptr;

	up_keep_inst_of_logger_if = std::move( up_logger_if_inst );

	internal::p_concrete_logger_if = up_keep_inst_of_logger_if.get();

	return;
}

void GetErrorWarningLogCount(
	int* p_num_err,   //!< [out] the pointer to store the number of ERR type log output
	int* p_num_warn   //!< [out] the pointer to store the number of WARN type log output
)
{
	if ( p_num_err != nullptr ) {
		*p_num_err = internal::err_log_count.load();
	}
	if ( p_num_warn != nullptr ) {
		*p_num_warn = internal::warn_log_count.load();
	}
	return;
}

void GetErrorWarningLogCountAndReset(
	int* p_num_err,   //!< [out] the pointer to store the number of ERR type log output
	int* p_num_warn   //!< [out] the pointer to store the number of WARN type log output
)
{
	if ( p_num_err != nullptr ) {
		*p_num_err = internal::err_log_count.load();
	}
	if ( p_num_warn != nullptr ) {
		*p_num_warn = internal::warn_log_count.load();
	}

	internal::err_log_count.store( 0 );
	internal::warn_log_count.store( 0 );

	return;
}

}   // namespace concurrent
}   // namespace alpha
