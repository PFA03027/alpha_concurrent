/**
 * @file atomic_share_ptr.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2025-02-15
 *
 * @copyright Copyright (c) 2025, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include <deque>

#include "alconcurrent/experiment/internal/lf_shared_ptr.hpp"
#include "alconcurrent/hazard_ptr.hpp"
#include "alconcurrent/lf_mem_alloc.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

struct keeper_deferred_delete {
	control_block_base* p_;
	keeper_deferred_delete( control_block_base* p ) noexcept
	  : p_( p )
	{
	}
	keeper_deferred_delete( const keeper_deferred_delete& )            = delete;
	keeper_deferred_delete& operator=( const keeper_deferred_delete& ) = delete;
	keeper_deferred_delete( keeper_deferred_delete&& src ) noexcept
	  : p_( src.p_ )
	{
		src.p_ = nullptr;
	}
	keeper_deferred_delete& operator=( keeper_deferred_delete&& src ) noexcept
	{
		if ( this == &src ) return *this;
		p_     = src.p_;
		src.p_ = nullptr;
		return *this;
	}
	~keeper_deferred_delete()
	{
		delete p_;
	}

	control_block_base* release( void ) noexcept
	{
		control_block_base* p_ans = p_;
		p_                        = nullptr;
		return p_ans;
	}
};

static std::mutex                         mtx;
static std::deque<keeper_deferred_delete> deq_deferred_delete;

// defered reclamation
void control_block_base::retire( control_block_base* p )
{
	std::lock_guard<std::mutex> lock( mtx );

	if ( deq_deferred_delete.size() >= 1 ) {
		if ( hazard_ptr_mgr::CheckPtrIsHazardPtr( deq_deferred_delete.front().p_ ) ) {
			deq_deferred_delete.pop_front();
		}
	}
	if ( deq_deferred_delete.size() >= 1 ) {
		if ( hazard_ptr_mgr::CheckPtrIsHazardPtr( deq_deferred_delete.front().p_ ) ) {
			deq_deferred_delete.pop_front();
		}
	}
	deq_deferred_delete.emplace_back( p );
}

#ifdef ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
#else
ALCC_INTERNAL_NODISCARD_ATTR void* control_block_base::operator new( std::size_t size )   // possible throw std::bad_alloc, from C++11
{
	void* p_ans = gmem_allocate( size );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}
	return p_ans;
}
ALCC_INTERNAL_NODISCARD_ATTR void* control_block_base::operator new( std::size_t size, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++11
{
	return gmem_allocate( size );
}
ALCC_INTERNAL_NODISCARD_ATTR void* control_block_base::operator new[]( std::size_t size )   // possible throw std::bad_alloc, from C++11
{
	void* p_ans = gmem_allocate( size );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}
	return p_ans;
}
ALCC_INTERNAL_NODISCARD_ATTR void* control_block_base::operator new[]( std::size_t size, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++11
{
	return gmem_allocate( size );
}

ALCC_INTERNAL_NODISCARD_ATTR void* control_block_base::operator new( std::size_t size, void* ptr ) noexcept   // placement new, from C++11
{
	return ptr;
}
ALCC_INTERNAL_NODISCARD_ATTR void* control_block_base::operator new[]( std::size_t size, void* ptr ) noexcept   // placement new for array, from C++11
{
	return ptr;
}

void control_block_base::operator delete( void* ptr ) noexcept   // from C++11
{
	gmem_deallocate( ptr );
}
void control_block_base::operator delete[]( void* ptr ) noexcept   // from C++11
{
	gmem_deallocate( ptr );
}

void control_block_base::operator delete( void* ptr, std::size_t size ) noexcept   // from C++14
{
	gmem_deallocate( ptr );
}
void control_block_base::operator delete[]( void* ptr, std::size_t size ) noexcept   // from C++14
{
	gmem_deallocate( ptr );
}

void control_block_base::operator delete( void* ptr, const std::nothrow_t& ) noexcept   // from C++11
{
	gmem_deallocate( ptr );
}
void control_block_base::operator delete[]( void* ptr, const std::nothrow_t& ) noexcept   // from C++11
{
	gmem_deallocate( ptr );
}

void control_block_base::operator delete( void* ptr, std::size_t size, const std::nothrow_t& ) noexcept   // from C++14
{
	gmem_deallocate( ptr );
}
void control_block_base::operator delete[]( void* ptr, std::size_t size, const std::nothrow_t& ) noexcept   // from C++14
{
	gmem_deallocate( ptr );
}

void control_block_base::operator delete( void* ptr, void* ) noexcept   // delete for area that is initialized by placement new.
{
	// nothing to do
}
void control_block_base::operator delete[]( void* ptr, void* ) noexcept   // delete for area that is initialized by placement new.
{
	// nothing to do
}

#if __cpp_aligned_new >= 201606
ALCC_INTERNAL_NODISCARD_ATTR void* control_block_base::operator new( std::size_t size, std::align_val_t alignment )   // possible throw std::bad_alloc, from C++17
{
	void* p_ans = gmem_allocate( size, static_cast<size_t>( alignment ) );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}
	return p_ans;
}
ALCC_INTERNAL_NODISCARD_ATTR void* control_block_base::operator new( std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++17
{
	return gmem_allocate( size, static_cast<size_t>( alignment ) );
}
ALCC_INTERNAL_NODISCARD_ATTR void* control_block_base::operator new[]( std::size_t size, std::align_val_t alignment )   // possible throw std::bad_alloc, from C++17
{
	void* p_ans = gmem_allocate( size, static_cast<size_t>( alignment ) );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}
	return p_ans;
}
ALCC_INTERNAL_NODISCARD_ATTR void* control_block_base::operator new[]( std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++17
{
	return gmem_allocate( size, static_cast<size_t>( alignment ) );
}
void control_block_base::operator delete( void* ptr, std::align_val_t alignment ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
{
	gmem_deallocate( ptr );
}
void control_block_base::operator delete[]( void* ptr, std::align_val_t alignment ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
{
	gmem_deallocate( ptr );
}

void control_block_base::operator delete( void* ptr, std::size_t size, std::align_val_t alignment ) noexcept   // from C++17
{
	gmem_deallocate( ptr );
}
void control_block_base::operator delete[]( void* ptr, std::size_t size, std::align_val_t alignment ) noexcept   // from C++17
{
	gmem_deallocate( ptr );
}

void control_block_base::operator delete( void* ptr, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
{
	gmem_deallocate( ptr );
}
void control_block_base::operator delete[]( void* ptr, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
{
	gmem_deallocate( ptr );
}

void control_block_base::operator delete( void* ptr, std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17
{
	gmem_deallocate( ptr );
}
void control_block_base::operator delete[]( void* ptr, std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17
{
	gmem_deallocate( ptr );
}
#endif
#endif

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
