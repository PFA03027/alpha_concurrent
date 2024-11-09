/**
 * @file od_node_essence.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-10-19
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCURRENT_INC_INTERNAL_OD_NODE_ESSENCE_HPP_
#define ALCONCURRENT_INC_INTERNAL_OD_NODE_ESSENCE_HPP_

#include "alconcurrent/hazard_ptr.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

template <typename T>
class deleter_nothing {
public:
	constexpr void operator()( T& ) {}
};
template <>
class deleter_nothing<void> {
public:
	constexpr void operator()( void* ) {}
};

/**
 * @brief node of one direction linking simply
 *
 */
class od_node_simple_link {
public:
	explicit constexpr od_node_simple_link( od_node_simple_link* p_next_arg = nullptr ) noexcept
	  : p_raw_next_( p_next_arg )
	{
	}
	constexpr od_node_simple_link( const od_node_simple_link& src ) noexcept = default;
	od_node_simple_link( od_node_simple_link&& src ) noexcept
	  : p_raw_next_( src.p_raw_next_ )
	{
		src.p_raw_next_ = nullptr;
	}
	od_node_simple_link& operator=( const od_node_simple_link& src ) noexcept = default;
	od_node_simple_link& operator=( od_node_simple_link&& src ) noexcept
	{
		if ( this == &src ) return *this;
		p_raw_next_     = src.p_raw_next_;
		src.p_raw_next_ = nullptr;
		return *this;
	}
	virtual ~od_node_simple_link() = default;

	od_node_simple_link* next( void ) const noexcept
	{
		return p_raw_next_;
	}

	void set_next( od_node_simple_link* p_n ) noexcept
	{
		p_raw_next_ = p_n;
	}

private:
	od_node_simple_link* p_raw_next_;
};

/**
 * @brief node of one direction linked by hazard handler
 *
 */
class alignas( atomic_variable_align ) od_node_link_by_hazard_handler {
public:
	using hazard_ptr_handler_t = hazard_ptr_handler<od_node_link_by_hazard_handler>;
	using hazard_pointer       = typename hazard_ptr_handler<od_node_link_by_hazard_handler>::hazard_pointer;

	explicit constexpr od_node_link_by_hazard_handler( od_node_link_by_hazard_handler* p_next_arg = nullptr ) noexcept
	  : hph_next_( p_next_arg )
	{
	}
	od_node_link_by_hazard_handler( const od_node_link_by_hazard_handler& ) noexcept            = default;
	od_node_link_by_hazard_handler( od_node_link_by_hazard_handler&& ) noexcept                 = default;
	od_node_link_by_hazard_handler& operator=( const od_node_link_by_hazard_handler& ) noexcept = default;
	od_node_link_by_hazard_handler& operator=( od_node_link_by_hazard_handler&& ) noexcept      = default;
	virtual ~od_node_link_by_hazard_handler()                                                   = default;

	od_node_link_by_hazard_handler* next( void ) const noexcept
	{
		return hph_next_.load();
	}

	void set_next( od_node_link_by_hazard_handler* p_n ) noexcept
	{
		hph_next_.store( p_n );
	}

	hazard_pointer get_hazard_ptr_of_next( void )
	{
		return hph_next_.get();
	}

	hazard_ptr_handler_t& hazard_handler_of_next( void ) noexcept
	{
		return hph_next_;
	}

	const hazard_ptr_handler_t& hazard_handler_of_next( void ) const noexcept
	{
		return hph_next_;
	}

	void* get_pointer_of_hazard_check( void ) noexcept
	{
		return reinterpret_cast<void*>( this );
	}

	const void* get_pointer_of_hazard_check( void ) const noexcept
	{
		return reinterpret_cast<const void*>( this );
	}

private:
	hazard_ptr_handler_t hph_next_;
};

/**
 * @brief node of one direction linked by hazard handler that is markable
 *
 */
class alignas( atomic_variable_align ) od_node_1bit_markable_link_by_hazard_handler {
public:
	using hazard_ptr_handler_t = hazard_ptr_w_mark_handler<od_node_1bit_markable_link_by_hazard_handler>;
	using hazard_pointer       = typename hazard_ptr_w_mark_handler<od_node_1bit_markable_link_by_hazard_handler>::hazard_pointer;

	explicit od_node_1bit_markable_link_by_hazard_handler( od_node_1bit_markable_link_by_hazard_handler* p_next_arg = nullptr ) noexcept
	  : hph_next_( p_next_arg )
	{
	}
	od_node_1bit_markable_link_by_hazard_handler( const od_node_1bit_markable_link_by_hazard_handler& )            = default;
	od_node_1bit_markable_link_by_hazard_handler( od_node_1bit_markable_link_by_hazard_handler&& )                 = default;
	od_node_1bit_markable_link_by_hazard_handler& operator=( const od_node_1bit_markable_link_by_hazard_handler& ) = default;
	od_node_1bit_markable_link_by_hazard_handler& operator=( od_node_1bit_markable_link_by_hazard_handler&& )      = default;
	virtual ~od_node_1bit_markable_link_by_hazard_handler()                                                        = default;

	std::tuple<od_node_1bit_markable_link_by_hazard_handler*, bool> next( void ) const noexcept
	{
		return hph_next_.load();
	}

	void set_next( const std::tuple<od_node_1bit_markable_link_by_hazard_handler*, bool>& tp ) noexcept
	{
		hph_next_.store( tp );
	}

	std::tuple<hazard_pointer, bool> get_hazard_ptr_of_next( void )
	{
		return hph_next_.get();
	}

	hazard_ptr_handler_t& hazard_handler_of_next( void ) noexcept
	{
		return hph_next_;
	}

	const hazard_ptr_handler_t& hazard_handler_of_next( void ) const noexcept
	{
		return hph_next_;
	}

	void* get_pointer_of_hazard_check( void ) noexcept
	{
		return reinterpret_cast<void*>( this );
	}

	const void* get_pointer_of_hazard_check( void ) const noexcept
	{
		return reinterpret_cast<const void*>( this );
	}

private:
	hazard_ptr_handler_t hph_next_;
};

/**
 * @brief value carrier class
 *
 * @tparam T value type kept in this class
 */
template <typename T>
class value_carrier {
public:
	using value_type           = T;
	using reference_type       = T&;
	using const_reference_type = const T&;

	template <bool IsDefaultConstructible = std::is_default_constructible<value_type>::value, typename std::enable_if<IsDefaultConstructible>::type* = nullptr>
	value_carrier( void ) noexcept( std::is_nothrow_default_constructible<value_type>::value )
	  : v_ {}
	{
	}

	template <bool IsCopyable = std::is_copy_constructible<value_type>::value, typename std::enable_if<IsCopyable>::type* = nullptr>
	value_carrier( const value_type& v_arg ) noexcept( std::is_nothrow_copy_constructible<value_type>::value )
	  : v_( v_arg )
	{
	}

	template <bool IsMovable = std::is_move_constructible<value_type>::value, typename std::enable_if<IsMovable>::type* = nullptr>
	value_carrier( value_type&& v_arg ) noexcept( std::is_nothrow_move_constructible<value_type>::value )
	  : v_( std::move( v_arg ) )
	{
	}

	template <typename Arg1st, typename... RemainingArgs,
	          typename RemoveCVArg1st                                                          = typename std::remove_reference<typename std::remove_const<Arg1st>::type>::type,
	          typename std::enable_if<!std::is_same<RemoveCVArg1st, value_type>::value>::type* = nullptr>
	value_carrier( Arg1st&& arg1, RemainingArgs&&... args )
	  : v_( std::forward<Arg1st>( arg1 ), std::forward<RemainingArgs>( args )... )
	{
	}

	template <bool IsCopyable = std::is_copy_assignable<value_type>::value, typename std::enable_if<IsCopyable>::type* = nullptr>
	void set_value( const value_type& v_arg ) noexcept( std::is_nothrow_copy_assignable<value_type>::value )
	{
		v_ = v_arg;
	}

	template <bool IsMovable = std::is_move_assignable<value_type>::value, typename std::enable_if<IsMovable>::type* = nullptr>
	void set_value( value_type&& v_arg ) noexcept( std::is_nothrow_move_assignable<value_type>::value )
	{
		v_ = std::move( v_arg );
	}

	reference_type get_value( void ) &
	{
		return v_;
	}

	const_reference_type get_value( void ) const&
	{
		return v_;
	}

	template <bool IsMovable = std::is_move_assignable<value_type>::value, typename std::enable_if<IsMovable>::type* = nullptr>
	value_type get_value( void ) &&
	{
		return std::move( v_ );
	}

	template <bool IsCopyable                                          = std::is_copy_assignable<value_type>::value,
	          bool IsMovable                                           = std::is_move_assignable<value_type>::value,
	          typename std::enable_if<!IsMovable && IsCopyable>::type* = nullptr>
	value_type get_value( void ) const&&
	{
		return v_;
	}

private:
	value_type v_;
};

/**
 * @brief node of one direction list
 *
 * @tparam T value type kept in this class
 */
template <typename T>
class od_node_type1 : public value_carrier<T>, public od_node_simple_link, public od_node_link_by_hazard_handler {
public:
	using value_type           = T;
	using reference_type       = T&;
	using const_reference_type = const T&;

	template <bool IsDefaultConstructible = std::is_default_constructible<value_type>::value, typename std::enable_if<IsDefaultConstructible>::type* = nullptr>
	od_node_type1( void ) noexcept( std::is_nothrow_default_constructible<value_type>::value )
	  : value_carrier<T>()
	  , od_node_simple_link()
	  , od_node_link_by_hazard_handler()
	{
	}

	template <bool IsCopyable = std::is_copy_constructible<value_type>::value, typename std::enable_if<IsCopyable>::type* = nullptr>
	od_node_type1( const value_type& v_arg ) noexcept( std::is_nothrow_copy_constructible<value_type>::value )
	  : value_carrier<T>( v_arg )
	  , od_node_simple_link()
	  , od_node_link_by_hazard_handler()
	{
	}

	template <bool IsMovable = std::is_move_constructible<value_type>::value, typename std::enable_if<IsMovable>::type* = nullptr>
	od_node_type1( value_type&& v_arg ) noexcept( std::is_nothrow_move_constructible<value_type>::value )
	  : value_carrier<T>( std::move( v_arg ) )
	  , od_node_simple_link()
	  , od_node_link_by_hazard_handler()
	{
	}

	template <typename Arg1st, typename... RemainingArgs,
	          typename RemoveCVArg1st                                                          = typename std::remove_reference<typename std::remove_const<Arg1st>::type>::type,
	          typename std::enable_if<!std::is_same<RemoveCVArg1st, value_type>::value>::type* = nullptr>
	od_node_type1( Arg1st&& arg1, RemainingArgs&&... args )
	  : value_carrier<T>( std::forward<Arg1st>( arg1 ), std::forward<RemainingArgs>( args )... )
	  , od_node_simple_link()
	  , od_node_link_by_hazard_handler()
	{
	}
};

/**
 * @brief node of one direction list
 *
 * @tparam T value type kept in this class
 */
template <typename T>
class od_node_type2 : public value_carrier<T>, public od_node_simple_link, public od_node_1bit_markable_link_by_hazard_handler {
public:
	using value_type           = T;
	using reference_type       = T&;
	using const_reference_type = const T&;

	template <bool IsDefaultConstructible = std::is_default_constructible<value_type>::value, typename std::enable_if<IsDefaultConstructible>::type* = nullptr>
	od_node_type2( void ) noexcept( std::is_nothrow_default_constructible<value_type>::value )
	  : value_carrier<T>()
	  , od_node_simple_link()
	  , od_node_1bit_markable_link_by_hazard_handler()
	{
	}

	template <bool IsCopyable = std::is_copy_constructible<value_type>::value, typename std::enable_if<IsCopyable>::type* = nullptr>
	od_node_type2( const value_type& v_arg ) noexcept( std::is_nothrow_copy_constructible<value_type>::value )
	  : value_carrier<T>( v_arg )
	  , od_node_simple_link()
	  , od_node_1bit_markable_link_by_hazard_handler()
	{
	}

	template <bool IsMovable = std::is_move_constructible<value_type>::value, typename std::enable_if<IsMovable>::type* = nullptr>
	od_node_type2( value_type&& v_arg ) noexcept( std::is_nothrow_move_constructible<value_type>::value )
	  : value_carrier<T>( std::move( v_arg ) )
	  , od_node_simple_link()
	  , od_node_1bit_markable_link_by_hazard_handler()
	{
	}

	template <typename Arg1st, typename... RemainingArgs,
	          typename RemoveCVArg1st                                                          = typename std::remove_reference<typename std::remove_const<Arg1st>::type>::type,
	          typename std::enable_if<!std::is_same<RemoveCVArg1st, value_type>::value>::type* = nullptr>
	od_node_type2( Arg1st&& arg1, RemainingArgs&&... args )
	  : value_carrier<T>( std::forward<Arg1st>( arg1 ), std::forward<RemainingArgs>( args )... )
	  , od_node_simple_link()
	  , od_node_1bit_markable_link_by_hazard_handler()
	{
	}
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif