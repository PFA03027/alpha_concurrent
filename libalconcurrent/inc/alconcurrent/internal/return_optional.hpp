/**
 * @file return_optional.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-08
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCURRENT_INC_INTERNAL_RETURN_OPTIONAL_HPP_
#define ALCONCURRENT_INC_INTERNAL_RETURN_OPTIONAL_HPP_

#if __has_include( <version>)
#include <version>
#endif

#if __has_include( <optional>)
#include <optional>
#endif

#include <exception>
#include <type_traits>

#include "cpp_std_configure.hpp"

namespace alpha {
namespace concurrent {

// #if __cpp_lib_optional >= 201606
#if 0

using return_nullopt_t                           = std::nullopt_t;
inline constexpr return_nullopt_t return_nullopt = std::nullopt;
using return_in_place_t                          = std::in_place_t;
inline constexpr return_in_place_t return_in_place;

template <typename T>
using return_optional = std::optional<T>;

using bad_return_optional_access = std::bad_optional_access;

#else

struct return_nullopt_t {};
inline constexpr return_nullopt_t return_nullopt;

struct return_in_place_t {
	explicit return_in_place_t() = default;
};

inline constexpr return_in_place_t return_in_place {};

class bad_return_optional_access : public std::exception {};

namespace internal {

template <typename T>
struct remove_cvref {
	using type = typename std::remove_reference<typename std::remove_cv<T>::type>::type;
};

template <typename T>
union storage_t {
	char dummy_;
	T    value_;

	constexpr storage_t( void )
	  : dummy_()
	{
	}

	template <typename... Args>
	constexpr storage_t( return_in_place_t, Args&&... args )
	  : value_( std::forward<Args>( args )... )
	{
	}

	~storage_t() {}
};

template <typename T>
union constexpr_storage_t {
	char dummy_;
	T    value_;

	constexpr constexpr_storage_t( void )
	  : dummy_()
	{
	}

	template <typename... Args>
	constexpr constexpr_storage_t( return_in_place_t, Args&&... args )
	  : value_( std::forward<Args>( args )... )
	{
	}

	~constexpr_storage_t() = default;
};

template <typename T>
struct optional_base {
	bool         init_;
	storage_t<T> storage_;

	constexpr optional_base( void )
	  : init_( false )
	  , storage_()
	{
	}
	template <typename... Args>
	constexpr optional_base( return_in_place_t, Args&&... args )
	  : init_( true )
	  , storage_( return_in_place, std::forward<Args>( args )... )
	{
	}

	~optional_base()
	{
		if ( init_ ) {
			storage_.value_.~T();
		}
	}
};

template <typename T>
struct constexpr_optional_base {
	bool                   init_;
	constexpr_storage_t<T> storage_;

	constexpr constexpr_optional_base( void )
	  : init_( false )
	  , storage_()
	{
	}
	template <typename... Args>
	constexpr constexpr_optional_base( return_in_place_t, Args&&... args )
	  : init_( true )
	  , storage_( return_in_place, std::forward<Args>( args )... )
	{
	}

	~constexpr_optional_base() = default;
};

template <typename T>
using OptionalBase = std::conditional_t<std::is_trivially_destructible<T>::value, constexpr_optional_base<T>, optional_base<T>>;

}   // namespace internal

template <typename T>
class return_optional;

template <typename T>
class return_optional : private internal::OptionalBase<T> {
public:
	template <typename U>
	friend class return_optional;

	constexpr return_optional( void ) noexcept
	  : internal::OptionalBase<T>()
	{
	}

	constexpr return_optional( return_nullopt_t ) noexcept
	  : return_optional()
	{
	}

	template <typename... Args>
	constexpr return_optional( return_in_place_t, Args&&... args )
	  : internal::OptionalBase<T>( return_in_place, std::forward<Args>( args )... )
	{
	}

	template <typename U = T, std::enable_if<std::is_constructible<T, U>::value>::type* = nullptr>
	constexpr return_optional( U&& value )
	  : internal::OptionalBase<T>( return_in_place, std::forward<U>( value ) )
	{
	}

	template <typename U = T, std::enable_if<std::is_constructible<T, U>::value || std::is_constructible<T, U&>::value>::type* = nullptr>
	constexpr return_optional( const return_optional<U>& src )
	  : internal::OptionalBase<T>()
	{
		if ( src.init_ ) {
			new ( &( storage_.value_ ) ) T( src.storage_.value_ );
			init_ = true;
		}
	}

	template <typename U = T, std::enable_if<std::is_constructible<T, U&&>::value>::type* = nullptr>
	constexpr return_optional( return_optional<U>&& src )
	  : internal::OptionalBase<T>()
	{
		if ( src.init_ ) {
			new ( &( storage_.value_ ) ) T( std::move( src.storage_.value_ ) );
			init_ = true;
		}
	}

	return_optional& operator=( return_nullopt_t )
	{
		reset();
		return *this;
	}

	template <bool IsCopyConstructible = std::is_copy_constructible<T>::value, bool IsCopyAssignable = std::is_copy_assignable<T>::value, std::enable_if<IsCopyConstructible && IsCopyAssignable>::type* = nullptr>
	return_optional& operator=( const return_optional& src )
	{
		if ( this == &src ) return *this;

		if ( init_ && src.init_ ) {
			storage_.value_ = src.storage_.value_;
		} else if ( init_ && ( !src.init_ ) ) {
			storage_.value_.~T();
			init_ = src.init_;
		} else if ( ( !init_ ) && src.init_ ) {
			new ( &( storage_.value_ ) ) T( src.storage_.value_ );
			init_ = src.init_;
		} else {
			// nothing to do
		}

		return *this;
	}

	template <bool IsMoveConstructible = std::is_move_constructible<T>::value, bool IsMoveAssignable = std::is_move_assignable<T>::value, std::enable_if<IsMoveConstructible && IsMoveAssignable>::type* = nullptr>
	return_optional& operator=( return_optional&& src )
	{
		if ( this == &src ) return *this;

		if ( init_ && src.init_ ) {
			storage_.value_ = std::move( src.storage_.value_ );
		} else if ( init_ && ( !src.init_ ) ) {
			storage_.value_.~T();
			init_ = src.init_;
		} else if ( ( !init_ ) && src.init_ ) {
			new ( &( storage_.value_ ) ) T( std::move( src.storage_.value_ ) );
			init_ = src.init_;
		} else {
			// nothing to do
		}

		return *this;
	}

	template <typename U = T, std::enable_if<std::is_convertible<U, T>::value>::type* = nullptr>
	return_optional& operator=( U&& src )
	{
		if ( init_ ) {
			storage_.value_ = std::forward<U>( src );
		} else {
			new ( &( storage_.value_ ) ) T( std::forward<U>( src ) );
			init_ = true;
		}

		return *this;
	}

	template <typename U, std::enable_if<std::is_convertible<U, T>::value && !std::is_same<U, T>::value>::type* = nullptr>
	return_optional& operator=( const return_optional<U>& src )
	{
		if ( this == &src ) return *this;

		if ( init_ && src.init_ ) {
			storage_.value_ = src.storage_.value_;
		} else if ( init_ && ( !src.init_ ) ) {
			storage_.value_.~T();
			init_ = src.init_;
		} else if ( ( !init_ ) && src.init_ ) {
			new ( &( storage_.value_ ) ) T( src.storage_.value_ );
			init_ = src.init_;
		} else {
			// nothing to do
		}

		return *this;
	}

	template <typename U, std::enable_if<std::is_convertible<U, T>::value && !std::is_same<U, T>::value>::type* = nullptr>
	return_optional& operator=( return_optional<U>&& src )
	{
		if ( this == &src ) return *this;

		if ( init_ && src.init_ ) {
			storage_.value_ = std::move( src.storage_.value_ );
		} else if ( init_ && ( !src.init_ ) ) {
			storage_.value_.~T();
			init_ = src.init_;
		} else if ( ( !init_ ) && src.init_ ) {
			new ( &( storage_.value_ ) ) T( std::move( src.storage_.value_ ) );
			init_ = src.init_;
		} else {
			// nothing to do
		}

		return *this;
	}

	template <class... Args, std::enable_if<std::is_constructible<T, Args...>::value>::type* = nullptr>
	constexpr T& emplace( Args&&... args )
	{
		reset();

		new ( &( storage_.value_ ) ) T( std::forward<Args>( args )... );
		init_ = true;

		return storage_.value_;
	}

	constexpr void swap( return_optional& src ) noexcept( std::is_nothrow_swappable<T>::value && std::is_nothrow_constructible<T>::value )
	{
		if ( this == &src ) return;

		if ( init_ && src.init_ ) {
			std::swap( storage_.value_, src.storage_.value_ );
		} else if ( init_ && ( !src.init_ ) ) {
			new ( &( src.storage_.value_ ) ) T( std::move( storage_.value_ ) );
			src.init_ = true;
			reset();
		} else if ( ( !init_ ) && src.init_ ) {
			new ( &( storage_.value_ ) ) T( std::move( src.storage_.value_ ) );
			init_ = true;
			src.reset();
		} else {
			// nothing to do
		}
	}

	constexpr void reset() noexcept
	{
		if ( init_ ) {
			storage_.value_.~T();
			init_ = false;
		}
	}

	constexpr T& operator*() &
	{
		return storage_.value_;
	}   // (1)
	constexpr T&& operator*() &&
	{
		return storage_.value_;
	}   // (2)
	constexpr const T& operator*() const&
	{
		return storage_.value_;
	}   // (3)
	constexpr const T&& operator*() const&&
	{
		return storage_.value_;
	}   // (4)

	constexpr const T* operator->() const
	{
		return &storage_.value_;
	}
	constexpr T* operator->()
	{
		return &storage_.value_;
	}

	constexpr explicit operator bool() const noexcept
	{
		return init_;
	}

	constexpr bool has_value() const noexcept
	{
		return init_;
	}

	constexpr const T& value() const&
	{
		if ( !init_ ) {
			throw bad_return_optional_access();
		}

		return storage_.value_;
	}   // (1)
	constexpr T& value() &
	{
		if ( !init_ ) {
			throw bad_return_optional_access();
		}

		return storage_.value_;
	}   // (2)
	constexpr T&& value() &&
	{
		if ( !init_ ) {
			throw bad_return_optional_access();
		}

		return storage_.value_;
	}   // (3)
	constexpr const T&& value() const&&
	{
		if ( !init_ ) {
			throw bad_return_optional_access();
		}

		return storage_.value_;
	}   // (4)

	template <class U, std::enable_if<std::is_convertible<U, T>::value>::type* = nullptr>
	constexpr T value_or( U&& v ) const&
	{
		return has_value() ? value() : static_cast<T>( std::forward<U>( v ) );
	}   // (1)
	template <class U, std::enable_if<std::is_convertible<U, T>::value>::type* = nullptr>
	constexpr T value_or( U&& v ) &&
	{
		return has_value() ? value() : static_cast<T>( std::forward<U>( v ) );
	}   // (2)

private:
	using internal::OptionalBase<T>::init_;
	using internal::OptionalBase<T>::storage_;
};

#endif

}   // namespace concurrent
}   // namespace alpha

#endif
