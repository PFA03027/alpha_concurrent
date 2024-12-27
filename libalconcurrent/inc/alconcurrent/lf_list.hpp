/*!
 * @file	lf_list.hpp
 * @brief	semi lock-free list
 * @author	Teruaki Ata
 * @date	Created on 2021/02/23
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef ALCONCCURRENT_INC_LF_LIST_HPP_
#define ALCONCCURRENT_INC_LF_LIST_HPP_

#include <atomic>
#include <functional>
#include <memory>
#include <tuple>

#include "hazard_ptr.hpp"
#include "internal/alcc_optional.hpp"
#include "internal/od_lockfree_list.hpp"
#include "internal/od_node_pool.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

/*!
 * @brief	ロックフリー方式用の単方向型リスト
 */
template <typename T>
class x_lockfree_list {
public:
	static_assert( ( !std::is_class<T>::value ) ||
	                   ( std::is_class<T>::value &&
	                     ( ( std::is_copy_constructible<T>::value && std::is_copy_assignable<T>::value ) ||
	                       ( std::is_move_constructible<T>::value && std::is_move_assignable<T>::value ) ) ),
	               "T should be copy constructible and copy assignable, or, move constructible and move assignable" );

	using value_type      = T;
	using predicate_t     = std::function<bool( const value_type& )>;   //!< find_if関数で使用する述語関数を保持するfunction型
	using for_each_func_t = std::function<void( value_type& )>;         //!< for_each関数で各要素の処理を実行するための関数を保持するfunction型

	constexpr x_lockfree_list( void ) noexcept
	  : lf_list_impl_()
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	  , allocated_node_count_( 0 )
	  , call_count_push_front_( 0 )
	  , call_count_pop_front_( 0 )
	  , call_count_push_back_( 0 )
	  , call_count_pop_back_( 0 )
#endif
	{
	}

	x_lockfree_list( size_t reserve_size ) noexcept
	  : x_lockfree_list()
	{
	}
	~x_lockfree_list()
	{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		if ( node_pool_t::profile_info_count() != 0 ) {
			internal::LogOutput( log_type::TEST, "%s", node_pool_t::profile_info_string().c_str() );
			node_pool_t::clear_as_possible_as();
		}
		internal::LogOutput( log_type::DUMP, "x_lockfree_list: allocated_node_count   = %zu", allocated_node_count_.load() );
		internal::LogOutput( log_type::DUMP, "x_lockfree_list: call_count_push_front_ = %zu", call_count_push_front_.load() );
		internal::LogOutput( log_type::DUMP, "x_lockfree_list: call_count_pop_back_   = %zu", call_count_pop_back_.load() );
#endif

		lf_list_impl_.clear();
	}

	void find_if( predicate_t& f )
	{
		find_if_impl( f );
	}
	void find_if( predicate_t&& f )
	{
		find_if_impl( f );
	}

	/**
	 * @brief 値を挿入する
	 *
	 * predがtrueを返すノードを見つけられなかった場合、リストの最後に挿入される。
	 *
	 * @param cont_arg 挿入する値
	 * @param pred 挿入個所を見つけるための関数オブジェクト
	 */
	template <bool IsMovable = std::is_move_assignable<value_type>::value, typename std::enable_if<IsMovable>::type* = nullptr>
	void insert(
		predicate_t& pred,      //!< [in]	A predicate function to specify the insertion position. const value_type& is passed as an argument
		value_type&& cont_arg   //!< [in]	a value to insert this list
	)
	{
		node_pointer p_new_node = alloc_node_impl();
		p_new_node->set_value( std::move( cont_arg ) );

		insert_to_before_of_curr_impl( p_new_node, pred );
	}
	template <bool IsCopyable = std::is_copy_assignable<value_type>::value, typename std::enable_if<IsCopyable>::type* = nullptr>
	void insert(
		predicate_t&      pred,      //!< [in]	A predicate function to specify the insertion position. const value_type& is passed as an argument
		const value_type& cont_arg   //!< [in]	a value to insert this list
	)
	{
		node_pointer p_new_node = alloc_node_impl();
		p_new_node->set_value( cont_arg );

		insert_to_before_of_curr_impl( p_new_node, pred );
	}
	template <bool IsMovable = std::is_move_assignable<value_type>::value, typename std::enable_if<IsMovable>::type* = nullptr>
	void insert(
		predicate_t&& pred,      //!< [in]	A predicate function to specify the insertion position. const value_type& is passed as an argument
		value_type&&  cont_arg   //!< [in]	a value to insert this list
	)
	{
		node_pointer p_new_node = alloc_node_impl();
		p_new_node->set_value( std::move( cont_arg ) );

		insert_to_before_of_curr_impl( p_new_node, pred );
	}
	template <bool IsCopyable = std::is_copy_assignable<value_type>::value, typename std::enable_if<IsCopyable>::type* = nullptr>
	void insert(
		predicate_t&&     pred,      //!< [in]	A predicate function to specify the insertion position. const value_type& is passed as an argument
		const value_type& cont_arg   //!< [in]	a value to insert this list
	)
	{
		node_pointer p_new_node = alloc_node_impl();
		p_new_node->set_value( cont_arg );

		insert_to_before_of_curr_impl( p_new_node, pred );
	}

	/*!
	 * @brief	remove all of nodes that pred return true from this list
	 */
	size_t remove_all_if(
		predicate_t& pred   //!< [in]	A predicate function to specify the deletion target. const value_type& is passed as an argument
	)
	{
		size_t ans = 0;

		std::pair<od_lockfree_list::hazard_pointer_w_mark, od_lockfree_list::hazard_pointer_w_mark> ret;
		while ( true ) {
			ret = find_if_impl( pred );
			if ( lf_list_impl_.is_end_node( ret ) ) {
				break;
			}
			if ( lf_list_impl_.remove( ret.first, std::move( ret.second ) ) ) {
				ans++;
			}
		}

		return ans;
	}
	size_t remove_all_if(
		predicate_t&& pred   //!< [in]	A predicate function to specify the deletion target. const value_type& is passed as an argument
	)
	{
		return remove_all_if( pred );
	}

	/*!
	 * @brief	remove a first node that pred return true from this list
	 */
	template <bool IsMovable = std::is_move_assignable<value_type>::value, typename std::enable_if<IsMovable>::type* = nullptr>
	alcc_optional<value_type> remove_one_if(
		predicate_t& pred   //!< [in]	A predicate function to specify the deletion target. const value_type& is passed as an argument
	)
	{
		auto ret = remove_one_if_impl( pred );
		if ( !ret.has_value() ) return alcc_nullopt;

		node_pointer p = static_cast<node_pointer>( ret.value().second.hp_.get() );
		return alcc_optional<value_type> { std::move( p->get_value() ) };
	}
	template <bool IsMovable                                           = std::is_move_assignable<value_type>::value,
	          bool IsCopyable                                          = std::is_copy_assignable<value_type>::value,
	          typename std::enable_if<!IsMovable && IsCopyable>::type* = nullptr>
	alcc_optional<value_type> remove_one_if(
		predicate_t& pred   //!< [in]	A predicate function to specify the deletion target. const value_type& is passed as an argument
	)
	{
		auto ret = remove_one_if_impl( pred );
		if ( !ret.has_value() ) return alcc_nullopt;

		node_pointer p = static_cast<node_pointer>( ret.value().second.hp_.get() );
		return alcc_optional<value_type> { p->get_value() };
	}

	template <bool IsMovable                                          = std::is_move_assignable<value_type>::value,
	          bool IsCopyable                                         = std::is_copy_assignable<value_type>::value,
	          typename std::enable_if<IsMovable || IsCopyable>::type* = nullptr>
	alcc_optional<value_type> remove_one_if(
		predicate_t&& pred   //!< [in]	A predicate function to specify the deletion target. const value_type& is passed as an argument
	)
	{
		return remove_one_if( pred );
	}

	/*!
	 * @brief	Applies the specified function to all elements.
	 *
	 * @pre
	 * The Function must meet the MoveConstructible requirement, but not the CopyConstructible requirement.
	 *
	 * @warning
	 * Due to the lock-free nature, it is possible that the element being processed may be deleted
	 * (just marked for deletion, the actual deletion may even be delayed until all threads are no
	 * longer referencing it) or modified.
	 * Also, access contention may occur during function execution. Therefore, when changing an element,
	 * it is necessary to perform access control such as exclusive control in Function f.
	 * Therefore, when a non-lock-free function such as exclusive control by mutex is used, the lock-free
	 * property of this class is limited.
	 */
	for_each_func_t for_each(
		for_each_func_t&& f   //!< [in]	A function f is passed value_type& as an argument
	)
	{
		lf_list_impl_.for_each(
			[&f]( od_lockfree_list::node_pointer p_nd ) {
				x_lockfree_list::node_pointer p_my_t = static_cast<x_lockfree_list::node_pointer>( p_nd );
				return f( p_my_t->get_value() );
			} );

		return std::move( f );
	}

	/*!
	 * @brief	push a value to the front of this list
	 */
	template <bool IsCopyable = std::is_copy_assignable<value_type>::value, typename std::enable_if<IsCopyable>::type* = nullptr>
	void push_front(
		const value_type& cont_arg   //!< [in]	a value to insert this list
	)
	{
		node_pointer p_new_node = alloc_node_impl();
		p_new_node->set_value( cont_arg );

		insert_to_next_of_prev_impl( p_new_node, []( const value_type& ) -> bool { return true; } );

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		call_count_push_front_++;
#endif
	}
	template <bool IsMovable = std::is_move_assignable<value_type>::value, typename std::enable_if<IsMovable>::type* = nullptr>
	void push_front(
		value_type&& cont_arg   //!< [in]	a value to insert this list
	)
	{
		node_pointer p_new_node = alloc_node_impl();
		p_new_node->set_value( std::move( cont_arg ) );

		insert_to_next_of_prev_impl( p_new_node, []( const value_type& ) -> bool { return true; } );

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		call_count_push_front_++;
#endif
	}

	template <typename... Args>
	void emplace_front(
		Args&&... args   //!< [in]	a value to insert to front of this list
	)
	{
		node_pointer p_new_node = alloc_node_impl();
		p_new_node->emplace_value( std::forward<Args>( args )... );

		insert_to_next_of_prev_impl( p_new_node, []( const value_type& ) -> bool { return true; } );

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		call_count_push_front_++;
#endif
	}

	/*!
	 * @brief	pop a value from the front of this list
	 *
	 * @return	1st return value. Success or fail to pop
	 * @retval	true	success to pop a value
	 * @retval	false	because of no data in the list, fail to pop
	 *
	 * @return	2nd return value. poped value, if 1st return value is true.
	 *
	 */
	template <bool IsMovable = std::is_move_assignable<value_type>::value, typename std::enable_if<IsMovable>::type* = nullptr>
	alcc_optional<value_type> pop_front( void )
	{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		call_count_pop_front_++;
#endif
		alcc_optional<od_lockfree_list::hazard_pointer_w_mark> ret = lf_list_impl_.remove_mark_head();
		if ( !ret.has_value() ) return alcc_nullopt;
		if ( ret.value().hp_.get() == nullptr ) return alcc_nullopt;

		node_pointer p = static_cast<node_pointer>( ret.value().hp_.get() );
		return alcc_optional<value_type> { std::move( p->get_value() ) };
	}
	template <bool IsMovable                                           = std::is_move_assignable<value_type>::value,
	          bool IsCopyable                                          = std::is_copy_assignable<value_type>::value,
	          typename std::enable_if<!IsMovable && IsCopyable>::type* = nullptr>
	alcc_optional<value_type> pop_front( void )
	{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		call_count_pop_front_++;
#endif
		alcc_optional<od_lockfree_list::hazard_pointer_w_mark> ret = lf_list_impl_.remove_mark_head();
		if ( !ret.has_value() ) return alcc_nullopt;
		if ( ret.value().hp_.get() == nullptr ) return alcc_nullopt;

		node_pointer p = static_cast<node_pointer>( ret.value().hp_.get() );
		return alcc_optional<value_type> { p->get_value() };
	}

	/*!
	 * @brief	append a value to the end of this list
	 */
	template <bool IsCopyable = std::is_copy_assignable<value_type>::value, typename std::enable_if<IsCopyable>::type* = nullptr>
	void push_back(
		const value_type& cont_arg   //!< [in]	a value to insert this list
	)
	{
		node_pointer p_new_node = alloc_node_impl();
		p_new_node->set_value( cont_arg );

		insert_to_before_of_curr_impl( p_new_node, []( const value_type& ) -> bool { return false; } );

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		call_count_push_back_++;
#endif
	}
	template <bool IsMovable = std::is_move_assignable<value_type>::value, typename std::enable_if<IsMovable>::type* = nullptr>
	void push_back(
		value_type&& cont_arg   //!< [in]	a value to insert this list
	)
	{
		node_pointer p_new_node = alloc_node_impl();
		p_new_node->set_value( std::move( cont_arg ) );

		insert_to_before_of_curr_impl( p_new_node, []( const value_type& ) -> bool { return false; } );

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		call_count_push_back_++;
#endif
	}
	template <typename... Args>
	void emplace_back(
		Args&&... args   //!< [in]	a value to insert to back of this list
	)
	{
		node_pointer p_new_node = alloc_node_impl();
		p_new_node->emplace_value( std::forward<Args>( args )... );

		insert_to_before_of_curr_impl( p_new_node, []( const value_type& ) -> bool { return false; } );

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		call_count_push_back_++;
#endif
	}

	/*!
	 * @brief	pop a value from the end of this list
	 *
	 * @return	1st return value. Success or fail to pop
	 * @retval	true	success to pop a value
	 * @retval	false	because of no data in the list, fail to pop
	 *
	 * @return	2nd return value. poped value, if 1st return value is true.
	 *
	 */
	template <bool IsMovable = std::is_move_assignable<value_type>::value, typename std::enable_if<IsMovable>::type* = nullptr>
	alcc_optional<value_type> pop_back( void )
	{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		call_count_pop_back_++;
#endif
		alcc_optional<od_lockfree_list::hazard_pointer_w_mark> ret = lf_list_impl_.remove_mark_tail();
		if ( !ret.has_value() ) return alcc_nullopt;
		if ( ret.value().hp_.get() == nullptr ) return alcc_nullopt;

		node_pointer p = static_cast<node_pointer>( ret.value().hp_.get() );
		return alcc_optional<value_type> { std::move( p->get_value() ) };
	}
	template <bool IsMovable                                           = std::is_move_assignable<value_type>::value,
	          bool IsCopyable                                          = std::is_copy_assignable<value_type>::value,
	          typename std::enable_if<!IsMovable && IsCopyable>::type* = nullptr>
	alcc_optional<value_type> pop_back( void )
	{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		call_count_pop_back_++;
#endif
		alcc_optional<od_lockfree_list::hazard_pointer_w_mark> ret = lf_list_impl_.remove_mark_tail();
		if ( !ret.has_value() ) return alcc_nullopt;
		if ( ret.value().hp_.get() == nullptr ) return alcc_nullopt;

		node_pointer p = static_cast<node_pointer>( ret.value().hp_.get() );
		return alcc_optional<value_type> { p->get_value() };
	}

	/*!
	 * @brief	get the total number of the nodes
	 *
	 * @warning
	 * This List will be access by several thread concurrently. So, true number of this List may be changed when caller uses the returned value.
	 */
	size_t count_size( void )
	{
		return lf_list_impl_.count_size();
	}

	/*!
	 * @brief	get the total number of the allocated internal nodes
	 *
	 * @warning
	 * This List will be access by several thread concurrently. So, true number of this List may be changed when caller uses the returned value.
	 */
	size_t get_allocated_num( void )
	{
		size_t ans = 0;
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		ans = allocated_node_count_.load();
#endif
		return ans;
	}

private:
	using node_type    = od_node_type2<T>;
	using node_pointer = od_node_type2<T>*;

	using node_pool_t = od_node_pool<node_type>;
	class node_list_lockfree_t : public od_lockfree_list {
	protected:
		void do_for_purged_node( od_lockfree_list::node_pointer p_nd ) override
		{
			if ( p_nd == nullptr ) return;

			// このリストに投入されているノードの型はx_lockfree_list::node_typeであることを保証しているので、static_castで対応する。
			x_lockfree_list::node_pool_t::push( static_cast<x_lockfree_list::node_pointer>( p_nd ) );
		}
	};

	std::pair<od_lockfree_list::hazard_pointer_w_mark, od_lockfree_list::hazard_pointer_w_mark> find_if_impl(
		predicate_t& pred   //!< [in]	チェック関数
	)
	{
		return lf_list_impl_.find_if(
			[&pred]( const od_lockfree_list::node_pointer p_nd ) -> bool {
				x_lockfree_list::node_pointer p_my_t = static_cast<x_lockfree_list::node_pointer>( p_nd );
				return pred( p_my_t->get_value() );
			} );
	}

	node_pointer alloc_node_impl( void )
	{
		node_pointer p_new_node = x_lockfree_list::node_pool_t::pop();
		if ( p_new_node == nullptr ) {
			p_new_node = new node_type;
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			allocated_node_count_++;
#endif
		}

		return p_new_node;
	}

	void insert_to_next_of_prev_impl( node_pointer p_in, predicate_t& pred )
	{
		std::pair<od_lockfree_list::hazard_pointer_w_mark, od_lockfree_list::hazard_pointer_w_mark> ret;
		do {
			ret = find_if_impl( pred );
		} while ( !lf_list_impl_.insert_to_next_of_prev( p_in, ret.first, ret.second ) );
	}
	void insert_to_next_of_prev_impl( node_pointer p_in, predicate_t&& pred )
	{
		insert_to_next_of_prev_impl( p_in, pred );
	}
	void insert_to_before_of_curr_impl( node_pointer p_in, predicate_t& pred )
	{
		std::pair<od_lockfree_list::hazard_pointer_w_mark, od_lockfree_list::hazard_pointer_w_mark> ret;
		do {
			ret = find_if_impl( pred );
		} while ( !lf_list_impl_.insert_to_before_of_curr( p_in, ret.first, ret.second ) );
	}
	void insert_to_before_of_curr_impl( node_pointer p_in, predicate_t&& pred )
	{
		insert_to_before_of_curr_impl( p_in, pred );
	}

	alcc_optional<std::pair<od_lockfree_list::hazard_pointer_w_mark, od_lockfree_list::hazard_pointer_w_mark>> remove_one_if_impl(
		predicate_t& pred   //!< [in]	A predicate function to specify the deletion target. const value_type& is passed as an argument
	)
	{
		std::pair<od_lockfree_list::hazard_pointer_w_mark, od_lockfree_list::hazard_pointer_w_mark> ret;
		do {
			ret = find_if_impl( pred );
			if ( lf_list_impl_.is_end_node( ret ) ) {
				return alcc_nullopt;
			}
		} while ( !lf_list_impl_.remove_mark( ret.second ) );

		return alcc_optional<std::pair<od_lockfree_list::hazard_pointer_w_mark, od_lockfree_list::hazard_pointer_w_mark>> { std::move( ret ) };
	}

	node_list_lockfree_t lf_list_impl_;

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	std::atomic<size_t> allocated_node_count_;
	std::atomic<size_t> call_count_push_front_;
	std::atomic<size_t> call_count_pop_front_;
	std::atomic<size_t> call_count_push_back_;
	std::atomic<size_t> call_count_pop_back_;
#endif
};

}   // namespace internal

template <typename T>
class lockfree_list : public internal::x_lockfree_list<T> {
public:
	lockfree_list( void ) = default;
	lockfree_list( size_t reserve_size ) noexcept
	  : lockfree_list()
	{
	}
};
template <typename T>
class lockfree_list<T[]> : public internal::x_lockfree_list<T*> {
public:
	using value_type = T[];

	lockfree_list( void ) = default;
	lockfree_list( size_t reserve_size ) noexcept
	  : lockfree_list()
	{
	}
};
template <typename T, size_t N>
class lockfree_list<T[N]> : public internal::x_lockfree_list<std::array<T, N>> {
public:
	using value_type = T[N];

	lockfree_list( void ) = default;
	lockfree_list( size_t reserve_size ) noexcept
	  : lockfree_list()
	{
	}
};

}   // namespace concurrent
}   // namespace alpha

#endif /* INC_LF_LIST_HPP_ */
