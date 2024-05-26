/**
 * @file hazard_ptr_impl.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-12
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCCURRENT_SRC_HAZARD_PTR_IMPL_HPP_
#define ALCONCCURRENT_SRC_HAZARD_PTR_IMPL_HPP_

namespace alpha {
namespace concurrent {
namespace internal {

class alignas( atomic_variable_align ) hazard_ptr_group {
public:
	static constexpr size_t kArraySize = 8;
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
	 * @return std::atomic<void*>* pointer to hazard ptr slot.
	 * @retval nullptr: fail to get hazard pointer slot.
	 * @retval non-nullptr: success to get hazard pointer slot
	 *
	 * @warning
	 * if p is nullptr, this api will be fail and return nullptr.
	 */
	hzrd_slot_ownership_t try_assign( void* p );

	/**
	 * @brief try to get ownership
	 *
	 * @return ownership_t nullptr: fail to get ownership. non nullptr: success to get ownership
	 */
	ownership_t try_ocupy( void );

	bool is_used( void ) const noexcept
	{
		return is_used_.load( std::memory_order_acquire );
	}

	void force_clear( void ) noexcept;

	bool check_pointer_is_hazard_pointer( void* p ) noexcept;
	void scan_hazard_pointers( std::function<void( void* )>& pred );

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
	ALCC_INTERNAL_CPPSTD17_CONSTEXPR iterator begin( void ) noexcept
	{
		return hzrd_ptr_array_.begin();
	}
	ALCC_INTERNAL_CPPSTD17_CONSTEXPR const_iterator begin( void ) const noexcept
	{
		return hzrd_ptr_array_.begin();
	}
	ALCC_INTERNAL_CPPSTD17_CONSTEXPR iterator end( void ) noexcept
	{
		return hzrd_ptr_array_.end();
	}
	ALCC_INTERNAL_CPPSTD17_CONSTEXPR const_iterator end( void ) const noexcept
	{
		return hzrd_ptr_array_.end();
	}

	std::atomic<bool> is_used_;
	alignas( internal::atomic_variable_align ) hzrd_p_array_t hzrd_ptr_array_;
	alignas( internal::atomic_variable_align ) iterator next_assign_hint_it_;
};

/**
 * @brief スレッドとhazard_ptr_listを紐づけるクラス
 *
 * スレッドローカルストレージで使用される前提
 *
 */
class alignas( atomic_variable_align ) bind_hazard_ptr_list {
public:
	constexpr bind_hazard_ptr_list( void )                    = default;
	bind_hazard_ptr_list( bind_hazard_ptr_list&& )            = default;
	bind_hazard_ptr_list& operator=( bind_hazard_ptr_list&& ) = default;
	~bind_hazard_ptr_list();

	/**
	 * @brief assign a slot of hazard pointer and set the pointer
	 *
	 * @param p pointer to a object. should not be nullptr
	 * @return hzrd_slot_ownership_t pointer to hazard pointer slot
	 * @retval nullptr fail to assign or p is nullptr
	 * @retval non-nullptr success to assign
	 */
	hzrd_slot_ownership_t slot_assign( void* p );

private:
	hazard_ptr_group::ownership_t ownership_ticket_;
};

extern thread_local bind_hazard_ptr_list tl_bhpl;

/////////////////////////////////////////////////////////////////
class alignas( internal::atomic_variable_align ) global_scope_hazard_ptr_chain {
public:
	constexpr global_scope_hazard_ptr_chain( void )
	  : ap_top_hzrd_ptr_chain_( nullptr )
	{
	}

	/**
	 * @brief Get ownership from unused hazard_ptr_group list
	 *
	 * @return hazard_ptr_group::ownership_t ownership data. this api does not return nullptr
	 */
	static inline hazard_ptr_group::ownership_t GetOwnership( void );

	/**
	 * @brief Check if p is still in hazard pointer list or not
	 *
	 * @param p
	 * @return true p is still listed in hazard pointer list
	 * @return false p is not hazard pointer
	 */
	bool check_pointer_is_hazard_pointer( void* p ) noexcept;

	/**
	 * @brief Check if p is still in hazard pointer list or not
	 *
	 * @param p
	 * @return true p is still listed in hazard pointer list
	 * @return false p is not hazard pointer
	 */
	void scan_hazard_pointers( std::function<void( void* )>& pred );

	/**
	 * @brief remove all hazard_ptr_group
	 */
	void remove_all( void );

	bool is_empty( void )
	{
		return ap_top_hzrd_ptr_chain_.load( std::memory_order_acquire ) == nullptr;
	}

private:
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

	std::atomic<hazard_ptr_group*> ap_top_hzrd_ptr_chain_;
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
