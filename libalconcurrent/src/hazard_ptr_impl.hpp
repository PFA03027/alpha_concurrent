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

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

#include "alconcurrent/internal/cpp_std_configure.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

static inline std::uintptr_t clr_del_mark_from_addr( std::uintptr_t addr_hpg_arg )
{
	return addr_hpg_arg & ( ~( static_cast<std::uintptr_t>( 1U ) ) );
}
template <typename T>
static inline T* get_pointer_from_addr_clr_marker( std::uintptr_t addr_hpg_arg )
{
	return reinterpret_cast<T*>( clr_del_mark_from_addr( addr_hpg_arg ) );
}
template <typename T>
static inline std::uintptr_t get_addr_from_pointer( T* const p_hpg_arg )
{
	return reinterpret_cast<std::uintptr_t>( p_hpg_arg );
}
static inline std::uintptr_t set_del_mark_to_addr( std::uintptr_t addr_hpg_arg )
{
	return addr_hpg_arg | static_cast<std::uintptr_t>( 1U );
}
static inline bool is_del_marked( std::uintptr_t addr_hpg_arg )
{
	return ( ( addr_hpg_arg & ( static_cast<std::uintptr_t>( 1U ) ) ) != 0 );
}

class alignas( atomic_variable_align ) del_markable_pointer {
public:
	class writer_accesser {
	public:
		writer_accesser( void )
		  : p_target_( nullptr )
		{
		}
		writer_accesser( const writer_accesser& src )
		  : p_target_( src.p_target_ )
		{
			p_target_->writer_accesser_cnt_++;
		}
		writer_accesser( writer_accesser&& src )
		  : p_target_( src.p_target_ )
		{
			src.p_target_ = nullptr;
		}
		writer_accesser& operator=( const writer_accesser& src )
		{
			if ( this != &src ) {
				if ( p_target_ != src.p_target_ ) {
					if ( p_target_ != nullptr ) {
						p_target_->writer_accesser_cnt_--;
					}
					p_target_ = src.p_target_;
					if ( p_target_ != nullptr ) {
						p_target_->writer_accesser_cnt_++;
					}
				}
			}
			return *this;
		}
		writer_accesser& operator=( writer_accesser&& src )
		{
			if ( this != &src ) {
				if ( p_target_ != src.p_target_ ) {
					if ( p_target_ != nullptr ) {
						p_target_->writer_accesser_cnt_--;
					}
					p_target_     = src.p_target_;
					src.p_target_ = nullptr;
				}
			}
			return *this;
		}
		~writer_accesser()
		{
			if ( p_target_ == nullptr ) return;

			p_target_->writer_accesser_cnt_--;
		}

		void set_del_mark( void )
		{
			std::uintptr_t expect_addr;
			std::uintptr_t del_marked_addr;

			do {
				expect_addr     = p_target_->aaddr_next_.load( std::memory_order_acquire );
				del_marked_addr = set_del_mark_to_addr( expect_addr );
			} while ( !p_target_->aaddr_next_.compare_exchange_weak( expect_addr, del_marked_addr, std::memory_order_release, std::memory_order_acquire ) );
		}

		void store_address( std::uintptr_t addr )
		{
			p_target_->aaddr_next_.store( addr, std::memory_order_release );
		}

		template <typename T>
		void store_pointer( T* p )
		{
			p_target_->aaddr_next_.store( get_addr_from_pointer( p ), std::memory_order_release );
		}

		std::uintptr_t load_address( void )
		{
			return p_target_->aaddr_next_.load( std::memory_order_acquire );
		}

		template <typename T>
		T* load_pointer( void )
		{
			std::uintptr_t addr = p_target_->aaddr_next_.load( std::memory_order_acquire );
			return get_pointer_from_addr_clr_marker<T>( addr );
		}

		template <typename T>
		bool compare_exchange_weak( std::uintptr_t& addr_expect, T* p_desire )
		{
			return p_target_->aaddr_next_.compare_exchange_weak( addr_expect, get_addr_from_pointer( p_desire ), std::memory_order_release, std::memory_order_acquire );
		}

		template <typename T>
		bool compare_exchange_string( std::uintptr_t& addr_expect, T* p_desire )
		{
			return p_target_->aaddr_next_.compare_exchange_strong( addr_expect, get_addr_from_pointer( p_desire ), std::memory_order_release, std::memory_order_acquire );
		}

	private:
		writer_accesser( del_markable_pointer* p_target_arg )
		  : p_target_( p_target_arg )
		{
			p_target_->writer_accesser_cnt_++;
		}

		del_markable_pointer* p_target_;

		friend class del_markable_pointer;
		friend class writer_twin_accessor;
	};

	template <typename T>
	class writer_twin_accessor {
	public:
		writer_twin_accessor( std::atomic<std::uintptr_t>* p_pre_addr_storage_arg )
		  : p_pre_addr_storage_( p_pre_addr_storage_arg )
		  , writer_pre_()
		  , p_focus_( nullptr )
		  , writer_cur_()
		{
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
			if ( p_pre_addr_storage_arg == nullptr ) {
				throw std::logic_error( "p_pre_addr_storage_arg should not be nullptr" );
			}
#endif
			setup_cur();
		}
		writer_twin_accessor( writer_accesser&& writer_pre_arg )
		  : p_pre_addr_storage_( nullptr )
		  , writer_pre_( std::move( writer_pre_arg ) )
		  , p_focus_( nullptr )
		  , writer_cur_()
		{
			if ( writer_pre_.p_target_ == nullptr ) return;

			p_pre_addr_storage_ = &( writer_pre_.p_target_->aaddr_next_ );
			setup_cur();
		}

		void shift( void )
		{
			writer_pre_ = std::move( writer_cur_ );
			if ( writer_pre_.p_target_ == nullptr ) {
				p_pre_addr_storage_ = nullptr;
				return;
			}

			p_pre_addr_storage_ = &( writer_pre_.p_target_->aaddr_next_ );
			setup_cur();
		}

		void re_setup( void )
		{
			setup_cur();
		}

		T* get_pointer_to_cur( void ) const
		{
			return p_focus_;
		}

		std::uintptr_t load_nxt_address_of_cur( void )
		{
			return writer_cur_.load_address();
		}

		bool pre_address_compare_exchange_weak( std::uintptr_t& expect_addr, T* p_desire )
		{
			return p_pre_addr_storage_->compare_exchange_weak( expect_addr, get_addr_from_pointer( p_desire ), std::memory_order_release, std::memory_order_acquire );
		}

		std::atomic<std::uintptr_t>* get_p_link_addr_pre( void )
		{
			return p_pre_addr_storage_;
		}

	private:
		void setup_cur( void )
		{
			std::uintptr_t addr  = p_pre_addr_storage_->load( std::memory_order_acquire );
			T*             p_tmp = get_pointer_from_addr_clr_marker<T>( addr );
			do {
				p_focus_ = p_tmp;
				if ( p_focus_ == nullptr ) return;
				writer_cur_ = p_focus_->get_valid_chain_next_writer_accesser();

				addr  = p_pre_addr_storage_->load( std::memory_order_acquire );
				p_tmp = get_pointer_from_addr_clr_marker<T>( addr );
			} while ( p_focus_ != p_tmp );
		}

		std::atomic<std::uintptr_t>* p_pre_addr_storage_;
		writer_accesser              writer_pre_;

		T*              p_focus_;
		writer_accesser writer_cur_;
	};

	class reader_accesser {
	public:
		reader_accesser( void )
		  : p_target_( nullptr )
		{
		}
		reader_accesser( const reader_accesser& src )
		  : p_target_( src.p_target_ )
		{
		}
		reader_accesser( reader_accesser&& src )
		  : p_target_( src.p_target_ )
		{
			src.p_target_ = nullptr;
		}
		reader_accesser& operator=( const reader_accesser& src )
		{
			if ( this != &src ) {
				p_target_ = src.p_target_;
			}
			return *this;
		}
		reader_accesser& operator=( reader_accesser&& src )
		{
			if ( this != &src ) {
				p_target_     = src.p_target_;
				src.p_target_ = nullptr;
			}
			return *this;
		}
		~reader_accesser()
		{
			if ( p_target_ == nullptr ) return;
		}

		std::uintptr_t load_address( void )
		{
			return p_target_->aaddr_next_.load( std::memory_order_acquire );
		}

		template <typename T>
		T* load_pointer( void )
		{
			std::uintptr_t addr = p_target_->aaddr_next_.load( std::memory_order_acquire );
			return get_pointer_from_addr_clr_marker<T>( addr );
		}

	private:
		reader_accesser( del_markable_pointer* p_target_arg )
		  : p_target_( p_target_arg )
		{
		}

		del_markable_pointer* p_target_;

		friend class del_markable_pointer;
	};

	constexpr del_markable_pointer( std::uintptr_t init_addr )
	  : aaddr_next_( init_addr )
	  , writer_accesser_cnt_( 0 )
	{
	}
	del_markable_pointer( const del_markable_pointer& src )
	  : aaddr_next_( src.aaddr_next_.load( std::memory_order_acquire ) )
	  , writer_accesser_cnt_( 0 )
	{
	}
	del_markable_pointer( del_markable_pointer&& src )
	  : aaddr_next_( src.aaddr_next_.load( std::memory_order_acquire ) )
	  , writer_accesser_cnt_( 0 )
	{
	}

	reader_accesser get_reader_accesser( void )
	{
		return reader_accesser( this );
	}
	writer_accesser get_writer_accesser( void )
	{
		return writer_accesser( this );
	}
	bool is_any_accesser( void )
	{
		int acr_cnt = writer_accesser_cnt_.load( std::memory_order_acquire );
#if defined( ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR ) || defined( ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION )
		if ( acr_cnt < 0 ) {
			internal::LogOutput( log_type::ERR, "is_any_deleting_accesser found unexpected count value. this is logic error. this value should be 0 <= count and count <= max threads in a process. deleting_accesser_cnt_ = %u", acr_cnt );
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION
			std::terminate();
#endif
		}
#endif
		return ( acr_cnt > 0 );
	}

private:
	alignas( atomic_variable_align ) std::atomic<std::uintptr_t> aaddr_next_;
	alignas( atomic_variable_align ) std::atomic<int> writer_accesser_cnt_;   // instead of hazard pointer
};

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
			ptr->is_using_.store( false, std::memory_order_release );
		}
	};
	using ownership_t = std::unique_ptr<hazard_ptr_group, ownership_releaser>;

	ALCC_INTERNAL_CPPSTD17_CONSTEXPR hazard_ptr_group( void )
	  : ap_chain_next_( nullptr )
	  , ap_list_next_( nullptr )
	  , delmarkable_valid_chain_next_( 1 )
	  , is_using_( false )
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
		return is_using_.load( std::memory_order_acquire );
	}

	void force_clear( void ) noexcept;

	bool check_pointer_is_hazard_pointer( void* p ) noexcept;
	void scan_hazard_pointers( std::function<void( void* )>& pred );

	del_markable_pointer::writer_accesser get_valid_chain_next_writer_accesser( void )
	{
		return delmarkable_valid_chain_next_.get_writer_accesser();
	}
	del_markable_pointer::reader_accesser get_valid_chain_next_reader_accesser( void )
	{
		return delmarkable_valid_chain_next_.get_reader_accesser();
	}
	bool is_any_deleting_accesser( void )
	{
		return delmarkable_valid_chain_next_.is_any_accesser();
	}

	static void push_front_hazard_ptr_group_to_valid_chain( hazard_ptr_group* const p_hpg_arg, std::atomic<std::uintptr_t>* const p_addr_top_valid_hpg_chain );
	static void remove_hazard_ptr_group_from_valid_chain( hazard_ptr_group* const p_hpg_arg, std::atomic<std::uintptr_t>* const p_addr_top_valid_hpg_chain );
	static bool is_hazard_ptr_group_in_valid_chain( hazard_ptr_group* const p_hpg_arg, std::atomic<std::uintptr_t>* const p_addr_top_valid_hpg_chain );

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

	del_markable_pointer delmarkable_valid_chain_next_;
	alignas( atomic_variable_align ) std::atomic<bool> is_using_;
	alignas( atomic_variable_align ) hzrd_p_array_t hzrd_ptr_array_;
	alignas( atomic_variable_align ) iterator next_assign_hint_it_;
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
class alignas( atomic_variable_align ) global_scope_hazard_ptr_chain {
public:
	constexpr global_scope_hazard_ptr_chain( void )
	  : ap_top_hzrd_ptr_chain_( nullptr )
	  , aaddr_top_hzrd_ptr_valid_chain_( 0 )
	{
	}

	/**
	 * @brief Get ownership from unused hazard_ptr_group list
	 *
	 * @return hazard_ptr_group::ownership_t ownership data. this api does not return nullptr
	 */
	static hazard_ptr_group::ownership_t GetOwnership( void );

	/**
	 * @brief Release ownership of hazard_ptr_group and return it to hazard_ptr_group list
	 *
	 * @return hazard_ptr_group::ownership_t ownership data. this api does not return nullptr
	 */
	static void ReleaseOwnership( hazard_ptr_group::ownership_t os );

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
	 * @brief release ownership that is gottne by try_get_ownership()
	 *
	 * @param up_o ownership
	 */
	void release_ownership( hazard_ptr_group::ownership_t up_o );

	/**
	 * @brief register new hazard_ptr_group list
	 *
	 * @param p_hpg_arg
	 */
	void register_new_hazard_ptr_group( hazard_ptr_group* p_hpg_arg );

	/**
	 * @brief register hazard_ptr_group to valid list
	 *
	 * @pre p_hpg_arg that has registered by register_new_hazard_ptr_group()
	 *
	 * @param p_hpg_arg pointer to hazard_ptr_group that has registered by  register_new_hazard_ptr_group()
	 */
	void register_hazard_ptr_group_to_valid_list( hazard_ptr_group* p_hpg_arg );

	/**
	 * @brief get ownership from unused hazard_ptr_group list
	 *
	 * @return hazard_ptr_group::ownership_t ownership data. this api does not return nullptr
	 */
	hazard_ptr_group::ownership_t get_ownership( void );

	std::atomic<hazard_ptr_group*> ap_top_hzrd_ptr_chain_;
	std::atomic<std::uintptr_t>    aaddr_top_hzrd_ptr_valid_chain_;
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
