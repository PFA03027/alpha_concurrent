//============================================================================
// Name        : test_mem_alloc_sanitize.cpp
// Author      : Teruaki Ata
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C, Ansi-style
//============================================================================

#include <stdio.h>
#include <stdlib.h>

#include <atomic>
#include <cstdint>
#include <iostream>

#include "gtest/gtest.h"

#include "alconcurrent/lf_mem_alloc.hpp"

alpha::concurrent::param_chunk_allocation param = { 27, 2 };

alpha::concurrent::general_mem_allocator gmem( &param, 1 );

int               data = 0;
std::atomic<bool> ready( false );   //

#if 0
// GCC ThreadSanitizer reports the error for below style.
// This may be false positive.
// https://gcc.gnu.org/bugzilla//show_bug.cgi?id=69204
void thread1()
{
  data = 42;  //
  std::atomic_thread_fence(std::memory_order_release);  //
  ready.store(true, std::memory_order_relaxed);         //
}

void thread2()
{
  while (!ready.load(std::memory_order_relaxed))        //
    ;
  std::atomic_thread_fence(std::memory_order_acquire);  //
  assert(data == 42);  //
}
#else
// On the other hand, GCC ThreadSanitizer does not report any error for below style.
void thread1()
{
	data = 42;
	ready.store( true, std::memory_order_release );
}

void thread2()
{
	while ( !ready.load( std::memory_order_acquire ) )
		;
	assert( data == 42 );
}
#endif

int main( void )
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

	printf( "c\n" );

	gmem.deallocate( test_ptr3 );
	gmem.deallocate( test_ptr1 );
	gmem.deallocate( test_ptr2 );

	// sanitizer check
	// char* p_test = new char[10];
	// p_test[10] = '7';

#endif

	std::thread t2( thread2 );
	std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
	std::thread t1( thread1 );

	t2.join();
	t1.join();

	printf( "number of keys of pthread_key_create(),     %d\n", alpha::concurrent::internal::get_num_of_tls_key() );
	printf( "max number of keys of pthread_key_create(), %d\n", alpha::concurrent::internal::get_max_num_of_tls_key() );

	return EXIT_SUCCESS;
}
