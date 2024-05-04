/**
 * @file od_node.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-04
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCURRENT_INC_INTERNAL_OD_NODE_HPP_
#define ALCONCURRENT_INC_INTERNAL_OD_NODE_HPP_

#include <atomic>
#include <memory>

#include "../hazard_ptr.hpp"
#include "../lf_mem_alloc.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

/**
 * @brief node of one direction list
 *
 * @tparam T value type kept in this class
 */
template <typename T>
struct od_node {
	using value_type = T;

	hazard_ptr_handler<od_node> hph_next_;
	value_type                  v_;

#ifdef ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
#else
#if 0   // too slow...
#if __cpp_aligned_new
		ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size, std::align_val_t alignment )   // possible throw std::bad_alloc, from C++17
		{
			void* p_ans = gmem_allocate( size, static_cast<size_t>( alignment ) );
			if ( p_ans == nullptr ) {
				throw std::bad_alloc();
			}
			return p_ans;
		}
		ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++17
		{
			return gmem_allocate( size, static_cast<size_t>( alignment ) );
		}
		ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size, std::align_val_t alignment )   // possible throw std::bad_alloc, from C++17
		{
			void* p_ans = gmem_allocate( size, static_cast<size_t>( alignment ) );
			if ( p_ans == nullptr ) {
				throw std::bad_alloc();
			}
			return p_ans;
		}
		ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++17
		{
			return gmem_allocate( size, static_cast<size_t>( alignment ) );
		}
#endif
		ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size )   // possible throw std::bad_alloc, from C++11
		{
			void* p_ans = gmem_allocate( size );
			if ( p_ans == nullptr ) {
				throw std::bad_alloc();
			}
			return p_ans;
		}
		ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++11
		{
			return gmem_allocate( size );
		}
		ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size )   // possible throw std::bad_alloc, from C++11
		{
			void* p_ans = gmem_allocate( size );
			if ( p_ans == nullptr ) {
				throw std::bad_alloc();
			}
			return p_ans;
		}
		ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++11
		{
			return gmem_allocate( size );
		}

		ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size, void* ptr ) noexcept   // placement new, from C++11
		{
			return ptr;
		}
		ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size, void* ptr ) noexcept   // placement new for array, from C++11
		{
			return ptr;
		}

#if __cpp_aligned_new
		void operator delete( void* ptr, std::align_val_t alignment ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
		{
			gmem_deallocate( ptr );
		}
		void operator delete[]( void* ptr, std::align_val_t alignment ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
		{
			gmem_deallocate( ptr );
		}

		void operator delete( void* ptr, std::size_t size, std::align_val_t alignment ) noexcept   // from C++17
		{
			gmem_deallocate( ptr );
		}
		void operator delete[]( void* ptr, std::size_t size, std::align_val_t alignment ) noexcept   // from C++17
		{
			gmem_deallocate( ptr );
		}

		void operator delete( void* ptr, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
		{
			gmem_deallocate( ptr );
		}
		void operator delete[]( void* ptr, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
		{
			gmem_deallocate( ptr );
		}

		void operator delete( void* ptr, std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17
		{
			gmem_deallocate( ptr );
		}
		void operator delete[]( void* ptr, std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17
		{
			gmem_deallocate( ptr );
		}
#endif
		void operator delete( void* ptr ) noexcept   // from C++11
		{
			gmem_deallocate( ptr );
		}
		void operator delete[]( void* ptr ) noexcept   // from C++11
		{
			gmem_deallocate( ptr );
		}

		void operator delete( void* ptr, std::size_t size ) noexcept   // from C++14
		{
			gmem_deallocate( ptr );
		}
		void operator delete[]( void* ptr, std::size_t size ) noexcept   // from C++14
		{
			gmem_deallocate( ptr );
		}

		void operator delete( void* ptr, const std::nothrow_t& ) noexcept   // from C++11
		{
			gmem_deallocate( ptr );
		}
		void operator delete[]( void* ptr, const std::nothrow_t& ) noexcept   // from C++11
		{
			gmem_deallocate( ptr );
		}

		void operator delete( void* ptr, std::size_t size, const std::nothrow_t& ) noexcept   // from C++14
		{
			gmem_deallocate( ptr );
		}
		void operator delete[]( void* ptr, std::size_t size, const std::nothrow_t& ) noexcept   // from C++14
		{
			gmem_deallocate( ptr );
		}

		void operator delete( void* ptr, void* ) noexcept   // delete for area that is initialized by placement new.
		{
			// nothing to do
		}
		void operator delete[]( void* ptr, void* ) noexcept   // delete for area that is initialized by placement new.
		{
			// nothing to do
		}
#endif
#endif
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_LF_FIFO_HPP_ */
