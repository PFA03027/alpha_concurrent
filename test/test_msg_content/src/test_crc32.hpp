/*!
 * @file	test_crc32.hpp
 * @brief
 * @author	alpha
 * @date	Created on 2021/01/24
 * @details
 *
 * Copyright (C) 2020 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef TEST_CRC32_HPP_
#define TEST_CRC32_HPP_

#include <stdint.h>
#include <stdio.h>
#include <string.h>

class test_crc32 {
public:
	static inline uint32_t crc32( const uint8_t* buf, size_t len )
	{
		return get_instance().crc32_impl( buf, len );
	}

private:
	static test_crc32& get_instance( void );

	test_crc32( void );
	uint32_t crc32_impl( const uint8_t* buf, size_t len );
};

#endif /* TEST_CRC32_HPP_ */
