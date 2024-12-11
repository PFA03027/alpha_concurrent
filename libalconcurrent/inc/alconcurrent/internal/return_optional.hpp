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

#if __has_include( <compare>)
#include <compare>
#endif

#include <exception>
#include <initializer_list>
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

	template <bool IsCopyConstructible                                                            = std::is_copy_constructible<T>::value,
	          bool IsTriviallyCopyConstructible                                                   = std::is_trivially_copy_constructible<T>::value,
	          typename std::enable_if<IsCopyConstructible && IsTriviallyCopyConstructible>::type* = nullptr>
	constexpr return_optional( const return_optional& src )
	  : internal::OptionalBase<T>()
	{
		if ( src.init_ ) {
			new ( &( storage_.value_ ) ) T( src.storage_.value_ );
			init_ = true;
		}
	}
	template <bool IsCopyConstructible                                                             = std::is_copy_constructible<T>::value,
	          bool IsTriviallyCopyConstructible                                                    = std::is_trivially_copy_constructible<T>::value,
	          typename std::enable_if<IsCopyConstructible && !IsTriviallyCopyConstructible>::type* = nullptr>
	return_optional( const return_optional& src )
	  : internal::OptionalBase<T>()
	{
		if ( src.init_ ) {
			new ( &( storage_.value_ ) ) T( src.storage_.value_ );
			init_ = true;
		}
	}

	template <bool IsMoveConstructible                                                            = std::is_move_constructible<T>::value,
	          bool IsTriviallyMoveConstructible                                                   = std::is_trivially_move_constructible<T>::value,
	          typename std::enable_if<IsMoveConstructible && IsTriviallyMoveConstructible>::type* = nullptr>
	constexpr return_optional( return_optional&& src ) noexcept( std::is_nothrow_move_constructible<T>::value )
	  : internal::OptionalBase<T>()
	{
		if ( src.init_ ) {
			new ( &( storage_.value_ ) ) T( std::move( src.storage_.value_ ) );
			init_ = true;
		}
	}

	template <bool IsMoveConstructible                                                             = std::is_move_constructible<T>::value,
	          bool IsTriviallyMoveConstructible                                                    = std::is_trivially_move_constructible<T>::value,
	          typename std::enable_if<IsMoveConstructible && !IsTriviallyMoveConstructible>::type* = nullptr>
	return_optional( return_optional&& src ) noexcept( std::is_nothrow_move_constructible<T>::value )
	  : internal::OptionalBase<T>()
	{
		if ( src.init_ ) {
			new ( &( storage_.value_ ) ) T( std::move( src.storage_.value_ ) );
			init_ = true;
		}
	}

	template <typename... Args,
	          typename std::enable_if<std::is_constructible<T, Args...>::value>::type* = nullptr>
	constexpr explicit return_optional( return_in_place_t, Args&&... args )
	  : internal::OptionalBase<T>( return_in_place, std::forward<Args>( args )... )
	{
	}
	template <typename U, typename... Args,
	          typename std::enable_if<std::is_constructible<T, std::initializer_list<U>&, Args...>::value>::type* = nullptr>
	constexpr explicit return_optional( return_in_place_t, std::initializer_list<U>& il, Args&&... args )
	  : internal::OptionalBase<T>( return_in_place, il, std::forward<Args>( args )... )
	{
	}

	template <typename U                                                                                                      = T,
	          typename std::enable_if<std::is_constructible<T, U>::value &&
	                                  !std::is_same<typename internal::remove_cvref<U>::type, return_in_place_t>::value &&
	                                  !std::is_same<typename internal::remove_cvref<U>::type, return_optional>::value>::type* = nullptr>
	constexpr return_optional( U&& value )
	  : internal::OptionalBase<T>( return_in_place, std::forward<U>( value ) )
	{
	}

	template <typename U,
	          bool IsConstructible                                                              = std::is_constructible<T, const U&>::value,
	          bool TIsBool                                                                      = std::is_same<typename internal::remove_cvref<T>::type, bool>::value,
	          bool IsConvertible                                                                = std::is_convertible<return_optional<U>, T>::value,
	          typename std::enable_if<IsConstructible && ( !TIsBool || !IsConvertible )>::type* = nullptr>
	constexpr return_optional( const return_optional<U>& src )
	  : internal::OptionalBase<T>()
	{
		if ( src.init_ ) {
			new ( &( storage_.value_ ) ) T( src.storage_.value_ );
			init_ = true;
		}
	}
	template <typename U,
	          bool IsConstructible                                                              = std::is_constructible<T, U>::value,
	          bool TIsBool                                                                      = std::is_same<typename internal::remove_cvref<T>::type, bool>::value,
	          bool IsConvertible                                                                = std::is_convertible<return_optional<U>, T>::value,
	          typename std::enable_if<IsConstructible && ( !TIsBool || !IsConvertible )>::type* = nullptr>
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

	template <bool IsCopyConstructible                                                = std::is_copy_constructible<T>::value,
	          bool IsCopyAssignable                                                   = std::is_copy_assignable<T>::value,
	          typename std::enable_if<IsCopyConstructible && IsCopyAssignable>::type* = nullptr>
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
	template <bool IsCopyConstructible                                                     = std::is_copy_constructible<T>::value,
	          bool IsCopyAssignable                                                        = std::is_copy_assignable<T>::value,
	          typename std::enable_if<!( IsCopyConstructible && IsCopyAssignable )>::type* = nullptr>
	return_optional& operator=( const return_optional& src ) = delete;

	template <bool IsMoveConstructible                                                = std::is_move_constructible<T>::value,
	          bool IsMoveAssignable                                                   = std::is_move_assignable<T>::value,
	          typename std::enable_if<IsMoveConstructible && IsMoveAssignable>::type* = nullptr>
	return_optional& operator=( return_optional&& src ) noexcept( std::is_nothrow_move_constructible<T>::value && std::is_nothrow_move_assignable<T>::value )
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
	template <bool IsMoveConstructible                                                     = std::is_move_constructible<T>::value,
	          bool IsMoveAssignable                                                        = std::is_move_assignable<T>::value,
	          typename std::enable_if<!( IsMoveConstructible && IsMoveAssignable )>::type* = nullptr>
	return_optional& operator=( return_optional&& src ) = delete;

	template <typename U                                                       = T,
	          typename std::enable_if<!std::is_same<typename internal::remove_cvref<U>::type, return_optional>::value &&
	                                  !( std::is_scalar<T>::value && std::is_same<T, typename std::decay<U>::type>::value ) &&
	                                  std::is_constructible<T, U>::value &&
	                                  std::is_assignable<T&, U>::value>::type* = nullptr>
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

	template <typename U,
	          typename std::enable_if<std::is_constructible<T, const U&>::value &&
	                                  std::is_assignable<T&, const U&>::value &&
	                                  !std::is_convertible<T, return_optional<U>>::value &&
	                                  !std::is_assignable<T&, return_optional<U>&>::value &&
	                                  !std::is_assignable<T&, return_optional<U>&&>::value &&
	                                  !std::is_assignable<T&, const return_optional<U>&>::value &&
	                                  !std::is_assignable<T&, const return_optional<U>&&>::value>::type* = nullptr>
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

	template <typename U,
	          typename std::enable_if<std::is_constructible<T, U>::value &&
	                                  std::is_assignable<T&, U>::value &&
	                                  !std::is_convertible<T, return_optional<U>>::value &&
	                                  !std::is_assignable<T&, return_optional<U>&>::value &&
	                                  !std::is_assignable<T&, return_optional<U>&&>::value &&
	                                  !std::is_assignable<T&, const return_optional<U>&>::value &&
	                                  !std::is_assignable<T&, const return_optional<U>&&>::value>::type* = nullptr>
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

	template <class... Args,
	          typename std::enable_if<std::is_constructible<T, Args...>::value>::type* = nullptr>
	constexpr T& emplace( Args&&... args )
	{
		reset();

		new ( &( storage_.value_ ) ) T( std::forward<Args>( args )... );
		init_ = true;

		return storage_.value_;
	}

	template <typename U, typename... Args,
	          typename std::enable_if<std::is_constructible<T, std::initializer_list<U>&, Args...>::value>::type* = nullptr>
	constexpr T& emplace( std::initializer_list<U>& il, Args&&... args )
	{
		reset();

		new ( &( storage_.value_ ) ) T( il, std::forward<Args>( args )... );
		init_ = true;

		return storage_.value_;
	}

	template <bool IsMoveConstructible                                           = std::is_move_constructible<T>::value,
	          bool IsSwappable                                                   = std::is_swappable<T>::value,
	          typename std::enable_if<IsMoveConstructible && IsSwappable>::type* = nullptr>
	constexpr void swap( return_optional& src ) noexcept( std::is_nothrow_swappable<T>::value && std::is_nothrow_move_constructible<T>::value )
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

	template <class U                                                  = T,
	          typename std::enable_if<std::is_convertible<U, T>::value &&
	                                  !std::is_array<U>::value>::type* = nullptr>
	constexpr T value_or( U&& v ) const&
	{
		return has_value() ? value() : static_cast<T>( std::forward<U>( v ) );
	}   // (1)
	template <class U                                                  = T,
	          typename std::enable_if<std::is_convertible<U, T>::value &&
	                                  !std::is_array<U>::value>::type* = nullptr>
	constexpr T value_or( U&& v ) &&
	{
		return has_value() ? value() : static_cast<T>( std::forward<U>( v ) );
	}   // (2)

private:
	using internal::OptionalBase<T>::init_;
	using internal::OptionalBase<T>::storage_;
};

template <typename T, typename U>
constexpr bool operator==( const return_optional<T>& x, const return_optional<U>& y )
{
	if ( x.has_value() != y.has_value() ) return false;
	if ( !x.has_value() ) return true;

	return *x == *y;
}
template <typename T, typename U>
constexpr bool operator!=( const return_optional<T>& x, const return_optional<U>& y )
{
	if ( x.has_value() != y.has_value() ) return true;
	if ( !x.has_value() ) return false;

	return *x != *y;
}
template <typename T, typename U>
constexpr bool operator<( const return_optional<T>& x, const return_optional<U>& y )
{
	if ( !y.has_value() ) return false;
	if ( !x.has_value() ) return true;

	return *x < *y;
}
template <typename T, typename U>
constexpr bool operator>( const return_optional<T>& x, const return_optional<U>& y )
{
	if ( !x.has_value() ) return false;
	if ( !y.has_value() ) return true;

	return *x > *y;
}
template <typename T, typename U>
constexpr bool operator<=( const return_optional<T>& x, const return_optional<U>& y )
{
	if ( !x.has_value() ) return true;
	if ( !y.has_value() ) return false;

	return *x <= *y;
}
template <typename T, typename U>
constexpr bool operator>=( const return_optional<T>& x, const return_optional<U>& y )
{
	if ( !y.has_value() ) return true;
	if ( !x.has_value() ) return false;

	return *x >= *y;
}

#if __cpp_lib_three_way_comparison >= 201907L
template <class T, std::three_way_comparable_with<T> U>
constexpr std::compare_three_way_result_t<T, U> operator<=>( const return_optional<T>& x, const return_optional<U>& y )
{
	if ( x.has_value() && y.has_value() ) return *x <=> *y;
	return x.has_value() <=> y.has_value();
}
#endif

template <class T>
constexpr bool operator==( const return_optional<T>& x, return_nullopt_t ) noexcept
{
	return !( x.has_value() );
}
#if __cpp_lib_three_way_comparison >= 201907L
template <class T>
constexpr std::strong_ordering operator<=>( const return_optional<T>& x, return_nullopt_t ) noexcept
{
	return x.has_value() <=> false;
}
#endif

// 22.5.8, comparison with T
template <class T, class U>
constexpr bool operator==( const return_optional<T>& x, const U& v )
{
	return x.has_value() ? *x == v : false;
}
template <class T, class U>
constexpr bool operator==( const T& v, const return_optional<U>& x )
{
	return x.has_value() ? v == *x : false;
}
template <class T, class U>
constexpr bool operator!=( const return_optional<T>& x, const U& v )
{
	return x.has_value() ? *x != v : true;
}
template <class T, class U>
constexpr bool operator!=( const T& v, const return_optional<U>& x )
{
	return x.has_value() ? v == *x : true;
}
template <class T, class U>
constexpr bool operator<( const return_optional<T>& x, const U& v )
{
	return x.has_value() ? *x < v : true;
}
template <class T, class U>
constexpr bool operator<( const T& v, const return_optional<U>& x )
{
	return x.has_value() ? v < *x : false;
}
template <class T, class U>
constexpr bool operator>( const return_optional<T>& x, const U& v )
{
	return x.has_value() ? *x > v : false;
}
template <class T, class U>
constexpr bool operator>( const T& v, const return_optional<U>& x )
{
	return x.has_value() ? v > *x : true;
}
template <class T, class U>
constexpr bool operator<=( const return_optional<T>& x, const U& v )
{
	return x.has_value() ? *x <= v : true;
}
template <class T, class U>
constexpr bool operator<=( const T& v, const return_optional<U>& x )
{
	return x.has_value() ? v <= *x : false;
}
template <class T, class U>
constexpr bool operator>=( const return_optional<T>& x, const U& v )
{
	return x.has_value() ? *x >= v : false;
}
template <class T, class U>
constexpr bool operator>=( const T& v, const return_optional<U>& x )
{
	return x.has_value() ? v >= *x : true;
}
#if __cpp_lib_three_way_comparison >= 201907L
template <class T, class U, class V>
	requires( !std::is_base_of<return_optional<V>, U>::value ) && std::three_way_comparable_with<T, U>
constexpr std::compare_three_way_result_t<T, U> operator<=>( const return_optional<T>& x, const U& v )
{
	return x.has_value() ? *x <=> v : std::strong_ordering::less;
}
#endif

#endif

}   // namespace concurrent
}   // namespace alpha

#endif
