/*!
 * @file	conf_logger.hpp
 * @brief	Logger I/F for this library
 * @author	Teruaki Ata
 * @date	Created on 2021/01/09
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef ALCONCCURRENT_INC_CONF_LOGGER_HPP_
#define ALCONCCURRENT_INC_CONF_LOGGER_HPP_

#include <execinfo.h>
#include <string.h>

#include <cstdio>
#include <memory>

#include "internal/cpp_std_configure.hpp"

namespace alpha {
namespace concurrent {

constexpr size_t CONF_LOGGER_INTERNAL_BUFF_SIZE              = 2048;
constexpr size_t ALCONCURRENT_CONF_MAX_RECORD_BACKTRACE_SIZE = 100;   //!< size of backtrace buffer size

/*!
 * @brief	log type that is used by this library
 */
enum class log_type {
	ERR,     //!< Log type is Error
	WARN,    //!< Log type is Warning
	INFO,    //!< Log type is Information
	DEBUG,   //!< Log type is debug level
	TEST,    //!< Log type is for test purpose
	DUMP     //!< Log type is dump data for debug purpose by dump() api.
};

/*!
 * @brief	log output I/F class to configure the logging.
 */
class logger_if_abst {
public:
	constexpr logger_if_abst( void ) = default;

	/*!
	 * @brief	Output a log to logger
	 */
	virtual void output_log(
		const log_type,              //!< [in]	log type for logger
		const size_t max_buf_size,   //!< [in]	max string buffer size of 3rd parameter
		const char*  p_log_str       //!< [in]	pointer log string
		) noexcept = 0;

	virtual ~logger_if_abst() = default;
};

namespace internal {

extern logger_if_abst* p_concrete_logger_if;

template <size_t BUFF_SIZE>
class fixedbuff_string {
public:
	static_assert( BUFF_SIZE > 0, "BUFF_SIZE should be greater than 0(zero)" );

	static constexpr size_t size = BUFF_SIZE;

	ALCC_INTERNAL_CONSTEXPR_CONSTRUCTOR_BODY fixedbuff_string( void ) noexcept
	{
		buff_[0] = 0;
	}
	ALCC_INTERNAL_CONSTEXPR_CONSTRUCTOR_BODY fixedbuff_string( const char* p ) noexcept
	{
		if ( p == nullptr ) {
			buff_[0] = 0;
			return;
		}
		for ( size_t i = 0; i < ( BUFF_SIZE - 1 ); i++ ) {
			buff_[i] = p[i];
			if ( p[i] == 0 ) {
				return;
			}
		}
		buff_[BUFF_SIZE - 1] = 0;
	}

	template <typename Arg1st, typename... ArgsRemain>
	fixedbuff_string( const char* p_format, Arg1st x_arg1st, ArgsRemain... x_argsremain ) noexcept
	{
		print( p_format, x_arg1st, x_argsremain... );
	}

	constexpr const char* c_str( void ) const noexcept
	{
		return buff_;
	}

	template <typename... Args>
	const fixedbuff_string& print( const char* p_format, Args... x_args ) noexcept
	{
		if ( p_format == nullptr ) {
			buff_[0] = 0;
		} else {
			snprintf( buff_, size, p_format, x_args... );
		}
		return *this;
	}

private:
	char buff_[BUFF_SIZE];
};

/*!
 * @brief	filter for logging
 *
 * Default configuration allows to pass ERR and WARN.
 * @li If compiled with ALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_INFO definition, this I/F allows to path INFO.
 * @li If compiled with ALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DEBUG definition, this I/F allows to path DEBUG.
 * @li If compiled with ALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_TEST definition, this I/F allows to path TEST.
 * @li If compiled with ALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DUMP definition, this I/F allows to path DUMP.
 */
bool is_allowed_to_output(
	const log_type lt   //!< [in]	log type to check
);

template <typename... Args>
inline void LogOutput( const log_type lt, const char* p_format, Args... args ) noexcept
{
	if ( internal::is_allowed_to_output( lt ) ) {
		char buff[CONF_LOGGER_INTERNAL_BUFF_SIZE + 1];
		snprintf( buff, CONF_LOGGER_INTERNAL_BUFF_SIZE, p_format, args... );
		buff[CONF_LOGGER_INTERNAL_BUFF_SIZE] = 0;

		internal::p_concrete_logger_if->output_log( lt, CONF_LOGGER_INTERNAL_BUFF_SIZE, buff );
	}

	return;
}

inline void LogOutput( const log_type lt, const char* p_str ) noexcept
{
	if ( internal::is_allowed_to_output( lt ) ) {
		char buff[CONF_LOGGER_INTERNAL_BUFF_SIZE + 1];
		strncpy( buff, p_str, CONF_LOGGER_INTERNAL_BUFF_SIZE );
		buff[CONF_LOGGER_INTERNAL_BUFF_SIZE] = 0;

		internal::p_concrete_logger_if->output_log( lt, CONF_LOGGER_INTERNAL_BUFF_SIZE, buff );
	}

	return;
}

}   // namespace internal

/*!
 * @brief	Set new logger I/F
 *
 * @warning
 * This I/F is not lock-free and not thread-safe. @n
 * Therefore, This I/F should be called before alpha::concurrent APIs calling.
 *
 * @note
 * even if no call this API, this library uses the default logger. @n
 * Default logger outputs a log by printf.
 */
std::unique_ptr<logger_if_abst> SetLoggerIf( std::unique_ptr<logger_if_abst> up_logger_if_inst );

/*!
 * @brief	Get the number of ERR type and WARN type log output
 *
 * This is test/debug purpose.
 */
void GetErrorWarningLogCount(
	int* p_num_err,   //!< [out] the pointer to store the number of ERR type log output
	int* p_num_warn   //!< [out] the pointer to store the number of WARN type log output
);

/*!
 * @brief	Get the number of ERR type and WARN type log output, then reset those numbers.
 *
 * This is test/debug purpose.
 */
void GetErrorWarningLogCountAndReset(
	int* p_num_err,   //!< [out] the pointer to store the number of ERR type log output
	int* p_num_warn   //!< [out] the pointer to store the number of WARN type log output
);

/*!
 * @brief	caller backtrace information
 */
struct bt_info {
	int   count_;                                             //!< backtrace data size. Zero: no data, Plus value: call stack information is valid, Minus value: information of previous allocation
	void* bt_[ALCONCURRENT_CONF_MAX_RECORD_BACKTRACE_SIZE];   //!< call stack of backtrace

	constexpr bt_info( void ) noexcept
	  : count_( 0 )
	  , bt_ { 0 }
	{
	}

	~bt_info()                          = default;
	constexpr bt_info( const bt_info& ) = default;
	constexpr bt_info( bt_info&& )      = default;
#if ( __cpp_constexpr >= 201304 )
	constexpr bt_info& operator=( const bt_info& ) = default;
	constexpr bt_info& operator=( bt_info&& )      = default;
#else
	bt_info& operator=( const bt_info& ) = default;
	bt_info& operator=( bt_info&& )      = default;
#endif

	void dump_to_log( log_type lt, char c, int id ) const noexcept;
	void invalidate( void ) noexcept
	{
		if ( count_ > 0 ) {
			count_ = -count_;
		}
	}

	static inline bt_info record_backtrace( void ) noexcept
	{
		bt_info ans;
		ans.count_ = backtrace( ans.bt_, ALCONCURRENT_CONF_MAX_RECORD_BACKTRACE_SIZE );
		return ans;
	}
};

/**
 * @brief this macro function has obsolated
 *
 * please use alpha::concurrent::bt_info::record_backtrace()
 */
#define RECORD_BACKTRACE_GET_BACKTRACE( BT_INFO_N )                 \
	{                                                               \
		BT_INFO_N = alpha::concurrent::bt_info::record_backtrace(); \
	}

/**
 * @brief this macro function has obsolated
 *
 * please use bt_info::invalidate()
 */
#define RECORD_BACKTRACE_INVALIDATE_BACKTRACE( BT_INFO_N ) \
	{                                                      \
		BT_INFO_N.invalidate();                            \
	}

}   // namespace concurrent
}   // namespace alpha

#endif /* INC_CONF_LOGGER_HPP_ */
