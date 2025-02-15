/**
 * @file atomic_shared_ptr.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief Atomic shared pointer
 * @version 0.1
 * @date 2025-02-11
 *
 * @copyright Copyright (c) 2025, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ATOMIC_SHARED_PTR_HPP_
#define ATOMIC_SHARED_PTR_HPP_

#include <atomic>
#include <memory>
#include <type_traits>

#include "alconcurrent/experiment/internal/wait_free_stickey_counter.hpp"
#include "alconcurrent/hazard_ptr.hpp"
#include "alconcurrent/internal/cpp_std_configure.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

class control_block_base {
public:
	control_block_base( void )
	  : sc_res_ {}
	  , sc_ctb_ {}
	{
	}
	virtual ~control_block_base() = default;

	virtual void* get_resource_ptr( void ) = 0;
	virtual void  release_resource( void ) = 0;

	bool increment_ref_of_shared( void )
	{
		if ( !sc_ctb_.increment_if_not_zero() ) {
			// this case means already retired this control block
			return false;
		}
		if ( sc_res_.increment_if_not_zero() ) {
			return true;
		}

		if ( sc_ctb_.decrement_then_is_zero() ) {
			// retire( this );   // ここには至らないので、コメントアウトする。
		}
		return false;
	}
	bool increment_ref_of_weak( void )
	{
		return sc_ctb_.increment_if_not_zero();
	}

	bool decrement_ref_of_shared_then_if_zero_release_this( void )
	{
		if ( sc_res_.decrement_then_is_zero() ) {
			release_resource();
		}
		if ( sc_ctb_.decrement_then_is_zero() ) {
			retire( this );
			return true;
		}
		return false;
	}
	bool decrement_ref_of_weak_then_if_zero_release_this( void )
	{
		// NOTE: nts_shared_ptr側が必ずdecrement_ref_of_shared_then_if_zero_release_this()を使用する。
		// という前提条件を守っていること。
		// そのうえで、下記だけで処理が成立する。

		if ( sc_ctb_.decrement_then_is_zero() ) {
			retire( this );
			return true;
		}
		return false;
	}

	bool expired() const noexcept
	{
		return sc_res_.read() == 0;
	}

#ifdef ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
#else
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size );                                     // possible throw std::bad_alloc, from C++11
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size, const std::nothrow_t& ) noexcept;     // possible return nullptr, instead of throwing exception, from C++11
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size );                                   // possible throw std::bad_alloc, from C++11
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size, const std::nothrow_t& ) noexcept;   // possible return nullptr, instead of throwing exception, from C++11

	ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size, void* ptr ) noexcept;     // placement new, from C++11
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size, void* ptr ) noexcept;   // placement new for array, from C++11

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

#if __cpp_aligned_new >= 201606
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size, std::align_val_t alignment );                                     // possible throw std::bad_alloc, from C++17
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept;     // possible return nullptr, instead of throwing exception, from C++17
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size, std::align_val_t alignment );                                   // possible throw std::bad_alloc, from C++17
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept;   // possible return nullptr, instead of throwing exception, from C++17

	void operator delete( void* ptr, std::align_val_t alignment ) noexcept;     // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
	void operator delete[]( void* ptr, std::align_val_t alignment ) noexcept;   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)

	void operator delete( void* ptr, std::size_t size, std::align_val_t alignment ) noexcept;     // from C++17
	void operator delete[]( void* ptr, std::size_t size, std::align_val_t alignment ) noexcept;   // from C++17

	void operator delete( void* ptr, std::align_val_t alignment, const std::nothrow_t& ) noexcept;     // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
	void operator delete[]( void* ptr, std::align_val_t alignment, const std::nothrow_t& ) noexcept;   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)

	void operator delete( void* ptr, std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept;     // from C++17
	void operator delete[]( void* ptr, std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept;   // from C++17
#endif
#endif

private:
	// defered reclamation
	static void retire( control_block_base* p );

	sticky_counter sc_res_;   //!< sticky counter for resource
	sticky_counter sc_ctb_;   //!< sticky counter for my control_block
};

template <typename T, typename Deleter = std::default_delete<T>>
class control_block : public control_block_base {
public:
	control_block( T* p )
	  : control_block_base {}
	  , p_value_ { p }
	  , my_deleter_ {}
	{
	}
	control_block( T* p, Deleter d )
	  : control_block_base {}
	  , p_value_ { p }
	  , my_deleter_ { d }
	{
	}
	~control_block()
	{
		if ( p_value_ == nullptr ) return;
		my_deleter_( p_value_ );
	}

	void* get_resource_ptr( void ) override
	{
		return reinterpret_cast<void*>( p_value_ );
	}

	void release_resource( void ) override
	{
		my_deleter_( p_value_ );
		p_value_ = nullptr;
	}

private:
	T*      p_value_;
	Deleter my_deleter_;
};

template <typename T>
class nts_weak_ptr;

/**
 * @brief Non Thread Safe shared pointer
 *
 * @tparam T
 */
template <typename T>
class nts_shared_ptr {
public:
	using element_type = typename std::remove_extent<T>::type;
	using weak_type    = nts_weak_ptr<T>;

	constexpr nts_shared_ptr( void ) noexcept
	  : p_dataholder_ { nullptr }
	{
	}

	template <typename Y = T, typename std::enable_if<std::is_convertible<Y*, T*>::value>::type* = nullptr>
	explicit nts_shared_ptr( Y* p )
	  : p_dataholder_ { nullptr }
	{
		if ( p == nullptr ) return;

		std::unique_ptr<Y> up_data { p };

		auto p_tmp = new control_block<Y>( p );
		up_data.release();
		p_dataholder_ = p_tmp;
	}
	template <typename Deleter, typename Y = T, typename std::enable_if<std::is_convertible<Y*, T*>::value>::type* = nullptr>
	nts_shared_ptr( Y* p, Deleter d )
	  : p_dataholder_ { nullptr }
	{
		if ( p == nullptr ) return;

		std::unique_ptr<Y, Deleter> up_data { p, d };

		auto p_tmp = new control_block<Y, Deleter>( p, d );
		up_data.release();
		p_dataholder_ = p_tmp;
	}
	template <typename Deleter, typename Y = T, typename std::enable_if<std::is_convertible<Y*, T*>::value>::type* = nullptr>
	explicit nts_shared_ptr( std::unique_ptr<Y, Deleter> up_arg )
	  : p_dataholder_ { nullptr }
	{
		if ( up_arg == nullptr ) return;

		auto p_tmp = new control_block<Y, Deleter>( up_arg.get(), up_arg.get_deleter() );
		up_arg.release();
		p_dataholder_ = p_tmp;
	}

	~nts_shared_ptr()
	{
		if ( p_dataholder_ == nullptr ) {
			return;
		}

		p_dataholder_->decrement_ref_of_shared_then_if_zero_release_this();
	}

	nts_shared_ptr( const nts_shared_ptr& src )
	  : p_dataholder_ { nullptr }
	{
		control_block_base* p_tmp = src.p_dataholder_;
		if ( p_tmp == nullptr ) {
			return;
		}

		if ( !p_tmp->increment_ref_of_shared() ) {
			return;
		}

		p_dataholder_ = p_tmp;
	}

	nts_shared_ptr( nts_shared_ptr&& src )
	  : p_dataholder_ { src.p_dataholder_ }
	{
		src.p_dataholder_ = nullptr;
		return;
	}

	nts_shared_ptr& operator=( const nts_shared_ptr& src )
	{
		if ( p_dataholder_ == src.p_dataholder_ ) return *this;

		nts_shared_ptr( src ).swap( *this );

		return *this;
	}

	nts_shared_ptr& operator=( nts_shared_ptr&& src )
	{
		if ( p_dataholder_ == src.p_dataholder_ ) return *this;

		nts_shared_ptr( std::move( src ) ).swap( *this );

		return *this;
	}

	template <typename Y = T, typename std::enable_if<std::is_convertible<Y*, T*>::value>::type* = nullptr>
	nts_shared_ptr( const nts_shared_ptr<Y>& src ) noexcept
	  : p_dataholder_ { nullptr }
	{
		control_block_base* p_tmp = src.p_dataholder_;
		if ( p_tmp == nullptr ) {
			return;
		}

		if ( !p_tmp->increment_ref_of_shared() ) {
			return;
		}

		p_dataholder_ = p_tmp;
	}
	template <typename Y = T, typename std::enable_if<std::is_convertible<Y*, T*>::value>::type* = nullptr>
	nts_shared_ptr( nts_shared_ptr<Y>&& src ) noexcept
	  : p_dataholder_ { src.p_dataholder_ }
	{
		src.p_dataholder_ = nullptr;
		return;
	}

	template <typename Y, typename std::enable_if<std::is_convertible<Y*, T*>::value>::type* = nullptr>
	nts_shared_ptr& operator=( const nts_shared_ptr<Y>& src )
	{
		if ( p_dataholder_ == src.p_dataholder_ ) return *this;

		nts_shared_ptr( src ).swap( *this );

		return *this;
	}

	template <typename Y = T, typename std::enable_if<std::is_convertible<Y*, T*>::value>::type* = nullptr>
	nts_shared_ptr& operator=( nts_shared_ptr<Y>&& src )
	{
		if ( p_dataholder_ == src.p_dataholder_ ) return *this;

		nts_shared_ptr( std::move( src ) ).swap( *this );

		return *this;
	}

	template <typename Deleter, typename Y = T, typename std::enable_if<std::is_convertible<Y*, T*>::value>::type* = nullptr>
	nts_shared_ptr& operator=( std::unique_ptr<Y, Deleter> up_arg )
	{
		nts_shared_ptr( std::move( up_arg ) ).swap( *this );

		return *this;
	}

	T* get( void ) const noexcept
	{
		if ( p_dataholder_ == nullptr ) return nullptr;

		return reinterpret_cast<T*>( p_dataholder_->get_resource_ptr() );
	}

	void reset() noexcept
	{
		nts_shared_ptr().swap( *this );
	}
	template <typename Y = T, typename std::enable_if<std::is_convertible<Y*, T*>::value>::type* = nullptr>
	void reset( Y* p )
	{
		nts_shared_ptr( p ).swap( *this );
	}
	template <typename Deleter, typename Y = T, typename std::enable_if<std::is_convertible<Y*, T*>::value>::type* = nullptr>
	void reset( Y* p, Deleter d )
	{
		nts_shared_ptr( p, d ).swap( *this );
	}

	void swap( nts_shared_ptr& src ) noexcept
	{
		control_block_base* p_tmp = src.p_dataholder_;
		src.p_dataholder_         = p_dataholder_;
		p_dataholder_             = p_tmp;
	}

	T& operator*() const noexcept
	{
		return *( get() );
	}
	T* operator->() const noexcept
	{
		return get();
	}

	explicit operator bool() const noexcept
	{
		return get() != nullptr;
	}

	template <class U>
	bool owner_before( const nts_shared_ptr<U>& b ) const noexcept
	{
		return p_dataholder_ < b.p_dataholder_;
	}

	template <class U>
	bool owner_before( const nts_weak_ptr<U>& b ) const noexcept;

private:
	control_block_base* p_dataholder_;

	template <class U>
	friend class nts_shared_ptr;
	template <class U>
	friend class nts_weak_ptr;
};

template <typename T>
class nts_weak_ptr {
public:
	~nts_weak_ptr()
	{
		if ( p_dataholder_ == nullptr ) {
			return;
		}

		p_dataholder_->decrement_ref_of_weak_then_if_zero_release_this();
	}

	constexpr nts_weak_ptr( void ) noexcept
	  : p_dataholder_ { nullptr }
	{
	}
	nts_weak_ptr( const nts_weak_ptr& src ) noexcept
	  : p_dataholder_ { nullptr }
	{
		control_block_base* p_tmp = src.p_dataholder_;
		if ( p_tmp == nullptr ) {
			return;
		}

		if ( !p_tmp->increment_ref_of_weak() ) {
			return;
		}

		p_dataholder_ = p_tmp;
	}
	nts_weak_ptr( nts_weak_ptr&& src ) noexcept
	  : p_dataholder_ { src.p_dataholder_ }
	{
		src.p_dataholder_ = nullptr;
	}
	nts_weak_ptr& operator=( const nts_weak_ptr& src ) noexcept
	{
		if ( p_dataholder_ == src.p_dataholder_ ) return *this;

		nts_weak_ptr( src ).swap( *this );
		return *this;
	}
	nts_weak_ptr& operator=( nts_weak_ptr&& src ) noexcept
	{
		if ( p_dataholder_ == src.p_dataholder_ ) return *this;

		nts_weak_ptr( std::move( src ) ).swap( *this );
		return *this;
	}

	template <typename Y = T, typename std::enable_if<std::is_convertible<Y*, T*>::value>::type* = nullptr>
	nts_weak_ptr( const nts_weak_ptr<Y>& src ) noexcept
	  : p_dataholder_ { nullptr }
	{
		control_block_base* p_tmp = src.p_dataholder_;
		if ( p_tmp == nullptr ) {
			return;
		}

		if ( !p_tmp->increment_ref_of_weak() ) {
			return;
		}

		p_dataholder_ = p_tmp;
	}
	template <typename Y = T, typename std::enable_if<std::is_convertible<Y*, T*>::value>::type* = nullptr>
	nts_weak_ptr( nts_weak_ptr<Y>&& src ) noexcept
	  : p_dataholder_ { src.p_dataholder_ }
	{
		src.p_dataholder_ = nullptr;
	}

	template <typename Y = T, typename std::enable_if<std::is_convertible<Y*, T*>::value>::type* = nullptr>
	nts_weak_ptr( const nts_shared_ptr<Y>& src ) noexcept
	  : p_dataholder_ { nullptr }
	{
		control_block_base* p_tmp = src.p_dataholder_;
		if ( p_tmp == nullptr ) {
			return;
		}

		if ( !p_tmp->increment_ref_of_weak() ) {
			return;
		}

		p_dataholder_ = p_tmp;
	}

	template <typename Y = T, typename std::enable_if<std::is_convertible<Y*, T*>::value>::type* = nullptr>
	nts_weak_ptr& operator=( const nts_weak_ptr<Y>& src ) noexcept
	{
		if ( p_dataholder_ == src.p_dataholder_ ) return *this;

		nts_weak_ptr( src ).swap( *this );
		return *this;
	}
	template <typename Y = T, typename std::enable_if<std::is_convertible<Y*, T*>::value>::type* = nullptr>
	nts_weak_ptr& operator=( nts_weak_ptr<Y>&& src ) noexcept
	{
		if ( p_dataholder_ == src.p_dataholder_ ) return *this;

		nts_weak_ptr( std::move( src ) ).swap( *this );
		return *this;
	}
	template <typename Y = T, typename std::enable_if<std::is_convertible<Y*, T*>::value>::type* = nullptr>
	nts_weak_ptr& operator=( const nts_shared_ptr<Y>& src ) noexcept
	{
		if ( p_dataholder_ == src.p_dataholder_ ) return *this;

		nts_weak_ptr( src ).swap( *this );
		return *this;
	}

	void reset() noexcept
	{
		nts_weak_ptr().swap( *this );
	}

	bool expired() const noexcept
	{
		if ( p_dataholder_ == nullptr ) return true;

		return p_dataholder_->expired();
	}

	nts_shared_ptr<T> lock() const noexcept
	{
		nts_shared_ptr<T> ans;
		if ( p_dataholder_ != nullptr ) {
			if ( p_dataholder_->increment_ref_of_shared() ) {
				ans.p_dataholder_ = p_dataholder_;
			}
		}
		return ans;
	}

	void swap( nts_weak_ptr& src ) noexcept
	{
		control_block_base* p_tmp = src.p_dataholder_;
		src.p_dataholder_         = p_dataholder_;
		p_dataholder_             = p_tmp;
	}

	template <class U>
	bool owner_before( const nts_shared_ptr<U>& b ) const noexcept;

	template <class U>
	bool owner_before( const nts_weak_ptr<U>& b ) const noexcept
	{
		return p_dataholder_ < b.p_dataholder_;
	}

private:
	control_block_base* p_dataholder_;

	template <class U>
	friend class nts_shared_ptr;
	template <class U>
	friend class nts_weak_ptr;
};

template <class T>
template <class U>
inline bool nts_shared_ptr<T>::owner_before( const nts_weak_ptr<U>& b ) const noexcept
{
	return p_dataholder_ < b.p_dataholder_;
}

template <class T>
template <class U>
inline bool nts_weak_ptr<T>::owner_before( const nts_shared_ptr<U>& b ) const noexcept
{
	return p_dataholder_ < b.p_dataholder_;
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
