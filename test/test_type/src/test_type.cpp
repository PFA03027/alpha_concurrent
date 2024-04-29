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

#include "alconcurrent/internal/one_way_list_node.hpp"
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
	alpha::concurrent::internal::value_holder<int>                  a1;
	alpha::concurrent::internal::value_holder<int*>                 a10;
	alpha::concurrent::internal::value_holder<int*, true>           a100;
	alpha::concurrent::internal::value_holder<int*, false>          a101;
	alpha::concurrent::internal::value_holder<int[]>                a200;
	alpha::concurrent::internal::value_holder<int[], true>          a210;
	alpha::concurrent::internal::value_holder<int[], false>         a211;
	alpha::concurrent::internal::value_holder<int[1]>               a22;
	alpha::concurrent::internal::value_holder<std::unique_ptr<int>> a3;
	alpha::concurrent::internal::one_way_list_node<int>             a4;
	static_assert( std::is_base_of<
					   alpha::concurrent::internal::value_holder<int>,
					   alpha::concurrent::internal::one_way_list_node<int>>::value,
	               "Oh no..." );
	static_assert( std::is_base_of<
					   alpha::concurrent::internal::value_holder<int*, false>,
					   alpha::concurrent::internal::one_way_list_node<int*, false>>::value,
	               "Oh no..." );
	static_assert( std::is_base_of<
					   alpha::concurrent::internal::value_holder<int*>,
					   alpha::concurrent::internal::one_way_list_node<int*>>::value,
	               "Oh no..." );
	static_assert( std::is_base_of<
					   alpha::concurrent::internal::value_holder<std::unique_ptr<int>>,
					   alpha::concurrent::internal::one_way_list_node<std::unique_ptr<int>>>::value,
	               "Oh no..." );
	alpha::concurrent::internal::one_way_list_node<int*>       a5;
	alpha::concurrent::internal::one_way_list_node<int, false> a6;
	alpha::concurrent::internal::fifo_nd_list<int>             a7;
	alpha::concurrent::fifo_list<int>                          a8;
	alpha::concurrent::fifo_list<int*>                         b9;    // move ownership
	alpha::concurrent::fifo_list<int*, false>                  c10;   // no move ownership
	alpha::concurrent::fifo_list<test_t>                       d11;

	alpha::concurrent::stack_list<int>         a12;
	alpha::concurrent::stack_list<int*>        b12;   // move ownership
	alpha::concurrent::stack_list<int*, false> c12;   // no move ownership
	alpha::concurrent::stack_list<test_t>      d12;

	alpha::concurrent::lockfree_list<int>         a13;
	alpha::concurrent::lockfree_list<int*>        b13;   // move ownership
	alpha::concurrent::lockfree_list<int*, false> c13;   // no move ownership
	alpha::concurrent::lockfree_list<test_t>      d13;

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
