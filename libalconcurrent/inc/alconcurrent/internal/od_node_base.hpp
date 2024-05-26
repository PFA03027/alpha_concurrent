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

#ifndef ALCONCURRENT_INC_INTERNAL_OD_NODE_HPP_
#define ALCONCURRENT_INC_INTERNAL_OD_NODE_HPP_

#include <atomic>
#include <condition_variable>
#include <memory>
#include <type_traits>

#include "../hazard_ptr.hpp"
#include "../lf_mem_alloc.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

template <typename NODE_T>
class alignas( atomic_variable_align ) od_node_base {
public:
	using node_pointer         = NODE_T*;
	using hazard_ptr_handler_t = hazard_ptr_handler<NODE_T>;

	hazard_ptr_handler_t hph_next_;

	od_node_base( node_pointer p_next_arg = nullptr ) noexcept
	  : hph_next_( p_next_arg )
	{
	}

#ifdef ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
#else
#if 0   // too slow...
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
 * @brief list that has od_node<T> pointer has header pointer
 *
 * @warning this class is not thread safe
 *
 * @tparam T value type kept in od_node class
 */
template <typename NODE_T>
class alignas( atomic_variable_align ) od_node_list_base {
	static_assert( std::is_base_of<od_node_base<NODE_T>, NODE_T>::value, "NODE_T should be derived from od_node_base<>" );

public:
	using node_type    = NODE_T;
	using node_pointer = NODE_T*;

	constexpr od_node_list_base( void ) noexcept  = default;
	od_node_list_base( const od_node_list_base& ) = delete;
	constexpr od_node_list_base( od_node_list_base&& src ) noexcept
	  : p_head_( src.p_head_ )
	  , p_tail_( src.p_tail_ )
	{
		src.p_head_ = nullptr;
		src.p_tail_ = nullptr;
	}
	od_node_list_base&           operator=( const od_node_list_base& ) = delete;
	constexpr od_node_list_base& operator=( od_node_list_base&& src ) noexcept
	{
		od_node_list_base( std::move( src ) ).swap( *this );
		return *this;
	}
	~od_node_list_base()
	{
		clear();
	}

	void swap( od_node_list_base& src ) noexcept
	{
		node_pointer p_tmp;

		p_tmp       = p_head_;
		p_head_     = src.p_head_;
		src.p_head_ = p_tmp;

		p_tmp       = p_tail_;
		p_tail_     = src.p_tail_;
		src.p_tail_ = p_tmp;
	}

	void push_front( node_pointer p_nd ) noexcept
	{
		if ( p_nd == nullptr ) return;
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_PUSH_FRONT_FUNCTION_NULLPTR
		if ( p_nd->hph_next_.load() != nullptr ) {
			LogOutput( log_type::WARN, "od_node_list_base::push_front() receives a od_node<T> that has non nullptr in hph_next_" );
		}
#endif
		if ( p_head_ == nullptr ) {
			p_tail_ = p_nd;
		}
		p_nd->hph_next_.store( p_head_ );
		p_head_ = p_nd;
	}

	void push_back( node_pointer p_nd ) noexcept
	{
		if ( p_nd == nullptr ) return;
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_PUSH_FRONT_FUNCTION_NULLPTR
		if ( p_nd->hph_next_.load() != nullptr ) {
			LogOutput( log_type::WARN, "od_node_list_base::push_front() receives a od_node<T> that has non nullptr in hph_next_" );
			p_nd->hph_next_.store( nullptr );
		}
#endif
		if ( p_head_ == nullptr ) {
			p_head_ = p_nd;
			p_tail_ = p_nd;
		} else {
			p_tail_->hph_next_.store( p_nd );
			p_tail_ = p_nd;
		}
	}

	void merge_push_front( od_node_list_base&& src ) noexcept
	{
		if ( src.p_head_ == nullptr ) return;

		node_pointer p_src_head = src.p_head_;
		node_pointer p_src_tail = src.p_tail_;
		src.p_head_             = nullptr;
		src.p_tail_             = nullptr;

		merge_push_front( p_src_head, p_src_tail );
	}

	void merge_push_front( node_pointer p_nd ) noexcept
	{
		if ( p_nd == nullptr ) return;

		node_pointer p_cur = p_nd;
		node_pointer p_nxt = p_cur->hph_next_.load();
		while ( p_nxt != nullptr ) {
			p_cur = p_nxt;
			p_nxt = p_cur->hph_next_.load();
		}

		merge_push_front( p_nd, p_cur );
	}

	void merge_push_back( od_node_list_base&& src ) noexcept
	{
		if ( src.p_head_ == nullptr ) return;

		node_pointer p_src_head = src.p_head_;
		node_pointer p_src_tail = src.p_tail_;
		src.p_head_             = nullptr;
		src.p_tail_             = nullptr;

		merge_push_back( p_src_head, p_src_tail );
	}

	void merge_push_back( node_pointer p_nd ) noexcept
	{
		if ( p_nd == nullptr ) return;

		node_pointer p_cur = p_nd;
		node_pointer p_nxt = p_cur->hph_next_.load();
		while ( p_nxt != nullptr ) {
			p_cur = p_nxt;
			p_nxt = p_cur->hph_next_.load();
		}

		merge_push_back( p_nd, p_cur );
	}

	node_pointer pop_front( void ) noexcept
	{
		node_pointer p_ans = p_head_;
		if ( p_ans == nullptr ) return p_ans;

		p_head_ = p_ans->hph_next_.load();
		p_ans->hph_next_.store( nullptr );

		return p_ans;
	}

	/**
	 * @brief if pred return true, that node is purged and push it into return value
	 *
	 * @tparam Predicate callable pred(const node_type&) and return bool
	 * @param pred callable pred(const node_type&) and return bool
	 * @return od_node_list_base purged nodes
	 */
	template <class Predicate>
	od_node_list_base split_if( Predicate pred )
	{
		od_node_list_base ans;

		node_pointer p_pre = nullptr;
		node_pointer p_cur = p_head_;
		while ( p_cur != nullptr ) {
			node_pointer p_next = p_cur->hph_next_.load();
			if ( pred( *( reinterpret_cast<const node_pointer>( p_cur ) ) ) ) {
				if ( p_pre == nullptr ) {
					p_head_ = p_next;
					if ( p_head_ == nullptr ) {
						p_tail_ = nullptr;
					}

					p_cur->hph_next_.store( nullptr );
					ans.push_back( p_cur );

					p_cur = p_head_;
				} else {
					p_pre->hph_next_.store( p_next );
					if ( p_next == nullptr ) {
						p_tail_ = p_pre;
					}

					p_cur->hph_next_.store( nullptr );
					ans.push_back( p_cur );

					p_cur = p_next;
				}
			} else {
				p_pre = p_cur;
				p_cur = p_next;
			}
		}

		return ans;
	}

	void clear( void )
	{
		node_pointer p_cur = p_head_;
		p_head_            = nullptr;
		p_tail_            = nullptr;
		while ( p_cur != nullptr ) {
			node_pointer p_nxt = p_cur->hph_next_.load();
			delete p_cur;
			p_cur = p_nxt;
		}
	}

	bool is_empty( void ) const
	{
		return p_head_ == nullptr;
	}

private:
	void merge_push_front( node_pointer p_nd_head, node_pointer p_nd_tail ) noexcept
	{
		if ( p_head_ == nullptr ) {
			p_head_ = p_nd_head;
			p_tail_ = p_nd_tail;
		} else {
			p_nd_tail->hph_next_.store( p_head_ );
			p_head_ = p_nd_head;
		}
	}

	void merge_push_back( node_pointer p_nd_head, node_pointer p_nd_tail ) noexcept
	{
		if ( p_head_ == nullptr ) {
			p_head_ = p_nd_head;
			p_tail_ = p_nd_tail;
		} else {
			p_tail_->hph_next_.store( p_nd_head );
			p_tail_ = p_nd_tail;
		}
	}

	node_pointer p_head_ = nullptr;
	node_pointer p_tail_ = nullptr;
};

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
 * @tparam T value type kept in od_node class
 */
template <typename NODE_T>
class alignas( atomic_variable_align ) od_node_list_lockfree_base {
	static_assert( std::is_base_of<od_node_base<NODE_T>, NODE_T>::value, "NODE_T should be derived from od_node_base<>" );

public:
	using node_type    = NODE_T;
	using node_pointer = NODE_T*;

	constexpr od_node_list_lockfree_base( void ) noexcept           = default;
	od_node_list_lockfree_base( const od_node_list_lockfree_base& ) = delete;
	constexpr od_node_list_lockfree_base( od_node_list_lockfree_base&& src ) noexcept
	  : hph_head_( std::move( src.hph_head_ ) )
	{
	}
	od_node_list_lockfree_base& operator=( const od_node_list_lockfree_base& ) = delete;
	od_node_list_lockfree_base& operator=( od_node_list_lockfree_base&& src )  = delete;
	~od_node_list_lockfree_base()
	{
		node_pointer p_cur = hph_head_.load();
		hph_head_.store( nullptr );
		while ( p_cur != nullptr ) {
			node_pointer p_nxt = p_cur->hph_next_.load();
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
#endif
		node_pointer p_expected = hph_head_.load();
		p_nd->hph_next_.store( p_expected );
		while ( !hph_head_.compare_exchange_weak( p_expected, p_nd, std::memory_order_release, std::memory_order_relaxed ) ) {
			p_nd->hph_next_.store( p_expected );
		}
	}

	node_pointer pop_front( void ) noexcept
	{
		hazard_ptr<node_type> hp_cur_head = hph_head_.get();
		node_pointer          p_expected  = hp_cur_head.get();
		if ( p_expected == nullptr ) return nullptr;

		node_pointer p_new_head = hp_cur_head->hph_next_.load( std::memory_order_acquire );
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
		return hp_cur_head.get();
	}

private:
	using hazard_ptr_handler_t = typename NODE_T::hazard_ptr_handler_t;

	hazard_ptr_handler_t hph_head_;
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_LF_FIFO_HPP_ */
