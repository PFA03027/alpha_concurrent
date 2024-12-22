/*
 * test_type.cpp
 *
 *  Created on: 2021/10/31
 *      Author: alpha
 */

#include <cstdio>
#include <cstdlib>

#include <memory>
#include <type_traits>

#include "gtest/gtest.h"

#include "alconcurrent/lf_fifo.hpp"
#include "alconcurrent/lf_list.hpp"
#include "alconcurrent/lf_stack.hpp"

TEST( TestTypes, DoCompile )
{
	alpha::concurrent::fifo_list<int>  a8;
	alpha::concurrent::fifo_list<int*> b9;   // move ownership

	alpha::concurrent::stack_list<int>  a12;
	alpha::concurrent::stack_list<int*> b12;   // move ownership

	alpha::concurrent::lockfree_list<int>  a13;
	alpha::concurrent::lockfree_list<int*> b13;   // move ownership

	static_assert( std::is_same<typename std::decay<char*>::type, char*>::value, "Oh ..." );
	static_assert( std::is_same<typename std::decay<const char*>::type, const char*>::value, "Oh ..." );
	static_assert( std::is_same<typename std::decay<char[]>::type, char*>::value, "Oh ..." );
	static_assert( std::is_same<typename std::decay<const char[]>::type, const char*>::value, "Oh ..." );
	static_assert( std::is_same<typename std::decay<char[5]>::type, char*>::value, "Oh ..." );
	static_assert( std::is_same<typename std::decay<const char[5]>::type, const char*>::value, "Oh ..." );
	static_assert( !std::is_pointer<const char[]>::value, "Oh ..." );
	static_assert( !std::is_pointer<const char[5]>::value, "Oh ..." );
	static_assert( std::is_array<const char[]>::value, "Oh ..." );
	static_assert( std::is_array<const char[5]>::value, "Oh ..." );
}
