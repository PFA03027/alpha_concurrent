/*!
 * @file	free_node_storage.hpp
 * @brief
 * @author	Teruaki Ata
 * @date	Created on 2021/01/03
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#include "alconcurrent/internal/free_node_storage.hpp"

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/lf_mem_alloc.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

////////////////////////////////////////////////////////////////////////////

void* node_of_list::operator new( std::size_t n )   // usual new...(1)
{
	void* p_ans;
#ifdef ALCONCURRENT_CONF_USE_MALLOC_FREE_LF_ALGO_NODE_ALLOC
	p_ans = std::malloc( n );
	// p_ans = ::operator new( n );
#else
	p_ans = gmem_allocate( n );
#endif

	if ( p_ans == nullptr ) throw std::bad_alloc();

	return p_ans;
}
void* node_of_list::operator new( std::size_t n, std::align_val_t alignment )   // usual new with alignment...(1) C++11/C++14 just ignore. C++17 and after uses this.
{
	void* p_ans;
#ifdef ALCONCURRENT_CONF_USE_MALLOC_FREE_LF_ALGO_NODE_ALLOC
	p_ans = std::malloc( n );
	// p_ans = ::operator new( n );
#else
#ifdef ALCONCURRENT_CONF_LF_ALGO_USE_LOCAL_ALLOCATER
	p_ans = get_gma().allocate( n, static_cast<size_t>( alignment ) );
#else
	p_ans = gmem_allocate( n, static_cast<size_t>( alignment ) );
#endif
#endif

	if ( p_ans == nullptr ) throw std::bad_alloc();

	return p_ans;
}
void node_of_list::operator delete( void* p_mem ) noexcept   // usual new...(2)
{
#ifdef ALCONCURRENT_CONF_USE_MALLOC_FREE_LF_ALGO_NODE_ALLOC
	std::free( p_mem );
	// ::operator delete( p_mem );
#else
	gmem_deallocate( p_mem );
#endif
	return;
}

void* node_of_list::operator new[]( std::size_t n )   // usual new...(1)
{
	void* p_ans;
#ifdef ALCONCURRENT_CONF_USE_MALLOC_FREE_LF_ALGO_NODE_ALLOC
	p_ans = std::malloc( n );
	// p_ans = ::operator new[]( n );
#else
	p_ans = gmem_allocate( n );
#endif

	if ( p_ans == nullptr ) throw std::bad_alloc();

	return p_ans;
}
void node_of_list::operator delete[]( void* p_mem ) noexcept   // usual new...(2)
{
#ifdef ALCONCURRENT_CONF_USE_MALLOC_FREE_LF_ALGO_NODE_ALLOC
	std::free( p_mem );
	// ::operator delete[]( p_mem );
#else
	gmem_deallocate( p_mem );
#endif
	return;
}

void* node_of_list::operator new( std::size_t n, void* p )   // placement new
{
	return p;
}
void node_of_list::operator delete( void* p, void* p2 ) noexcept   // placement delete...(3)
{
	return;
}

general_mem_allocator_statistics node_of_list::get_statistics( void )
{
#ifdef ALCONCURRENT_CONF_USE_MALLOC_FREE_LF_ALGO_NODE_ALLOC
	return general_mem_allocator_statistics {};
#else
	return gmem_get_statistics();
#endif
}

void node_of_list::release_ownership( void )
{
	return;
}

void node_of_list::teardown_by_recycle( void )
{
	return;
}

}   // namespace internal

}   // namespace concurrent
}   // namespace alpha
