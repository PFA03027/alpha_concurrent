/*!
 * @file	conf_logger.cpp
 * @brief	Logger I/F for this library
 * @author	Teruaki Ata
 * @date	Created on 2021/01/09
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#include "conf_logger.hpp"

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

logger_if_abst* p_concrete_logger_if = &default_logger_inst;
}   // namespace internal

}   // namespace concurrent
}   // namespace alpha
