/**
 * @file test_type_variation.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief Unitテストで共通的に使用する型を定義するヘッダファイル
 * @version 0.1
 * @date 2024-12-14
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef TEST_TYPE_VARIATION_HPP_
#define TEST_TYPE_VARIATION_HPP_

#include <type_traits>

class fully_userdefined_5_special_op {
public:
	fully_userdefined_5_special_op( void )
	  : x_( 1 )
	  , y_( 2.0 )
	{
	}
	~fully_userdefined_5_special_op()
	{
		x_ = 0;
		y_ = 0.0;
	}
	fully_userdefined_5_special_op( const fully_userdefined_5_special_op& src )
	  : x_( src.x_ )
	  , y_( src.y_ )
	{
	}
	fully_userdefined_5_special_op( fully_userdefined_5_special_op&& src )
	  : x_( src.x_ )
	  , y_( src.y_ )
	{
	}
	fully_userdefined_5_special_op& operator=( const fully_userdefined_5_special_op& src )
	{
		if ( this == &src ) return *this;
		x_ = src.x_;
		y_ = src.y_;
		return *this;
	}
	fully_userdefined_5_special_op& operator=( fully_userdefined_5_special_op&& src )
	{
		if ( this == &src ) return *this;
		x_ = src.x_;
		y_ = src.y_;
		return *this;
	}

	int    x_;
	double y_;
};
static_assert( std::is_default_constructible<fully_userdefined_5_special_op>::value );
static_assert( std::is_move_constructible<fully_userdefined_5_special_op>::value && std::is_move_assignable<fully_userdefined_5_special_op>::value );
static_assert( std::is_copy_constructible<fully_userdefined_5_special_op>::value && std::is_copy_assignable<fully_userdefined_5_special_op>::value );

class partly_userdefined_5_special_op_no_default_constructor {
public:
	partly_userdefined_5_special_op_no_default_constructor( int x_arg, double y_arg )
	  : x_( x_arg )
	  , y_( y_arg )
	{
	}
	~partly_userdefined_5_special_op_no_default_constructor()
	{
		x_ = 0;
		y_ = 0.0;
	}
	partly_userdefined_5_special_op_no_default_constructor( const partly_userdefined_5_special_op_no_default_constructor& src )
	  : x_( src.x_ )
	  , y_( src.y_ )
	{
	}
	partly_userdefined_5_special_op_no_default_constructor( partly_userdefined_5_special_op_no_default_constructor&& src )
	  : x_( src.x_ )
	  , y_( src.y_ )
	{
	}
	partly_userdefined_5_special_op_no_default_constructor& operator=( const partly_userdefined_5_special_op_no_default_constructor& src )
	{
		if ( this == &src ) return *this;
		x_ = src.x_;
		y_ = src.y_;
		return *this;
	}
	partly_userdefined_5_special_op_no_default_constructor& operator=( partly_userdefined_5_special_op_no_default_constructor&& src )
	{
		if ( this == &src ) return *this;
		x_ = src.x_;
		y_ = src.y_;
		return *this;
	}

	int    x_;
	double y_;
};
static_assert( !std::is_default_constructible<partly_userdefined_5_special_op_no_default_constructor>::value );
static_assert( std::is_move_constructible<partly_userdefined_5_special_op_no_default_constructor>::value && std::is_move_assignable<partly_userdefined_5_special_op_no_default_constructor>::value );
static_assert( std::is_copy_constructible<partly_userdefined_5_special_op_no_default_constructor>::value && std::is_copy_assignable<partly_userdefined_5_special_op_no_default_constructor>::value );

class partly_userdefined_5_special_op_no_copy_constructor_assignment {
public:
	partly_userdefined_5_special_op_no_copy_constructor_assignment( void )
	  : x_( 1 )
	  , y_( 2.0 )
	{
	}
	~partly_userdefined_5_special_op_no_copy_constructor_assignment()
	{
		x_ = 0;
		y_ = 0.0;
	}
	partly_userdefined_5_special_op_no_copy_constructor_assignment( partly_userdefined_5_special_op_no_copy_constructor_assignment&& src )
	  : x_( src.x_ )
	  , y_( src.y_ )
	{
	}
	partly_userdefined_5_special_op_no_copy_constructor_assignment& operator=( partly_userdefined_5_special_op_no_copy_constructor_assignment&& src )
	{
		if ( this == &src ) return *this;
		x_ = src.x_;
		y_ = src.y_;
		return *this;
	}

	partly_userdefined_5_special_op_no_copy_constructor_assignment( const partly_userdefined_5_special_op_no_copy_constructor_assignment& src )            = delete;
	partly_userdefined_5_special_op_no_copy_constructor_assignment& operator=( const partly_userdefined_5_special_op_no_copy_constructor_assignment& src ) = delete;

	int    x_;
	double y_;
};
static_assert( std::is_default_constructible<partly_userdefined_5_special_op_no_copy_constructor_assignment>::value );
static_assert( std::is_move_constructible<partly_userdefined_5_special_op_no_copy_constructor_assignment>::value && std::is_move_assignable<partly_userdefined_5_special_op_no_copy_constructor_assignment>::value );
static_assert( !( std::is_copy_constructible<partly_userdefined_5_special_op_no_copy_constructor_assignment>::value && std::is_copy_assignable<partly_userdefined_5_special_op_no_copy_constructor_assignment>::value ) );

class partly_userdefined_5_special_op_no_move_constructor_assignment {
public:
	partly_userdefined_5_special_op_no_move_constructor_assignment( void )
	  : x_( 1 )
	  , y_( 2.0 )
	{
	}
	~partly_userdefined_5_special_op_no_move_constructor_assignment()
	{
		x_ = 0;
		y_ = 0.0;
	}
	partly_userdefined_5_special_op_no_move_constructor_assignment( const partly_userdefined_5_special_op_no_move_constructor_assignment& src )
	  : x_( src.x_ )
	  , y_( src.y_ )
	{
	}
	partly_userdefined_5_special_op_no_move_constructor_assignment& operator=( const partly_userdefined_5_special_op_no_move_constructor_assignment& src )
	{
		if ( this == &src ) return *this;
		x_ = src.x_;
		y_ = src.y_;
		return *this;
	}

	partly_userdefined_5_special_op_no_move_constructor_assignment( partly_userdefined_5_special_op_no_move_constructor_assignment&& src )            = delete;
	partly_userdefined_5_special_op_no_move_constructor_assignment& operator=( partly_userdefined_5_special_op_no_move_constructor_assignment&& src ) = delete;

	int    x_;
	double y_;
};
static_assert( std::is_default_constructible<partly_userdefined_5_special_op_no_move_constructor_assignment>::value );
static_assert( !( std::is_move_constructible<partly_userdefined_5_special_op_no_move_constructor_assignment>::value && std::is_move_assignable<partly_userdefined_5_special_op_no_move_constructor_assignment>::value ) );
static_assert( std::is_copy_constructible<partly_userdefined_5_special_op_no_move_constructor_assignment>::value && std::is_copy_assignable<partly_userdefined_5_special_op_no_move_constructor_assignment>::value );

class partly_userdefined_5_special_op_no_copy_move_constructor_assignment {
public:
	partly_userdefined_5_special_op_no_copy_move_constructor_assignment( void )
	  : x_( 1 )
	  , y_( 2.0 )
	{
	}
	~partly_userdefined_5_special_op_no_copy_move_constructor_assignment()
	{
		x_ = 0;
		y_ = 0.0;
	}

	partly_userdefined_5_special_op_no_copy_move_constructor_assignment( const partly_userdefined_5_special_op_no_copy_move_constructor_assignment& src )            = delete;
	partly_userdefined_5_special_op_no_copy_move_constructor_assignment& operator=( const partly_userdefined_5_special_op_no_copy_move_constructor_assignment& src ) = delete;

	partly_userdefined_5_special_op_no_copy_move_constructor_assignment( partly_userdefined_5_special_op_no_copy_move_constructor_assignment&& src )            = delete;
	partly_userdefined_5_special_op_no_copy_move_constructor_assignment& operator=( partly_userdefined_5_special_op_no_copy_move_constructor_assignment&& src ) = delete;

	int    x_;
	double y_;
};

static_assert( std::is_default_constructible<partly_userdefined_5_special_op_no_copy_move_constructor_assignment>::value );
static_assert( !( std::is_move_constructible<partly_userdefined_5_special_op_no_copy_move_constructor_assignment>::value && std::is_move_assignable<partly_userdefined_5_special_op_no_copy_move_constructor_assignment>::value ) );
static_assert( !( std::is_copy_constructible<partly_userdefined_5_special_op_no_copy_move_constructor_assignment>::value && std::is_copy_assignable<partly_userdefined_5_special_op_no_copy_move_constructor_assignment>::value ) );

#endif
