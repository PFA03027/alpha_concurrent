/*!
 * @file	conf_logger.hpp
 * @brief	Logger I/F for this library
 * @author	Teruaki Ata
 * @date	Created on 2021/01/09
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef INC_CONF_LOGGER_HPP_
#define INC_CONF_LOGGER_HPP_

#include <stdio.h>
#include <string.h>

#include <memory>

#define CONF_LOGGER_INTERNAL_BUFF_SIZE ( 2048 )

namespace alpha {
namespace concurrent {

/*!
 * @breif	log type that is used by this library
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
 * @breif	log output I/F class to configure the logging.
 */
class logger_if_abst {
public:
	/*!
	 * @breif	Output a log to logger
	 *
	 *
	 */
	virtual void output_log(
		const log_type,              //!< [in]	log type for logger
		const size_t max_buf_size,   //!< [in]	max string buffer size of 3rd parameter
		const char*  p_log_str       //!< [in]	pointer log string
		) = 0;

	virtual ~logger_if_abst()
	{
	}
};

namespace internal {

extern logger_if_abst* p_concrete_logger_if;

/*!
 * @breif	filter for logging
 *
 * Default configuration allows to pass ERR and WARN.
 * @li If compiled with CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_INFO definition, this I/F allows to path INFO.
 * @li If compiled with CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DEBUG definition, this I/F allows to path DEBUG.
 * @li If compiled with CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_TEST definition, this I/F allows to path TEST.
 * @li If compiled with CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DUMP definition, this I/F allows to path DUMP.
 */
bool is_allowed_to_output(
	const log_type lt   //!< [in]	log type to check
);

template <typename... Args>
inline void LogOutput( const log_type lt, const char* p_format, Args... args )
{
	if ( internal::is_allowed_to_output( lt ) ) {
		char buff[CONF_LOGGER_INTERNAL_BUFF_SIZE + 1];
		snprintf( buff, CONF_LOGGER_INTERNAL_BUFF_SIZE, p_format, args... );
		buff[CONF_LOGGER_INTERNAL_BUFF_SIZE] = 0;

		internal::p_concrete_logger_if->output_log( lt, CONF_LOGGER_INTERNAL_BUFF_SIZE, buff );
	}

	return;
}

inline void LogOutput( const log_type lt, const char* p_str )
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
 * @breif	Set new logger I/F
 *
 * @warning
 * This I/F is not lock-free and not thread-safe. @n
 * Therefore, This I/F should be called before alpha::concurrent APIs calling.
 *
 * @note
 * even if no call this API, this library uses the default logger. @n
 * Default logger outputs a log by printf.
 */
void SetLoggerIf( std::unique_ptr<logger_if_abst> up_logger_if_inst );

/*!
 * @breif	Get the number of ERR type and WARN type log output
 *
 * This is test/debug purpose.
 */
void GetErrorWarningLogCount(
	int* p_num_err,   //!< [out] the pointer to store the number of ERR type log output
	int* p_num_warn   //!< [out] the pointer to store the number of WARN type log output
);

/*!
 * @breif	Get the number of ERR type and WARN type log output, then reset those numbers.
 *
 * This is test/debug purpose.
 */
void GetErrorWarningLogCountAndReset(
	int* p_num_err,   //!< [out] the pointer to store the number of ERR type log output
	int* p_num_warn   //!< [out] the pointer to store the number of WARN type log output
);

}   // namespace concurrent
}   // namespace alpha

#endif /* INC_CONF_LOGGER_HPP_ */
