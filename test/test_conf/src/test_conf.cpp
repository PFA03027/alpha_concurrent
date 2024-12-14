/**
 * @file test_conf.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief Test for conf_logger.hpp
 * @version 0.1
 * @date 2022-12-30
 *
 * @copyright Copyright (c) 2022, Teruaki Ata <PFA03027@nifty.com>
 *
 */

#include <pthread.h>

#include <cstdint>
#include <deque>
#include <iostream>
#include <list>
#include <mutex>
#include <random>
#include <thread>

#include "gtest/gtest.h"

#include "alconcurrent/conf_logger.hpp"

class DefaultLogger_Param : public testing::TestWithParam<alpha::concurrent::log_type> {};

TEST_P( DefaultLogger_Param, Can_Call_output_log )
{
	// Arrange

	// Act
	alpha::concurrent::internal::LogOutput( GetParam(), "Test log: DefaultLogger.Can_Call_output_log #1" );
	alpha::concurrent::internal::LogOutput( GetParam(), "Test log: DefaultLogger.Can_Call_output_log #%d", 2 );

	// Assert
}

INSTANTIATE_TEST_SUITE_P( log_type_list,
                          DefaultLogger_Param,
                          testing::Values(
							  alpha::concurrent::log_type::DEBUG,
							  alpha::concurrent::log_type::DUMP,
							  alpha::concurrent::log_type::ERR,
							  alpha::concurrent::log_type::INFO,
							  alpha::concurrent::log_type::TEST,
							  alpha::concurrent::log_type::WARN ) );

class TEST_default_logger : public alpha::concurrent::logger_if_abst {
	void output_log(
		const alpha::concurrent::log_type,   //!< [in]	log type for logger
		const size_t max_buf_size,           //!< [in]	max string buffer size of 3rd parameter
		const char*  p_log_str               //!< [in]	pointer log string
		) noexcept override
	{
		printf( "%s\n", p_log_str );
		//		fflush( NULL );
		return;
	}
};

TEST( DefaultLogger, CanCall_SetLoggerIf )
{
	// Arrange
	auto p_test_logger = new TEST_default_logger;
	alpha::concurrent::SetLoggerIf( std::unique_ptr<alpha::concurrent::logger_if_abst>( p_test_logger ) );

	// Act
	auto up_pre_logger = alpha::concurrent::SetLoggerIf( nullptr );

	// Assert
	EXPECT_EQ( p_test_logger, up_pre_logger.get() );
}

TEST( BackTraceInfo, CanCall_dump )
{
	// Arrange
	alpha::concurrent::bt_info bi;
	RECORD_BACKTRACE_GET_BACKTRACE( bi );

	// Act
	bi.dump_to_log( alpha::concurrent::log_type::ERR, 'a', 1 );

	// Assert
}

TEST( BackTraceInfo, CanCall_dump_invalidate )
{
	// Arrange
	alpha::concurrent::bt_info bi;
	RECORD_BACKTRACE_INVALIDATE_BACKTRACE( bi );

	// Act
	bi.dump_to_log( alpha::concurrent::log_type::ERR, 'a', 1 );

	// Assert
}

TEST( TestGetErrorWarningLogCount, CanCall_GetErrorWarningLogCount )
{
	// Arrange

	// Act
	alpha::concurrent::GetErrorWarningLogCount( nullptr, nullptr );

	// Assert
}

TEST( TestGetErrorWarningLogCount, CanCall_GetErrorWarningLogCountAndReset )
{
	// Arrange

	// Act
	alpha::concurrent::GetErrorWarningLogCountAndReset( nullptr, nullptr );

	// Assert
}
