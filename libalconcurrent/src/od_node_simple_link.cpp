/**
 * @file od_node_simple_link.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-15
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "alconcurrent/internal/od_node_essence.hpp"
#include "alconcurrent/lf_mem_alloc.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

#ifdef ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
#else
ALCC_INTERNAL_NODISCARD_ATTR void* od_node_simple_link::operator new( std::size_t size )   // possible throw std::bad_alloc, from C++11
{
	void* p_ans = gmem_allocate( size );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}
	return p_ans;
}
ALCC_INTERNAL_NODISCARD_ATTR void* od_node_simple_link::operator new( std::size_t size, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++11
{
	return gmem_allocate( size );
}
ALCC_INTERNAL_NODISCARD_ATTR void* od_node_simple_link::operator new[]( std::size_t size )   // possible throw std::bad_alloc, from C++11
{
	void* p_ans = gmem_allocate( size );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}
	return p_ans;
}
ALCC_INTERNAL_NODISCARD_ATTR void* od_node_simple_link::operator new[]( std::size_t size, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++11
{
	return gmem_allocate( size );
}

ALCC_INTERNAL_NODISCARD_ATTR void* od_node_simple_link::operator new( std::size_t size, void* ptr ) noexcept   // placement new, from C++11
{
	return ptr;
}
ALCC_INTERNAL_NODISCARD_ATTR void* od_node_simple_link::operator new[]( std::size_t size, void* ptr ) noexcept   // placement new for array, from C++11
{
	return ptr;
}

void od_node_simple_link::operator delete( void* ptr ) noexcept   // from C++11
{
	gmem_deallocate( ptr );
}
void od_node_simple_link::operator delete[]( void* ptr ) noexcept   // from C++11
{
	gmem_deallocate( ptr );
}

void od_node_simple_link::operator delete( void* ptr, std::size_t size ) noexcept   // from C++14
{
	gmem_deallocate( ptr );
}
void od_node_simple_link::operator delete[]( void* ptr, std::size_t size ) noexcept   // from C++14
{
	gmem_deallocate( ptr );
}

void od_node_simple_link::operator delete( void* ptr, const std::nothrow_t& ) noexcept   // from C++11
{
	gmem_deallocate( ptr );
}
void od_node_simple_link::operator delete[]( void* ptr, const std::nothrow_t& ) noexcept   // from C++11
{
	gmem_deallocate( ptr );
}

void od_node_simple_link::operator delete( void* ptr, std::size_t size, const std::nothrow_t& ) noexcept   // from C++14
{
	gmem_deallocate( ptr );
}
void od_node_simple_link::operator delete[]( void* ptr, std::size_t size, const std::nothrow_t& ) noexcept   // from C++14
{
	gmem_deallocate( ptr );
}

void od_node_simple_link::operator delete( void* ptr, void* ) noexcept   // delete for area that is initialized by placement new.
{
	// nothing to do
}
void od_node_simple_link::operator delete[]( void* ptr, void* ) noexcept   // delete for area that is initialized by placement new.
{
	// nothing to do
}

#if __cpp_aligned_new
ALCC_INTERNAL_NODISCARD_ATTR void* od_node_simple_link::operator new( std::size_t size, std::align_val_t alignment )   // possible throw std::bad_alloc, from C++17
{
	void* p_ans = gmem_allocate( size, static_cast<size_t>( alignment ) );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}
	return p_ans;
}
ALCC_INTERNAL_NODISCARD_ATTR void* od_node_simple_link::operator new( std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++17
{
	return gmem_allocate( size, static_cast<size_t>( alignment ) );
}
ALCC_INTERNAL_NODISCARD_ATTR void* od_node_simple_link::operator new[]( std::size_t size, std::align_val_t alignment )   // possible throw std::bad_alloc, from C++17
{
	void* p_ans = gmem_allocate( size, static_cast<size_t>( alignment ) );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}
	return p_ans;
}
ALCC_INTERNAL_NODISCARD_ATTR void* od_node_simple_link::operator new[]( std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++17
{
	return gmem_allocate( size, static_cast<size_t>( alignment ) );
}
void od_node_simple_link::operator delete( void* ptr, std::align_val_t alignment ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
{
	gmem_deallocate( ptr );
}
void od_node_simple_link::operator delete[]( void* ptr, std::align_val_t alignment ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
{
	gmem_deallocate( ptr );
}

void od_node_simple_link::operator delete( void* ptr, std::size_t size, std::align_val_t alignment ) noexcept   // from C++17
{
	gmem_deallocate( ptr );
}
void od_node_simple_link::operator delete[]( void* ptr, std::size_t size, std::align_val_t alignment ) noexcept   // from C++17
{
	gmem_deallocate( ptr );
}

void od_node_simple_link::operator delete( void* ptr, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
{
	gmem_deallocate( ptr );
}
void od_node_simple_link::operator delete[]( void* ptr, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
{
	gmem_deallocate( ptr );
}

void od_node_simple_link::operator delete( void* ptr, std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17
{
	gmem_deallocate( ptr );
}
void od_node_simple_link::operator delete[]( void* ptr, std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17
{
	gmem_deallocate( ptr );
}
#endif
#endif

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
