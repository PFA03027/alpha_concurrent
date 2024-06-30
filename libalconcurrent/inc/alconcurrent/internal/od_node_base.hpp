/**
 * @file od_node.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-04
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCURRENT_INC_INTERNAL_OD_NODE_BASE_HPP_
#define ALCONCURRENT_INC_INTERNAL_OD_NODE_BASE_HPP_

#include <atomic>
#include <condition_variable>
#include <memory>
#include <type_traits>

#include "alconcurrent/hazard_ptr.hpp"
#include "alconcurrent/lf_mem_alloc.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

template <typename NODE_T, typename NEXT_NODE_T = NODE_T>
class alignas( atomic_variable_align ) od_node_base {
public:
	using node_type            = NODE_T;
	using node_pointer         = NODE_T*;
	using next_node_type       = NEXT_NODE_T;
	using next_node_pointer    = NEXT_NODE_T*;
	using hazard_ptr_handler_t = hazard_ptr_handler<next_node_type>;
	using hazard_pointer       = typename hazard_ptr_handler<next_node_type>::hazard_pointer;

	next_node_pointer    p_raw_next_;
	hazard_ptr_handler_t hph_next_;

	od_node_base( node_pointer p_next_arg = nullptr ) noexcept
	  : p_raw_next_( nullptr )
	  , hph_next_( p_next_arg )
	{
	}

	void clear_next( void )
	{
		p_raw_next_ = nullptr;
		hph_next_.store( nullptr );
	}

	class raw_next_rw {
	public:
		static inline next_node_pointer read( const node_pointer onb )
		{
			return onb->p_raw_next_;
		}
		static inline void write( node_pointer onb, next_node_pointer p )
		{
			onb->p_raw_next_ = p;
		}
	};
	class hph_next_rw {
	public:
		static inline next_node_pointer read( const node_pointer onb )
		{
			return onb->hph_next_.load();
		}
		static inline void write( node_pointer onb, next_node_pointer p )
		{
			onb->hph_next_.store( p );
		}
	};

#ifdef ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
#else
#if 0   // too slow... if enabled, prune thread termination has failed...
#if __cpp_aligned_new
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size, std::align_val_t alignment )   // possible throw std::bad_alloc, from C++17
	{
		void* p_ans = gmem_allocate( size, static_cast<size_t>( alignment ) );
		if ( p_ans == nullptr ) {
			throw std::bad_alloc();
		}
		return p_ans;
	}
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++17
	{
		return gmem_allocate( size, static_cast<size_t>( alignment ) );
	}
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size, std::align_val_t alignment )   // possible throw std::bad_alloc, from C++17
	{
		void* p_ans = gmem_allocate( size, static_cast<size_t>( alignment ) );
		if ( p_ans == nullptr ) {
			throw std::bad_alloc();
		}
		return p_ans;
	}
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++17
	{
		return gmem_allocate( size, static_cast<size_t>( alignment ) );
	}
#endif
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size )   // possible throw std::bad_alloc, from C++11
	{
		void* p_ans = gmem_allocate( size );
		if ( p_ans == nullptr ) {
			throw std::bad_alloc();
		}
		return p_ans;
	}
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++11
	{
		return gmem_allocate( size );
	}
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size )   // possible throw std::bad_alloc, from C++11
	{
		void* p_ans = gmem_allocate( size );
		if ( p_ans == nullptr ) {
			throw std::bad_alloc();
		}
		return p_ans;
	}
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++11
	{
		return gmem_allocate( size );
	}

	ALCC_INTERNAL_NODISCARD_ATTR void* operator new( std::size_t size, void* ptr ) noexcept   // placement new, from C++11
	{
		return ptr;
	}
	ALCC_INTERNAL_NODISCARD_ATTR void* operator new[]( std::size_t size, void* ptr ) noexcept   // placement new for array, from C++11
	{
		return ptr;
	}

#if __cpp_aligned_new
	void operator delete( void* ptr, std::align_val_t alignment ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
	{
		gmem_deallocate( ptr );
	}
	void operator delete[]( void* ptr, std::align_val_t alignment ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
	{
		gmem_deallocate( ptr );
	}

	void operator delete( void* ptr, std::size_t size, std::align_val_t alignment ) noexcept   // from C++17
	{
		gmem_deallocate( ptr );
	}
	void operator delete[]( void* ptr, std::size_t size, std::align_val_t alignment ) noexcept   // from C++17
	{
		gmem_deallocate( ptr );
	}

	void operator delete( void* ptr, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
	{
		gmem_deallocate( ptr );
	}
	void operator delete[]( void* ptr, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
	{
		gmem_deallocate( ptr );
	}

	void operator delete( void* ptr, std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17
	{
		gmem_deallocate( ptr );
	}
	void operator delete[]( void* ptr, std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17
	{
		gmem_deallocate( ptr );
	}
#endif
	void operator delete( void* ptr ) noexcept   // from C++11
	{
		gmem_deallocate( ptr );
	}
	void operator delete[]( void* ptr ) noexcept   // from C++11
	{
		gmem_deallocate( ptr );
	}

	void operator delete( void* ptr, std::size_t size ) noexcept   // from C++14
	{
		gmem_deallocate( ptr );
	}
	void operator delete[]( void* ptr, std::size_t size ) noexcept   // from C++14
	{
		gmem_deallocate( ptr );
	}

	void operator delete( void* ptr, const std::nothrow_t& ) noexcept   // from C++11
	{
		gmem_deallocate( ptr );
	}
	void operator delete[]( void* ptr, const std::nothrow_t& ) noexcept   // from C++11
	{
		gmem_deallocate( ptr );
	}

	void operator delete( void* ptr, std::size_t size, const std::nothrow_t& ) noexcept   // from C++14
	{
		gmem_deallocate( ptr );
	}
	void operator delete[]( void* ptr, std::size_t size, const std::nothrow_t& ) noexcept   // from C++14
	{
		gmem_deallocate( ptr );
	}

	void operator delete( void* ptr, void* ) noexcept   // delete for area that is initialized by placement new.
	{
		// nothing to do
	}
	void operator delete[]( void* ptr, void* ) noexcept   // delete for area that is initialized by placement new.
	{
		// nothing to do
	}
#endif
#endif
};

/**
 * @brief list implementation class that has od_node<T> pointer has header pointer
 *
 * @warning this class is not thread safe
 *
 * @tparam NODE_T this type should be the derived class from od_node_base<NODE_T>
 * @tparam NEXT_RW_T this type should be od_node_base<NODE_T>::raw_next_rw or od_node_base<NODE_T>::hph_next_rw
 */
template <typename NODE_T, typename NEXT_RW_T>
class alignas( atomic_variable_align ) od_node_list_base_impl {
	static_assert( std::is_base_of<typename NODE_T::next_node_type, NODE_T>::value, "NODE_T::next_node_type should be a base class of NODE_T." );
	static_assert( std::is_same<NEXT_RW_T, typename NODE_T::raw_next_rw>::value ||
	                   std::is_same<NEXT_RW_T, typename NODE_T::hph_next_rw>::value,
	               "NEXT_RW_T should be accesser class of od_node_base<NODE_T>" );

public:
	using node_type      = NODE_T;
	using node_pointer   = NODE_T*;
	using node_next_rw_t = NEXT_RW_T;

	constexpr od_node_list_base_impl( void ) noexcept       = default;
	od_node_list_base_impl( const od_node_list_base_impl& ) = delete;
	constexpr od_node_list_base_impl( od_node_list_base_impl&& src ) noexcept
	  : p_head_( src.p_head_ )
	  , p_tail_( src.p_tail_ )
	{
		src.p_head_ = nullptr;
		src.p_tail_ = nullptr;

#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_TAIL_NODE_NEXT_NULLPTR
		if ( p_tail_ != nullptr ) {
			if ( node_next_rw_t::read( p_tail_ ) != nullptr ) {
				LogOutput( log_type::ERR, "tail node has non-nullptr in next" );
				std::terminate();
			}
		}
#endif
	}
	od_node_list_base_impl&           operator=( const od_node_list_base_impl& ) = delete;
	constexpr od_node_list_base_impl& operator=( od_node_list_base_impl&& src ) noexcept
	{
		od_node_list_base_impl( std::move( src ) ).swap( *this );

#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_TAIL_NODE_NEXT_NULLPTR
		if ( p_tail_ != nullptr ) {
			if ( node_next_rw_t::read( p_tail_ ) != nullptr ) {
				LogOutput( log_type::ERR, "tail node has non-nullptr in next" );
				std::terminate();
			}
		}
#endif

		return *this;
	}
	~od_node_list_base_impl()
	{
		clear();
	}

	void swap( od_node_list_base_impl& src ) noexcept
	{
		node_pointer p_tmp;

		p_tmp       = p_head_;
		p_head_     = src.p_head_;
		src.p_head_ = p_tmp;

		p_tmp       = p_tail_;
		p_tail_     = src.p_tail_;
		src.p_tail_ = p_tmp;

#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_TAIL_NODE_NEXT_NULLPTR
		if ( p_tail_ != nullptr ) {
			if ( node_next_rw_t::read( p_tail_ ) != nullptr ) {
				LogOutput( log_type::ERR, "tail node has non-nullptr in next" );
				std::terminate();
			}
		}
#endif
	}

	void push_front( node_pointer p_nd ) noexcept
	{
		if ( p_nd == nullptr ) return;
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_PUSH_FRONT_FUNCTION_NULLPTR
		if ( node_next_rw_t::read( p_nd ) != nullptr ) {
			LogOutput( log_type::WARN, "od_node_list_base::push_front() receives a od_node<T> that has non nullptr in next" );
			node_next_rw_t::write( p_nd, nullptr );
		}
#endif
		if ( p_head_ == nullptr ) {
			p_tail_ = p_nd;
		}
		node_next_rw_t::write( p_nd, p_head_ );
		p_head_ = p_nd;
	}

	void push_back( node_pointer p_nd ) noexcept
	{
		if ( p_nd == nullptr ) return;
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_PUSH_FRONT_FUNCTION_NULLPTR
		if ( node_next_rw_t::read( p_nd ) != nullptr ) {
			LogOutput( log_type::WARN, "od_node_list_base::push_back() receives a od_node<T> that has non nullptr in next" );
			node_next_rw_t::write( p_nd, nullptr );
		}
#endif
		if ( p_head_ == nullptr ) {
			p_head_ = p_nd;
			p_tail_ = p_nd;
		} else {
			node_next_rw_t::write( p_tail_, p_nd );
			p_tail_ = p_nd;
		}
	}

	void merge_push_front( od_node_list_base_impl&& src ) noexcept
	{
		if ( src.p_head_ == nullptr ) return;

		next_node_pointer p_src_head = src.p_head_;
		next_node_pointer p_src_tail = src.p_tail_;
		src.p_head_                  = nullptr;
		src.p_tail_                  = nullptr;

		merge_push_front( p_src_head, p_src_tail );
	}

	void merge_push_front( node_pointer p_nd ) noexcept
	{
		if ( p_nd == nullptr ) return;

		next_node_pointer p_cur = p_nd;
		next_node_pointer p_nxt = node_next_rw_t::read( p_cur );
		while ( p_nxt != nullptr ) {
			p_cur = p_nxt;
			p_nxt = node_next_rw_t::read( p_cur );
		}

		merge_push_front( p_nd, p_cur );
	}

	void merge_push_back( od_node_list_base_impl&& src ) noexcept
	{
		if ( src.p_head_ == nullptr ) return;

		next_node_pointer p_src_head = src.p_head_;
		next_node_pointer p_src_tail = src.p_tail_;
		src.p_head_                  = nullptr;
		src.p_tail_                  = nullptr;

		merge_push_back( p_src_head, p_src_tail );
	}

	void merge_push_back( node_pointer p_nd ) noexcept
	{
		if ( p_nd == nullptr ) return;

		next_node_pointer p_cur = p_nd;
		next_node_pointer p_nxt = node_next_rw_t::read( p_cur );
		while ( p_nxt != nullptr ) {
			p_cur = p_nxt;
			p_nxt = node_next_rw_t::read( p_cur );
		}

		merge_push_back( p_nd, p_cur );
	}

	node_pointer pop_front( void ) noexcept
	{
		next_node_pointer p_ans = p_head_;
		if ( p_ans == nullptr ) return nullptr;

		p_head_ = node_next_rw_t::read( p_ans );

		node_next_rw_t::write( p_ans, nullptr );
		node_pointer p_tmp = dynamic_cast<node_pointer>( p_ans );
		if ( p_tmp == nullptr ) {
			LogOutput( log_type::ERR, "fail to dynamic_cast from next_node_pointer to node_pointer. this error caused by type mismatch implementation logic error" );
			delete p_ans;
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
			throw std::logic_error( "fail to dynamic_cast from next_node_pointer to node_pointer. this error caused by type mismatch implementation logic error" );
#endif
		}

		return p_tmp;
	}

	/**
	 * @brief if pred return true, that node is purged and push it into return value
	 *
	 * @tparam Predicate callable pred(const node_type&) and return bool
	 * @param pred callable pred(const node_type&) and return bool
	 * @return od_node_list_base purged nodes
	 */
	template <class Predicate>
	od_node_list_base_impl split_if( Predicate pred )
	{
		od_node_list_base_impl ans;

#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_TAIL_NODE_NEXT_NULLPTR
		if ( p_tail_ != nullptr ) {
			if ( node_next_rw_t::read( p_tail_ ) != nullptr ) {
				LogOutput( log_type::ERR, "tail node has non-nullptr in next" );
				std::terminate();
			}
		}
#endif

		next_node_pointer p_pre = nullptr;
		next_node_pointer p_cur = p_head_;
		while ( p_cur != nullptr ) {
			next_node_pointer  p_next = node_next_rw_t::read( p_cur );
			const node_pointer p_tmp  = dynamic_cast<const node_pointer>( p_cur );
			if ( p_tmp == nullptr ) {
				LogOutput( log_type::ERR, "fail to dynamic_cast from next_node_pointer to const node_pointer. this error caused by type mismatch implementation logic error" );
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
				throw std::logic_error( "fail to dynamic_cast from next_node_pointer to const node_pointer. this error caused by type mismatch implementation logic error" );
#endif
				p_pre = p_cur;
				p_cur = p_next;
			} else {
				if ( pred( *p_tmp ) ) {
					if ( p_pre == nullptr ) {
						p_head_ = p_next;
						if ( p_head_ == nullptr ) {
							p_tail_ = nullptr;
						}

						node_next_rw_t::write( p_cur, nullptr );
						ans.push_back( p_cur );

						p_cur = p_head_;
					} else {
						node_next_rw_t::write( p_pre, p_next );
						if ( p_next == nullptr ) {
							p_tail_ = p_pre;
						}

						node_next_rw_t::write( p_cur, nullptr );
						ans.push_back( p_cur );

						p_cur = p_next;
					}
				} else {
					p_pre = p_cur;
					p_cur = p_next;
				}
			}
		}

		return ans;
	}

	template <typename Pred>
	void clear( Pred pred )
	{
		next_node_pointer p_cur = p_head_;
		p_head_                 = nullptr;
		p_tail_                 = nullptr;
		while ( p_cur != nullptr ) {
			auto p_nxt = node_next_rw_t::read( p_cur );
			p_cur->clear_next();
			node_pointer p_tmp = dynamic_cast<node_pointer>( p_cur );
			if ( p_tmp != nullptr ) {
				pred( p_tmp );
			} else {
				LogOutput( log_type::ERR, "fail to dynamic_cast from next_node_pointer to node_pointer. this error caused by type mismatch implementation logic error" );
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
				throw std::logic_error( "fail to dynamic_cast from next_node_pointer to node_pointer. this error caused by type mismatch implementation logic error" );
#endif
			}
			p_cur = p_nxt;
		}
	}

	void clear( void )
	{
		clear( std::default_delete<node_type>() );
#if 0
		node_pointer p_cur = p_head_;
		p_head_            = nullptr;
		p_tail_            = nullptr;
		while ( p_cur != nullptr ) {
			node_pointer p_nxt = node_next_rw_t::read( p_cur );
			delete p_cur;
			p_cur = p_nxt;
		}
#endif
	}

	bool is_empty( void ) const
	{
		return p_head_ == nullptr;
	}

private:
	using next_node_pointer = typename NODE_T::next_node_pointer;

	void merge_push_front( next_node_pointer p_nd_head, next_node_pointer p_nd_tail ) noexcept
	{
		if ( p_head_ == nullptr ) {
			p_head_ = p_nd_head;
			p_tail_ = p_nd_tail;
		} else {
			node_next_rw_t::write( p_nd_tail, p_head_ );
			p_head_ = p_nd_head;
		}

#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_TAIL_NODE_NEXT_NULLPTR
		if ( p_tail_ != nullptr ) {
			if ( node_next_rw_t::read( p_tail_ ) != nullptr ) {
				LogOutput( log_type::ERR, "tail node has non-nullptr in next" );
				std::terminate();
			}
		}
#endif
	}

	void merge_push_back( next_node_pointer p_nd_head, next_node_pointer p_nd_tail ) noexcept
	{
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_TAIL_NODE_NEXT_NULLPTR
		if ( p_nd_tail != nullptr ) {
			if ( node_next_rw_t::read( p_nd_tail ) != nullptr ) {
				LogOutput( log_type::ERR, "tail node has non-nullptr in next" );
				std::terminate();
			}
		}
#endif

		if ( p_head_ == nullptr ) {
			p_head_ = p_nd_head;
			p_tail_ = p_nd_tail;
		} else {
			node_next_rw_t::write( p_tail_, p_nd_head );
			p_tail_ = p_nd_tail;
		}
	}

	next_node_pointer p_head_ = nullptr;
	next_node_pointer p_tail_ = nullptr;
};

/**
 * @brief list that is linked by hph_next_
 *
 * @warning this class is not thread safe
 *
 * @tparam NODE_T this type should be the derived class from od_node_base<NODE_T>
 */
template <typename NODE_T>
using od_node_list_base = od_node_list_base_impl<NODE_T, typename NODE_T::hph_next_rw>;

/**
 * @brief list that is linked by p_raw_next_
 *
 * @warning this class is not thread safe
 *
 * @tparam NODE_T this type should be the derived class from od_node_base<NODE_T>
 */
template <typename NODE_T>
using od_node_raw_list_base = od_node_list_base_impl<NODE_T, typename NODE_T::raw_next_rw>;

/**
 * @brief od_node list that supports exclusive control
 *
 */
template <typename LIST_T>
class od_node_list_lockable_base {
public:
	using list_type = LIST_T;

	class locker {
	public:
		locker( const locker& ) = delete;
		locker( locker&& src )
		  : reftarget_( src.reftarget_ )
		  , lg_( std::move( src.lg_ ) )
		{
		}
		locker& operator=( const locker& ) = delete;
		locker& operator=( locker&& src )
		{
			reftarget_ = src.reftarget_;
			lg_        = std::move( src.lg_ );
		}
		~locker() = default;

		bool owns_lock( void ) const noexcept
		{
			return lg_.owns_lock();
		}

		LIST_T& ref( void )
		{
			if ( !lg_.owns_lock() ) {
				throw std::logic_error( "no lock access is logic error" );
			}
			return reftarget_;
		}

		const LIST_T& ref( void ) const
		{
			if ( !lg_.owns_lock() ) {
				throw std::logic_error( "no lock access is logic error" );
			}
			return reftarget_;
		}

	private:
		locker( LIST_T& reftarget_arg, std::mutex& mtx_arg )
		  : reftarget_( reftarget_arg )
		  , lg_( mtx_arg )
		{
		}
		locker( LIST_T& reftarget_arg, std::mutex& mtx_arg, std::try_to_lock_t )
		  : reftarget_( reftarget_arg )
		  , lg_( mtx_arg, std::try_to_lock )
		{
		}

		LIST_T&                      reftarget_;
		std::unique_lock<std::mutex> lg_;

		friend class od_node_list_lockable_base;
	};

	constexpr od_node_list_lockable_base( void ) noexcept           = default;
	od_node_list_lockable_base( const od_node_list_lockable_base& ) = delete;
	constexpr od_node_list_lockable_base( od_node_list_lockable_base&& src ) noexcept
	  : list_data_( std::move( src.lock().ref() ) )
	  , mtx_()
	{
	}
	od_node_list_lockable_base&           operator=( const od_node_list_lockable_base& ) = delete;
	constexpr od_node_list_lockable_base& operator=( od_node_list_lockable_base&& src ) noexcept
	{
		// need to avoid the dead-lock
		LIST_T tmp( std::move( src.lock().ref() ) );
		lock().ref().swap( tmp );
	}
	~od_node_list_lockable_base() = default;

	locker lock( void )
	{
		return locker( list_data_, mtx_ );
	}
	locker try_lock( void )
	{
		return locker( list_data_, mtx_, std::try_to_lock );
	}

protected:
	std::mutex mtx_;
	LIST_T     list_data_;
};

template <typename LIST_T>
class od_node_list_conditional_lockable_base {
public:
	using list_type = LIST_T;

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
		locker& operator=( locker&& src )
		{
			reftarget_ = src.reftarget_;
			lg_        = std::move( src.lg_ );
			refcv_     = src.refcv_;
		}
		~locker() = default;

		bool owns_lock( void ) const noexcept
		{
			return lg_.owns_lock();
		}

		LIST_T& ref( void )
		{
			if ( !lg_.owns_lock() ) {
				throw std::logic_error( "no lock access is logic error" );
			}
			return reftarget_;
		}

		const LIST_T& ref( void ) const
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
			cv_.wait( lg_ );
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
		locker( LIST_T& reftarget_arg, std::mutex& mtx_arg, std::condition_variable& cv_arg )
		  : reftarget_( reftarget_arg )
		  , lg_( mtx_arg )
		  , refcv_( cv_arg )
		{
		}
		locker( LIST_T& reftarget_arg, std::mutex& mtx_arg, std::condition_variable& cv_arg, std::try_to_lock_t )
		  : reftarget_( reftarget_arg )
		  , lg_( mtx_arg, std::try_to_lock )
		  , refcv_( cv_arg )
		{
		}

		LIST_T&                      reftarget_;
		std::unique_lock<std::mutex> lg_;
		std::condition_variable&     refcv_;

		friend class od_node_list_conditional_lockable_base;
	};

	od_node_list_conditional_lockable_base( void ) noexcept                                 = default;
	od_node_list_conditional_lockable_base( const od_node_list_conditional_lockable_base& ) = delete;
	od_node_list_conditional_lockable_base( od_node_list_conditional_lockable_base&& src ) noexcept
	  : mtx_()
	  , cv_()
	  , list_data_( std::move( src.lock().ref() ) )
	{
	}
	od_node_list_conditional_lockable_base& operator=( const od_node_list_conditional_lockable_base& ) = delete;
	od_node_list_conditional_lockable_base& operator=( od_node_list_conditional_lockable_base&& src ) noexcept
	{
		// need to avoid the dead-lock
		LIST_T tmp( std::move( src.lock().ref() ) );
		lock().ref().swap( tmp );
	}
	~od_node_list_conditional_lockable_base() = default;

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
	LIST_T                  list_data_;
};

/**
 * @brief list that has hazard_ptr_handler of od_node<T> has header pointer
 *
 * this class support lock-free behavior
 *
 * @tparam NODE_T node type
 * @tparam NODE_T node type
 */
template <typename NODE_T>
class alignas( atomic_variable_align ) od_node_stack_lockfree_base {
	static_assert( std::is_base_of<typename NODE_T::next_node_type, NODE_T>::value, "NODE_T::next_node_type should be a base class of NODE_T." );

public:
	using node_type    = NODE_T;
	using node_pointer = NODE_T*;

	constexpr od_node_stack_lockfree_base( void ) noexcept            = default;
	od_node_stack_lockfree_base( const od_node_stack_lockfree_base& ) = delete;
	constexpr od_node_stack_lockfree_base( od_node_stack_lockfree_base&& src ) noexcept
	  : hph_head_( std::move( src.hph_head_ ) )
	{
	}
	od_node_stack_lockfree_base& operator=( const od_node_stack_lockfree_base& ) = delete;
	od_node_stack_lockfree_base& operator=( od_node_stack_lockfree_base&& src )  = delete;
	~od_node_stack_lockfree_base()
	{
		next_node_pointer p_cur = hph_head_.load();
		hph_head_.store( nullptr );
		while ( p_cur != nullptr ) {
			next_node_pointer p_nxt = p_cur->hph_next_.load();
			delete p_cur;
			p_cur = p_nxt;
		}
	}

	/**
	 * @brief ノードリストの先頭に、ノード一つを追加する
	 *
	 * @pre
	 * p_ndは、他スレッドからアクセスされない。
	 * 自インスタンスは、他スレッドからアクセスされない。
	 *
	 * @param p_nd
	 */
	void push_front( node_pointer p_nd ) noexcept
	{
		if ( p_nd == nullptr ) return;
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_PUSH_FRONT_FUNCTION_NULLPTR
		if ( p_nd->hph_next_.load() != nullptr ) {
			LogOutput( log_type::WARN, "od_node_list_lockfree::push_front() receives a od_node<T> that has non nullptr in hph_next_" );
		}
		if ( p_nd->p_raw_next_ != nullptr ) {
			LogOutput( log_type::WARN, "od_node_list_lockfree::push_front() receives a od_node<T> that has non nullptr in p_raw_next_" );
		}
#endif
		next_node_pointer p_typematch_node = p_nd;
		next_node_pointer p_expected       = hph_head_.load();
		p_nd->hph_next_.store( p_expected );
		while ( !hph_head_.compare_exchange_weak( p_expected, p_typematch_node, std::memory_order_release, std::memory_order_relaxed ) ) {
			p_nd->hph_next_.store( p_expected );
		}
	}

	/**
	 * @brief pop from front
	 *
	 * @warning because there is a possibility that a returned node is in hazard pointer, hazard_ptr_handler_t hph_next_ in returned node is not set nullptr.
	 *
	 * @return popped node_pointer. please see warning also.
	 */
	node_pointer pop_front( void ) noexcept
	{
		hazard_pointer    hp_cur_head = hph_head_.get();
		next_node_pointer p_expected  = hp_cur_head.get();
		if ( p_expected == nullptr ) return nullptr;

		next_node_pointer p_new_head = hp_cur_head->hph_next_.load( std::memory_order_acquire );
		while ( !hph_head_.compare_exchange_weak( p_expected, p_new_head, std::memory_order_release, std::memory_order_relaxed ) ) {
			hp_cur_head = hph_head_.get();
			p_expected  = hp_cur_head.get();
			if ( p_expected == nullptr ) {
				return nullptr;
			}
			p_new_head = hp_cur_head->hph_next_.load( std::memory_order_acquire );
		}

		// ここに来た時点で、hp_cur_head で保持されているノードの所有権を確保できた。
		// ※ 確保しているノードへのポインタを他スレッドでも持っているかもしれないが、
		//    メンバ変数 v_ を参照しないアルゴリズムになっているので、以降は参照してよい。
		//    hph_next_ は他スレッドで読みだされているため、書き換えてはならない。
		//    なお、hp_cur_headは、他スレッドでもハザードポインタとして登録中であるため、ハザードポインタとしての登録がなくなるまで破棄してはならない。
		next_node_pointer p_tmp = hp_cur_head.get();
		node_pointer      p_ans = dynamic_cast<node_pointer>( p_tmp );
		if ( p_ans == nullptr ) {
			LogOutput( log_type::ERR, "fail to dynamic_cast from next_node_pointer to node_pointer. this error caused by type mismatch implementation logic error" );
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
			throw std::logic_error( "fail to dynamic_cast from next_node_pointer to node_pointer. this error caused by type mismatch implementation logic error" );
#endif
		}

		return p_ans;
	}

private:
	using hazard_ptr_handler_t = typename NODE_T::hazard_ptr_handler_t;
	using hazard_pointer       = typename NODE_T::hazard_pointer;
	using next_node_type       = typename NODE_T::next_node_type;
	using next_node_pointer    = typename NODE_T::next_node_pointer;

	hazard_ptr_handler_t hph_head_;
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif /* ALCONCURRENT_INC_INTERNAL_OD_NODE_BASE_HPP_ */
