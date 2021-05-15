//============================================================================
// Name        : test_mem_alloc.cpp
// Author      : Teruaki Ata
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C, Ansi-style
//============================================================================

#include <stdio.h>
#include <stdlib.h>

#include <cstdint>
#include <iostream>

#include "alconcurrent/lf_mem_alloc.hpp"

alpha::concurrent::param_chunk_allocation param = { 27, 2 };

void test_chunk_header_multi_slot( void )
{
	alpha::concurrent::chunk_header_multi_slot* p_chms = new alpha::concurrent::chunk_header_multi_slot( param );

	void* test_ptr1 = p_chms->allocate_mem_slot();
	void* test_ptr2 = p_chms->allocate_mem_slot();
	void* test_ptr3 = p_chms->allocate_mem_slot();

	if ( test_ptr1 == nullptr ) {
		std::cout << "NGGGGGGgggggg #1 !" << std::endl;
		exit( 1 );
	}

	if ( test_ptr2 == nullptr ) {
		std::cout << "NGGGGGGgggggg #2 !" << std::endl;
		exit( 1 );
	}

	if ( test_ptr3 != nullptr ) {
		std::cout << "NGGGGGGgggggg #3 !" << std::endl;
		exit( 1 );
	}

	if ( p_chms->recycle_mem_slot( test_ptr3 ) != false ) {
		std::cout << "NGGGGGGgggggg #4 !" << std::endl;
		exit( 1 );
	}

	if ( p_chms->recycle_mem_slot( reinterpret_cast<void*>( test_ptr1 + 1 ) ) != false ) {
		std::cout << "NGGGGGGgggggg #5 !" << std::endl;
		exit( 1 );
	}

	if ( p_chms->recycle_mem_slot( test_ptr1 ) != true ) {
		std::cout << "NGGGGGGgggggg #6 !" << std::endl;
		exit( 1 );
	}

	if ( p_chms->recycle_mem_slot( test_ptr2 ) != true ) {
		std::cout << "NGGGGGGgggggg #7 !" << std::endl;
		exit( 1 );
	}

	delete p_chms;
}

void test_chunk_list( void )
{
	alpha::concurrent::chunk_list* p_ch_lst = new alpha::concurrent::chunk_list( param );

	void* test_ptr1 = p_ch_lst->allocate_mem_slot();
	void* test_ptr2 = p_ch_lst->allocate_mem_slot();
	void* test_ptr3 = p_ch_lst->allocate_mem_slot();

	if ( test_ptr1 == nullptr ) {
		std::cout << "NGGGGGGgggggg #1 !" << std::endl;
		exit( 1 );
	}

	if ( test_ptr2 == nullptr ) {
		std::cout << "NGGGGGGgggggg #2 !" << std::endl;
		exit( 1 );
	}

	if ( test_ptr3 == nullptr ) {
		std::cout << "NGGGGGGgggggg #3 !" << std::endl;
		exit( 1 );
	}

	if ( p_ch_lst->recycle_mem_slot( test_ptr3 ) != true ) {
		std::cout << "NGGGGGGgggggg #4 !" << std::endl;
		exit( 1 );
	}

	if ( p_ch_lst->recycle_mem_slot( reinterpret_cast<void*>( test_ptr1 + 1 ) ) != false ) {
		std::cout << "NGGGGGGgggggg #5 !" << std::endl;
		exit( 1 );
	}

	if ( p_ch_lst->recycle_mem_slot( test_ptr1 ) != true ) {
		std::cout << "NGGGGGGgggggg #6 !" << std::endl;
		exit( 1 );
	}

	if ( p_ch_lst->recycle_mem_slot( test_ptr2 ) != true ) {
		std::cout << "NGGGGGGgggggg #7 !" << std::endl;
		exit( 1 );
	}

	delete p_ch_lst;
}

void test_general_mem_allocator( void )
{
	alpha::concurrent::param_chunk_allocation param[] = {
		{ 27, 2 },
		{ 100, 2 },
	};

	alpha::concurrent::general_mem_allocator* p_mem_allocator = new alpha::concurrent::general_mem_allocator( param, 2 );

	void* test_ptr1 = p_mem_allocator->allocate( 10 );
	void* test_ptr2 = p_mem_allocator->allocate( 100 );
	void* test_ptr3 = p_mem_allocator->allocate( 1000 );

	if ( test_ptr1 == nullptr ) {
		std::cout << "NGGGGGGgggggg #1 !" << std::endl;
		exit( 1 );
	}

	if ( test_ptr2 == nullptr ) {
		std::cout << "NGGGGGGgggggg #2 !" << std::endl;
		exit( 1 );
	}

	if ( test_ptr3 == nullptr ) {
		std::cout << "NGGGGGGgggggg #3 !" << std::endl;
		exit( 1 );
	}

	p_mem_allocator->deallocate( test_ptr3 );
	p_mem_allocator->deallocate( test_ptr1 );
	p_mem_allocator->deallocate( test_ptr2 );

	delete p_mem_allocator;
}

int main( void )
{
	puts( "!!!Hello Test World!!!" );

	test_chunk_header_multi_slot();
	test_chunk_list();

	puts( "!!!End Test World!!!" );
	return EXIT_SUCCESS;
}
