//============================================================================
// Name        : test_mem_alloc_sanitize.cpp
// Author      : Teruaki Ata
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C, Ansi-style
//============================================================================


#include <stdio.h>
#include <stdlib.h>

#include <cstdint>
#include <iostream>

#include "gtest/gtest.h"

#include "alconcurrent/lf_mem_alloc.hpp"

alpha::concurrent::param_chunk_allocation param = { 27, 2 };

alpha::concurrent::general_mem_allocator    gmem(&param, 1);

int main(void)
{
#if 0
    alpha::concurrent::general_mem_allocator* p_mem_allocator = new alpha::concurrent::general_mem_allocator( &param, 1 );

	void* test_ptr1 = p_mem_allocator->allocate( 10 );
	void* test_ptr2 = p_mem_allocator->allocate( 10 );
	void* test_ptr3 = p_mem_allocator->allocate( 10 );

    printf("b\n");

	p_mem_allocator->deallocate( test_ptr3 );
	p_mem_allocator->deallocate( test_ptr1 );
	p_mem_allocator->deallocate( test_ptr2 );

	delete p_mem_allocator;
#else
	void* test_ptr1 = gmem.allocate( 10 );
	void* test_ptr2 = gmem.allocate( 10 );
	void* test_ptr3 = gmem.allocate( 10 );

    printf("c\n");

	gmem.deallocate( test_ptr3 );
	gmem.deallocate( test_ptr1 );
	gmem.deallocate( test_ptr2 );


#endif

	printf( "number of keys of pthread_key_create(),     %d\n", alpha::concurrent::internal::get_num_of_tls_key() );
	printf( "max number of keys of pthread_key_create(), %d\n", alpha::concurrent::internal::get_max_num_of_tls_key() );

    return EXIT_SUCCESS;
}