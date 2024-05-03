/**
 * @file hazard_ptr_internal.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-01
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCURRENT_INC_INTERNAL_HAZARD_PTR_INTERNAL_HPP_
#define ALCONCURRENT_INC_INTERNAL_HAZARD_PTR_INTERNAL_HPP_

#include <array>
#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <thread>

#include "alconcurrent/internal/alloc_only_allocator.hpp"
#include "alconcurrent/internal/cpp_std_configure.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

#ifdef __cpp_lib_hardware_interference_size
constexpr size_t atomic_variable_align = std::hardware_destructive_interference_size;   // it is better to be equal to std::hardware_destructive_interference_size
#else
constexpr size_t atomic_variable_align = 64;   // it is better to be equal to std::hardware_destructive_interference_size
#endif

class hazard_ptr_group {
public:
	static constexpr size_t kArraySize = 32;
	using hzrd_p_array_t               = std::array<std::atomic<void*>, kArraySize>;
	using reference                    = hzrd_p_array_t::reference;
	using const_reference              = hzrd_p_array_t::const_reference;
	using iterator                     = hzrd_p_array_t::iterator;
	using const_iterator               = hzrd_p_array_t::const_iterator;
	using reverse_iterator             = hzrd_p_array_t::reverse_iterator;
	using const_reverse_iterator       = hzrd_p_array_t::const_reverse_iterator;
	using size_type                    = hzrd_p_array_t::size_type;
	using difference_type              = hzrd_p_array_t::difference_type;
	using pointer                      = hzrd_p_array_t::pointer;
	using const_pointer                = hzrd_p_array_t::const_pointer;
	using value_type                   = hzrd_p_array_t::value_type;

	class ownership_releaser {
	public:
		void operator()( hazard_ptr_group* ptr ) const
		{
			if ( ptr == nullptr ) return;
			ptr->is_used_.store( false, std::memory_order_release );
		}
	};
	using ownership_t = std::unique_ptr<hazard_ptr_group, ownership_releaser>;

	class hzrd_slot_releaser {
	public:
		void operator()( std::atomic<void*>* ptr ) const
		{
			if ( ptr == nullptr ) return;
			ptr->store( nullptr, std::memory_order_release );
		}
	};
	using hzrd_slot_ownership_t = std::unique_ptr<std::atomic<void*>, hzrd_slot_releaser>;

	ALCC_INTERNAL_CPPSTD17_CONSTEXPR hazard_ptr_group( void )
	  : ap_chain_next_( nullptr )
	  , ap_list_next_( nullptr )
	  , is_used_( false )
	  , hzrd_ptr_array_ {}                                // zero-initialization as nullptr
	  , next_assign_hint_it_( hzrd_ptr_array_.begin() )   // C++17 and after, array::begin is constexpr
	{
	}

	~hazard_ptr_group();

	/**
	 * @brief try to assing a pointer, p, as hazard pointer
	 *
	 * @param p
	 * @return std::atomic<void*>* pointer to hazard ptr slot. nullptr: fail to get hazard pointer slot. non nullptr: success to get hazard pointer slot
	 */
	hzrd_slot_ownership_t try_assign( void* p );

	/**
	 * @brief try to get ownership
	 *
	 * @return ownership_t nullptr: fail to get ownership. non nullptr: success to get ownership
	 */
	ownership_t try_ocupy( void );

	ALCC_INTERNAL_CPPSTD17_CONSTEXPR iterator begin( void )
	{
		return hzrd_ptr_array_.begin();
	}
	ALCC_INTERNAL_CPPSTD17_CONSTEXPR const_iterator begin( void ) const
	{
		return hzrd_ptr_array_.begin();
	}
	ALCC_INTERNAL_CPPSTD17_CONSTEXPR iterator end( void )
	{
		return hzrd_ptr_array_.end();
	}
	ALCC_INTERNAL_CPPSTD17_CONSTEXPR const_iterator end( void ) const
	{
		return hzrd_ptr_array_.end();
	}

#ifdef ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
#else
#if __cpp_aligned_new
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size, std::align_val_t alignment );                                     // possible throw std::bad_alloc, from C++17
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept;     // possible return nullptr, instead of throwing exception, from C++17
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size, std::align_val_t alignment );                                   // possible throw std::bad_alloc, from C++17
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept;   // possible return nullptr, instead of throwing exception, from C++17
#endif
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size );                                     // possible throw std::bad_alloc, from C++11
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size, const std::nothrow_t& ) noexcept;     // possible return nullptr, instead of throwing exception, from C++11
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size );                                   // possible throw std::bad_alloc, from C++11
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size, const std::nothrow_t& ) noexcept;   // possible return nullptr, instead of throwing exception, from C++11

	ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size, void* ptr ) noexcept;     // placement new, from C++11
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size, void* ptr ) noexcept;   // placement new for array, from C++11

#if __cpp_aligned_new
	void operator delete( void* ptr, std::align_val_t alignment ) noexcept;     // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
	void operator delete[]( void* ptr, std::align_val_t alignment ) noexcept;   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)

	void operator delete( void* ptr, std::size_t size, std::align_val_t alignment ) noexcept;     // from C++17
	void operator delete[]( void* ptr, std::size_t size, std::align_val_t alignment ) noexcept;   // from C++17

	void operator delete( void* ptr, std::align_val_t alignment, const std::nothrow_t& ) noexcept;     // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
	void operator delete[]( void* ptr, std::align_val_t alignment, const std::nothrow_t& ) noexcept;   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)

	void operator delete( void* ptr, std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept;     // from C++17
	void operator delete[]( void* ptr, std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept;   // from C++17
#endif
	void operator delete( void* ptr ) noexcept;     // from C++11
	void operator delete[]( void* ptr ) noexcept;   // from C++11

	void operator delete( void* ptr, std::size_t size ) noexcept;     // from C++14
	void operator delete[]( void* ptr, std::size_t size ) noexcept;   // from C++14

	void operator delete( void* ptr, const std::nothrow_t& ) noexcept;     // from C++11
	void operator delete[]( void* ptr, const std::nothrow_t& ) noexcept;   // from C++11

	void operator delete( void* ptr, std::size_t size, const std::nothrow_t& ) noexcept;     // from C++14
	void operator delete[]( void* ptr, std::size_t size, const std::nothrow_t& ) noexcept;   // from C++14

	void operator delete( void* ptr, void* ) noexcept;     // delete for area that is initialized by placement new.
	void operator delete[]( void* ptr, void* ) noexcept;   // delete for area that is initialized by placement new.
#endif

	std::atomic<hazard_ptr_group*> ap_chain_next_;
	std::atomic<hazard_ptr_group*> ap_list_next_;

#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
	static std::atomic<size_t> call_count_try_assign_;
	static std::atomic<size_t> loop_count_in_try_assign_;
#endif

private:
	std::atomic<bool> is_used_;
	hzrd_p_array_t    hzrd_ptr_array_;
	iterator          next_assign_hint_it_;
};

class bind_hazard_ptr_list {
public:
	using hzrd_slot_ownership_t = hazard_ptr_group::hzrd_slot_ownership_t;

	constexpr bind_hazard_ptr_list( void )                    = default;
	bind_hazard_ptr_list( bind_hazard_ptr_list&& )            = default;
	bind_hazard_ptr_list& operator=( bind_hazard_ptr_list&& ) = default;
	~bind_hazard_ptr_list();

	hzrd_slot_ownership_t assign( void* p );

private:
	hazard_ptr_group::ownership_t ownership_ticket_;
};

extern thread_local bind_hazard_ptr_list tl_bhpl;

/////////////////////////////////////////////////////////////////
class global_scope_hazard_ptr_chain {
public:
	/**
	 * @brief Get ownership from unused hazard_ptr_group list
	 *
	 * @return hazard_ptr_group::ownership_t ownership data. this api does not return nullptr
	 */
	static inline hazard_ptr_group::ownership_t GetOwnership( void )
	{
		return g_scope_hzrd_chain_.get_ownership();
	}

	/**
	 * @brief Check if p is still in hazard pointer list or not
	 *
	 * @param p
	 * @return true p is still listed in hazard pointer list
	 * @return false p is not hazard pointer
	 */
	static inline bool CheckPtrIsHazardPtr( void* p )
	{
		return g_scope_hzrd_chain_.check_pointer_is_hazard_pointer( p );
	}

	/**
	 * @brief remove all hazard_ptr_group from internal global variable
	 *
	 * This API is for debug and test purpose.
	 *
	 * @pre this API should be called from main thread. And, all other threads should be exited before call this API.
	 */
	static inline void DestoryAll( void )
	{
		return g_scope_hzrd_chain_.remove_all();
	}

	/**
	 * @brief check empty or not
	 *
	 * @warning this API has race condition. Therefore, this API is test purpose only.
	 *
	 * @return true hazard pointer related resouce is Empty
	 * @return false hazard pointer related resouce is Not Empty
	 *
	 * @todo introduce exclusive control by mutex b/w DestroyAll() and IsDestoryed()
	 */
	static inline bool IsDestoryed( void )
	{
		return g_scope_hzrd_chain_.ap_top_hzrd_ptr_chain_.load( std::memory_order_acquire ) == nullptr;
	}

private:
	constexpr global_scope_hazard_ptr_chain( void )
	  : ap_top_hzrd_ptr_chain_( nullptr )
	{
	}

	/**
	 * @brief try to get ownership from unused hazard_ptr_group list
	 *
	 * @return hazard_ptr_group::ownership_t non nullptr: success to get ownership. nullptr: fail to get ownership
	 */
	hazard_ptr_group::ownership_t try_get_ownership( void );

	/**
	 * @brief register new hazard_ptr_group list
	 *
	 * @param p_hpg_arg
	 */
	void register_new_hazard_ptr_group( hazard_ptr_group* p_hpg_arg );

	/**
	 * @brief get ownership from unused hazard_ptr_group list
	 *
	 * @return hazard_ptr_group::ownership_t ownership data. this api does not return nullptr
	 */
	hazard_ptr_group::ownership_t get_ownership( void );

	/**
	 * @brief Check if p is still in hazard pointer list or not
	 *
	 * @param p
	 * @return true p is still listed in hazard pointer list
	 * @return false p is not hazard pointer
	 */
	bool check_pointer_is_hazard_pointer( void* p );

	/**
	 * @brief remove all hazard_ptr_group
	 */
	void remove_all( void );

	std::atomic<hazard_ptr_group*> ap_top_hzrd_ptr_chain_;

	static global_scope_hazard_ptr_chain g_scope_hzrd_chain_;
};

///////////////////////////////////////////////////////////////////////////////////////////////
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
};

/**
 * @brief retire管理用I/Fクラス（Facadeクラス）
 *
 * @todo シングルトン化すべきか？
 */
class retire_mgr {
public:
	template <typename T, typename Deleter = std::default_delete<T>>
	static void retire( T* p_retire_obj, Deleter&& deleter_arg = std::default_delete<T> {} )
	{
		if ( internal::global_scope_hazard_ptr_chain::CheckPtrIsHazardPtr( p_retire_obj ) ) {
			retire_node_abst* p_new_retire = new retire_node<T, Deleter>( p_retire_obj, std::forward<Deleter>( deleter_arg ) );
			retire( p_new_retire );
		} else {
			deleter_arg( p_retire_obj );
		}
	}

	static void recycle( void );

private:
	static void retire( retire_node_abst* p_new_retire );
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
