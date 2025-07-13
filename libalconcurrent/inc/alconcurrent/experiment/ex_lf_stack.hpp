/**
 * @file ex_lf_stack.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief ロックフリー共有ポインタを用いたスタックの実装
 * @version 0.1
 * @date 2025-02-16
 *
 * @copyright Copyright (c) 2025, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCURRENT_INC_EX_LF_STACK_HPP_
#define ALCONCURRENT_INC_EX_LF_STACK_HPP_

#include "alconcurrent/internal/alcc_optional.hpp"
#include "lf_shared_ptr.hpp"

namespace alpha {
namespace concurrent {

template <typename T>
class ex_node {
public:
	using value_type          = T;
	using next_lf_ptr_t       = lf_shared_ptr<ex_node<T>>;
	using next_ptr_snapshot_t = typename lf_shared_ptr<ex_node<T>>::shared_type;

	ex_node( void ) = delete;
	ex_node( const value_type& v )
	  : value_( v )
	  , next_()
	{
	}
	ex_node( value_type&& v )
	  : value_( std::move( v ) )
	  , next_()
	{
	}
	ex_node( const ex_node& )            = delete;
	ex_node( ex_node&& )                 = delete;
	ex_node& operator=( const ex_node& ) = delete;
	ex_node& operator=( ex_node&& )      = delete;
	~ex_node()                           = default;

	alcc_optional<value_type> value_;
	next_lf_ptr_t             next_;
};

template <typename T>
class ex_lf_stack {
public:
	using value_type = T;

	~ex_lf_stack() = default;
	ex_lf_stack( void )
	  : head_()
	{
	}
	ex_lf_stack( ex_lf_stack&& )                 = delete;
	ex_lf_stack& operator=( ex_lf_stack&& )      = delete;
	ex_lf_stack( const ex_lf_stack& )            = delete;
	ex_lf_stack& operator=( const ex_lf_stack& ) = delete;

	void push( const value_type& v )
	{
		auto p_new_node = make_nts_shared<node_type>( v );
		auto p_head     = head_.load();
		do {
			p_new_node->next_.store( p_head );
		} while ( !head_.compare_exchange_weak( p_head, p_new_node ) );
	}
	void push( value_type&& v )
	{
		auto p_new_node = make_nts_shared<node_type>( std::move( v ) );
		auto p_head     = head_.load();
		do {
			p_new_node->next_.store( p_head );
		} while ( !head_.compare_exchange_weak( p_head, p_new_node ) );
	}

	alcc_optional<value_type> pop( void )
	{
		auto p_head = head_.load();
		while ( p_head.get() != nullptr ) {
			auto p_next = p_head->next_.load();
			if ( head_.compare_exchange_weak( p_head, p_next ) ) {
				return p_head->value_;
			}
		}
		return alcc_nullopt;
	}

	size_t count_size( void ) const
	{
		size_t ans    = 0;
		auto   p_next = head_.load();
		while ( p_next.get() != nullptr ) {
			ans++;
			p_next = p_next->next_.load();
		}
		return ans;
	}

	bool is_empty( void ) const
	{
		return head_.load().get() == nullptr;
	}

	size_t get_allocated_num( void ) const
	{
		return 0;
	}

private:
	using node_type           = ex_node<value_type>;
	using next_lf_ptr_t       = typename node_type::next_lf_ptr_t;
	using next_ptr_snapshot_t = typename node_type::next_ptr_snapshot_t;

	next_lf_ptr_t head_;
};

}   // namespace concurrent
}   // namespace alpha

#endif
