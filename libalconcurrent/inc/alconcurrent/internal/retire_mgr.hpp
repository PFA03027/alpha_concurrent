/**
 * @file retire_mgr.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-04
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCURRENT_INC_INTERNAL_RETIRE_MGR_HPP_
#define ALCONCURRENT_INC_INTERNAL_RETIRE_MGR_HPP_

#include <atomic>
#include <memory>
#include <mutex>

#include "../hazard_ptr.hpp"
#include "../lf_mem_alloc.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

struct retire_node_abst {
	retire_node_abst* p_next_ = nullptr;

	constexpr retire_node_abst( void ) = default;
	virtual ~retire_node_abst()        = default;

	virtual void* get_retire_pointer( void ) const noexcept = 0;
};

/**
 * @brief
 *
 * @tparam T
 * @tparam Deleter please refer to std::deleter<T>. this class should not throw execption
 */
template <typename T, typename Deleter>
struct retire_node : public retire_node_abst {
	static_assert( std::is_nothrow_default_constructible<Deleter>::value &&
	                   std::is_nothrow_copy_constructible<Deleter>::value &&
	                   std::is_nothrow_move_constructible<Deleter>::value,
	               "Deleter should be nothrow of constructors" );

	T*      p_retire_;
	Deleter deleter_;

	constexpr retire_node( T* p_retire_arg, const Deleter& deleter_arg ) noexcept
	  : retire_node_abst {}
	  , p_retire_( p_retire_arg )
	  , deleter_( deleter_arg )
	{
	}
	constexpr retire_node( T* p_retire_arg, Deleter&& deleter_arg ) noexcept
	  : retire_node_abst {}
	  , p_retire_( p_retire_arg )
	  , deleter_( std::move( deleter_arg ) )
	{
	}

	~retire_node() override
	{
		deleter_( p_retire_ );
	}

	void* get_retire_pointer( void ) const noexcept override
	{
		return reinterpret_cast<void*>( p_retire_ );
	}

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

/**
 * @brief retire管理用I/Fクラス（Facadeクラス）
 *
 * @todo シングルトン化すべきか？
 */
class retire_mgr {
public:
	/**
	 * @brief
	 *
	 * @tparam T
	 * @tparam Deleter please refer to std::deleter<T>. this class should not throw execption
	 * @param p_retire_obj
	 * @param deleter_arg
	 */
	template <typename T, typename Deleter = std::default_delete<T>>
	static void retire( T* p_retire_obj, Deleter&& deleter_arg = std::default_delete<T> {} )
	{
		if ( internal::global_scope_hazard_ptr_chain::CheckPtrIsHazardPtr( p_retire_obj ) ) {
			retire_node_abst* p_new_retire = new retire_node<T, Deleter> { p_retire_obj, std::forward<Deleter>( deleter_arg ) };
			retire( p_new_retire );
		} else {
			deleter_arg( p_retire_obj );
		}
	}

	static void prune( void );

private:
	static void retire( retire_node_abst* p_new_retire );
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif