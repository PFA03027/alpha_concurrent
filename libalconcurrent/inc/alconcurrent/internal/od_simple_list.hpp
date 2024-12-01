/**
 * @file od_simple_list.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-10-20
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCURRENT_INC_INTERNAL_OD_SIMPLE_LIST_HPP_
#define ALCONCURRENT_INC_INTERNAL_OD_SIMPLE_LIST_HPP_

#include <condition_variable>
#include <functional>

#include "alconcurrent/hazard_ptr.hpp"
#include "alconcurrent/internal/od_node_essence.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

/**
 * @brief simple list that keeps class "od_node_simple_link" and/or the derived class of "od_node_simple_link"
 *
 */
class od_simple_list {
public:
	using node_pointer       = od_node_simple_link*;
	using const_node_pointer = const od_node_simple_link*;

	constexpr od_simple_list( void ) noexcept = default;
	od_simple_list( const od_simple_list& )   = delete;
	od_simple_list( od_simple_list&& src ) noexcept;
	od_simple_list& operator=( const od_simple_list& ) = delete;
	od_simple_list& operator=( od_simple_list&& src ) noexcept;
	~od_simple_list();

	void swap( od_simple_list& src ) noexcept;
	void clear( void );

	void push_front( node_pointer p_nd ) noexcept;
	void push_back( node_pointer p_nd ) noexcept;

	void merge_push_front( od_simple_list&& src ) noexcept;
	void merge_push_front( node_pointer p_nd ) noexcept;

	void merge_push_back( od_simple_list&& src ) noexcept;
	void merge_push_back( node_pointer p_nd ) noexcept;

	node_pointer pop_front( void ) noexcept;

	bool is_empty( void ) const
	{
		return p_head_ == nullptr;
	}
	bool is_one( void ) const
	{
		return ( p_head_ != nullptr ) && ( p_head_ == p_tail_ );
	}
	bool is_more_than_one( void ) const
	{
		return ( p_head_ != nullptr ) && ( p_head_ != p_tail_ );
	}

	/**
	 * @brief if pred return true, that node is purged and push it into return value
	 *
	 * @param pred callable pred(const_node_pointer) and return bool
	 * @return od_simple_list has the purged nodes
	 */
	od_simple_list split_if( std::function<bool( const_node_pointer )> pred );

	/**
	 * @brief purge all nodes and apply Pred
	 *
	 * Pred becomes an onwer of a node. therefore Pred should not leak the memory of a node pointer
	 *
	 * @param pred callable pred( node_pointer) and return bool
	 */
	void clear( std::function<void( node_pointer )> pred );

	size_t size( void ) const noexcept
	{
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
		if ( count_ == 0 ) {
			if ( p_head_ != nullptr ) {
				throw std::runtime_error( "internal error: count is zero, but p_head_ is not nullptr. counting and/or internal status is unexpected" );
			}
			if ( p_tail_ != nullptr ) {
				throw std::runtime_error( "internal error: count is zero, but p_tail_ is not nullptr. counting and/or internal status is unexpected" );
			}
		} else if ( count_ == 1 ) {
			if ( p_head_ != p_tail_ ) {
				throw std::runtime_error( "internal error: count is one, but p_head_ is not same to p_tail_. counting and/or internal status is unexpected" );
			}
		}
#endif
		return count_;
	}

private:
	void merge_push_front( node_pointer p_nd_head, node_pointer p_nd_tail ) noexcept;
	void merge_push_back( node_pointer p_nd_head, node_pointer p_nd_tail ) noexcept;

	node_pointer p_head_ = nullptr;
	node_pointer p_tail_ = nullptr;
	size_t       count_  = 0;
};

/**
 * @brief od_simple_list list that supports exclusive control
 *
 */
class od_simple_list_lockable {
public:
	class locker {
	public:
		locker( const locker& ) = delete;
		locker( locker&& src )
		  : reftarget_( src.reftarget_ )
		  , lg_( std::move( src.lg_ ) )
		{
		}
		locker& operator=( const locker& ) = delete;
		locker& operator=( locker&& src )  = delete;
		// locker& operator=( locker&& src )
		// {
		// 	reftarget_ = src.reftarget_;
		// 	lg_        = std::move( src.lg_ );
		// }
		~locker() = default;

		bool owns_lock( void ) const noexcept
		{
			return lg_.owns_lock();
		}

		od_simple_list& ref( void )
		{
			if ( !lg_.owns_lock() ) {
				throw std::logic_error( "no lock access is logic error" );
			}
			return reftarget_;
		}

		const od_simple_list& ref( void ) const
		{
			if ( !lg_.owns_lock() ) {
				throw std::logic_error( "no lock access is logic error" );
			}
			return reftarget_;
		}

	private:
		locker( od_simple_list& reftarget_arg, std::mutex& mtx_arg )
		  : reftarget_( reftarget_arg )
		  , lg_( mtx_arg )
		{
		}
		locker( od_simple_list& reftarget_arg, std::mutex& mtx_arg, std::try_to_lock_t )
		  : reftarget_( reftarget_arg )
		  , lg_( mtx_arg, std::try_to_lock )
		{
		}

		od_simple_list&              reftarget_;
		std::unique_lock<std::mutex> lg_;

		friend class od_simple_list_lockable;
	};

	constexpr od_simple_list_lockable( void ) noexcept        = default;
	od_simple_list_lockable( const od_simple_list_lockable& ) = delete;
	od_simple_list_lockable( od_simple_list_lockable&& src ) noexcept
	  : mtx_()
	  , list_data_( std::move( src.lock().ref() ) )
	{
	}
	od_simple_list_lockable& operator=( const od_simple_list_lockable& ) = delete;
	od_simple_list_lockable& operator=( od_simple_list_lockable&& src ) noexcept
	{
		// need to avoid the dead-lock
		od_simple_list tmp( std::move( src.lock().ref() ) );
		lock().ref().swap( tmp );
		return *this;
	}
	~od_simple_list_lockable() = default;

	locker lock( void )
	{
		return locker( list_data_, mtx_ );
	}
	locker try_lock( void )
	{
		return locker( list_data_, mtx_, std::try_to_lock );
	}

protected:
	std::mutex     mtx_;
	od_simple_list list_data_;
};

class od_simple_list_conditional_lockable {
public:
	class locker {
	public:
		locker( const locker& ) = delete;
		locker( locker&& src )
		  : reftarget_( src.reftarget_ )
		  , lg_( std::move( src.lg_ ) )
		  , refcv_( src.refcv_ )
		{
		}
		locker& operator=( const locker& ) = delete;
		locker& operator=( locker&& src )  = delete;
		// locker& operator=( locker&& src )
		// {
		// 	reftarget_ = src.reftarget_;
		// 	lg_        = std::move( src.lg_ );
		// 	refcv_     = src.refcv_;
		// }
		~locker() = default;

		bool owns_lock( void ) const noexcept
		{
			return lg_.owns_lock();
		}

		od_simple_list& ref( void )
		{
			if ( !lg_.owns_lock() ) {
				throw std::logic_error( "no lock access is logic error" );
			}
			return reftarget_;
		}

		const od_simple_list& ref( void ) const
		{
			if ( !lg_.owns_lock() ) {
				throw std::logic_error( "no lock access is logic error" );
			}
			return reftarget_;
		}

		void wait( void )
		{
			if ( !lg_.owns_lock() ) {
				throw std::logic_error( "no lock access is logic error" );
				// condition_variable requires this pre-condtion requirement also.
			}
			refcv_.wait( lg_ );
		}

		template <class Predicate>
		void wait( Predicate pred )
		{
			if ( !lg_.owns_lock() ) {
				throw std::logic_error( "no lock access is logic error" );
				// condition_variable requires this pre-condtion requirement also.
			}
			while ( !pred() ) {
				refcv_.wait( lg_ );
			}
		}

		void notify_all( void )
		{
			refcv_.notify_all();
		}

	private:
		locker( od_simple_list& reftarget_arg, std::mutex& mtx_arg, std::condition_variable& cv_arg )
		  : reftarget_( reftarget_arg )
		  , lg_( mtx_arg )
		  , refcv_( cv_arg )
		{
		}
		locker( od_simple_list& reftarget_arg, std::mutex& mtx_arg, std::condition_variable& cv_arg, std::try_to_lock_t )
		  : reftarget_( reftarget_arg )
		  , lg_( mtx_arg, std::try_to_lock )
		  , refcv_( cv_arg )
		{
		}

		od_simple_list&              reftarget_;
		std::unique_lock<std::mutex> lg_;
		std::condition_variable&     refcv_;

		friend class od_simple_list_conditional_lockable;
	};

	od_simple_list_conditional_lockable( void ) noexcept                              = default;
	od_simple_list_conditional_lockable( const od_simple_list_conditional_lockable& ) = delete;
	od_simple_list_conditional_lockable( od_simple_list_conditional_lockable&& src ) noexcept
	  : mtx_()
	  , cv_()
	  , list_data_( std::move( src.lock().ref() ) )
	{
	}
	od_simple_list_conditional_lockable& operator=( const od_simple_list_conditional_lockable& ) = delete;
	od_simple_list_conditional_lockable& operator=( od_simple_list_conditional_lockable&& src ) noexcept
	{
		// need to avoid the dead-lock
		od_simple_list tmp( std::move( src.lock().ref() ) );
		lock().ref().swap( tmp );
		return *this;
	}
	~od_simple_list_conditional_lockable() = default;

	locker lock( void )
	{
		return locker( list_data_, mtx_, cv_ );
	}
	locker try_lock( void )
	{
		return locker( list_data_, mtx_, cv_, std::try_to_lock );
	}

protected:
	std::mutex              mtx_;
	std::condition_variable cv_;
	od_simple_list          list_data_;
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif