/*
 * test_type.cpp
 *
 *  Created on: 2021/10/31
 *      Author: alpha
 */
#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <type_traits>

#include "alconcurrent/lf_fifo.hpp"
#include "alconcurrent/lf_list.hpp"
#include "alconcurrent/lf_mem_alloc_type.hpp"
#include "alconcurrent/lf_stack.hpp"

struct test_t {
	int a;
	int b;
};

int main( void )
{

	alpha::concurrent::fifo_list<int>         a8;
	alpha::concurrent::fifo_list<int*>        b9;    // move ownership
	alpha::concurrent::fifo_list<int*, false> c10;   // no move ownership
	alpha::concurrent::fifo_list<test_t>      d11;

	alpha::concurrent::stack_list<int>         a12;
	alpha::concurrent::stack_list<int*>        b12;   // move ownership
	alpha::concurrent::stack_list<int*, false> c12;   // no move ownership
	alpha::concurrent::stack_list<test_t>      d12;

	alpha::concurrent::old_lockfree_list<int>         a13;
	alpha::concurrent::old_lockfree_list<int*>        b13;   // move ownership
	alpha::concurrent::old_lockfree_list<int*, false> c13;   // no move ownership
	alpha::concurrent::old_lockfree_list<test_t>      d13;

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

	return EXIT_SUCCESS;
}
