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

/**
 * @brief node of one direction linking simply
 *
 */
class od_node_simple_link {
public:
	explicit od_node_simple_link( od_node_simple_link* p_next_arg = nullptr ) noexcept
	  : p_raw_next_( p_next_arg )
	{
	}
	od_node_simple_link( const od_node_simple_link& src ) noexcept = default;
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

	explicit od_node_link_by_hazard_handler( od_node_link_by_hazard_handler* p_next_arg = nullptr ) noexcept
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

	void reuse_hazard_ptr_of_next( hazard_pointer& hp )
	{
		return hph_next_.reuse( hp );
	}

	hazard_ptr_handler_t& hazard_handler_of_next( void ) noexcept
	{
		return hph_next_;
	}

	const hazard_ptr_handler_t& hazard_handler_of_next( void ) const noexcept
	{
		return hph_next_;
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

	void reuse_hazard_ptr_of_next( std::tuple<hazard_pointer, bool>& hp )
	{
		return hph_next_.reuse( hp );
	}

	hazard_ptr_handler_t& hazard_handler_of_next( void ) noexcept
	{
		return hph_next_;
	}

	const hazard_ptr_handler_t& hazard_handler_of_next( void ) const noexcept
	{
		return hph_next_;
	}

private:
	hazard_ptr_handler_t hph_next_;
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif