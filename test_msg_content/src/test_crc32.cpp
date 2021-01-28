/*!
 * @file	test_crc32.cpp
 * @brief
 * @author	alpha
 * @date	Created on 2021/01/24
 * @details
 *
 * Copyright (C) 2021 by alpha <e-mail address>
 */

/*
 * https://qiita.com/tobira-code/items/dbcffc41f54201130b6c
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "test_crc32.hpp"

static uint32_t crc32_table[256];

test_crc32& test_crc32::get_instance( void )
{
	static test_crc32 singleton;
	return singleton;
}

test_crc32::test_crc32( void )
{
	// make_crc32_table
	for ( uint32_t i = 0; i < 256; i++ ) {
		uint32_t c = i << 24;
		for ( int j = 0; j < 8; j++ ) {
			c = ( c << 1 ) ^ ( ( c & 0x80000000 ) ? 0x04c11db7 : 0 );
		}
		crc32_table[i] = c;
	}

	return;
}

uint32_t test_crc32::crc32_impl( const uint8_t* buf, size_t len )
{
	uint32_t c = 0xFFFFFFFF;
	for ( size_t i = 0; i < len; i++ ) {
		c = ( c << 8 ) ^ crc32_table[( ( c >> 24 ) ^ buf[i] ) & 0xFF];
	}
	return c;
}
