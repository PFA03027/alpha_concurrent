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
#include "alconcurrent/lf_mem_alloc_type.hpp"
#include "alconcurrent/one_way_list_node.hpp"

struct test_t {
	int a;
	int b;
};

int main( void )
{
	alpha::concurrent::internal::value_holder_available_lf_atomic<int>                    a1;
	alpha::concurrent::internal::value_holder_available_lf_atomic_pointer_ownership<int*> a2;
	alpha::concurrent::internal::value_holder_non_atomic<std::unique_ptr<int>>            a3;
	alpha::concurrent::internal::one_way_list_node<int>                                   a4;
	static_assert( std::is_base_of<
					   alpha::concurrent::internal::value_holder_available_lf_atomic<int>,
					   alpha::concurrent::internal::one_way_list_node<int>>::value,
	               "Oh no..." );
	static_assert( std::is_base_of<
					   alpha::concurrent::internal::value_holder_available_lf_atomic<int*>,
					   alpha::concurrent::internal::one_way_list_node<int*, false>>::value,
	               "Oh no..." );
	static_assert( std::is_base_of<
					   alpha::concurrent::internal::value_holder_available_lf_atomic_pointer_ownership<int*>,
					   alpha::concurrent::internal::one_way_list_node<int*>>::value,
	               "Oh no..." );
	static_assert( std::is_base_of<
					   alpha::concurrent::internal::value_holder_non_atomic<std::unique_ptr<int>>,
					   alpha::concurrent::internal::one_way_list_node<std::unique_ptr<int>>>::value,
	               "Oh no..." );
	//	alpha::concurrent::internal::one_way_list_node<int*> a5;
	//	alpha::concurrent::internal::one_way_list_node<int, false> a6;
	//	alpha::concurrent::internal::fifo_nd_list<int> a7;
	//	alpha::concurrent::fifo_list<int>         a8;
	//	alpha::concurrent::fifo_list<int*>        b9;
	//	alpha::concurrent::fifo_list<int*, false> c10;
	//	alpha::concurrent::fifo_list<test_t>      d11;

	static_assert( std::is_same<typename std::decay<char*>::type, char*>::value, "Oh ..." );
	static_assert( std::is_same<typename std::decay<const char*>::type, const char*>::value, "Oh ..." );
	static_assert( std::is_same<typename std::decay<char[]>::type, char*>::value, "Oh ..." );
	static_assert( std::is_same<typename std::decay<const char[]>::type, const char*>::value, "Oh ..." );
	static_assert( std::is_same<typename std::decay<char[5]>::type, char*>::value, "Oh ..." );
	static_assert( std::is_same<typename std::decay<const char[5]>::type, const char*>::value, "Oh ..." );

	return EXIT_SUCCESS;
}
