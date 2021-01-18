/*!
 * @file	one_way_list_node.hpp
 * @brief	node class definition for one way list type structure as a internal common class
 * @author	Teruaki Ata
 * @date	Created on 2021/01/09
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef ONE_WAY_LIST_NODE_HPP_
#define ONE_WAY_LIST_NODE_HPP_

#include <atomic>
#include <memory>
#include <tuple>

#include "free_node_storage.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

template <typename T>
struct one_way_list_node : public node_of_list {
	using value_type = T;

	one_way_list_node( void )
	  : target_()
	  , next_( nullptr )
	{
	}

	one_way_list_node( const T& cont_arg )
	  : target_( cont_arg )
	  , next_( nullptr )
	{
	}

	virtual ~one_way_list_node()
	{
		return;
	}

	T get_value( void ) const
	{
		std::atomic_thread_fence( std::memory_order_acquire );
		return target_;
	}

	void set_value( const T& value_arg )
	{
		target_ = value_arg;
		std::atomic_thread_fence( std::memory_order_release );
	}

	one_way_list_node* get_next( void )
	{
		return next_.load( std::memory_order_acquire );
	}

	void set_next( one_way_list_node* p_new_next )
	{
		next_.store( p_new_next, std::memory_order_release );
		return;
	}

	bool next_CAS( one_way_list_node** pp_expect_ptr, one_way_list_node* p_desired_ptr )
	{
		return next_.compare_exchange_weak( *pp_expect_ptr, p_desired_ptr );
	}

private:
	value_type                      target_;
	std::atomic<one_way_list_node*> next_;
};

}   // namespace internal

}   // namespace concurrent
}   // namespace alpha

#endif /* ONE_WAY_LIST_NODE_HPP_ */
