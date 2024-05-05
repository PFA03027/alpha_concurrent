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
#include <memory>

#include "../hazard_ptr.hpp"
#include "../lf_mem_alloc.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

/**
 * @brief node of one direction list
 *
 * @tparam T value type kept in this class
 */
template <typename T>
struct alignas( atomic_variable_align ) od_node {
	using value_type           = T;
	using hazard_ptr_handler_t = hazard_ptr_handler<od_node>;

	hazard_ptr_handler<od_node> hph_next_;
	value_type                  v_;

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
template <typename T>
class od_node_list {
public:
	using node_type    = od_node<T>;
	using value_type   = typename od_node<T>::value_type;
	using node_pointer = od_node<T>*;

	constexpr od_node_list( void ) noexcept = default;
	explicit constexpr od_node_list( node_pointer p_head_arg ) noexcept
	  : p_head_( p_head_arg )
	{
	}
	od_node_list( const od_node_list& ) = delete;
	constexpr od_node_list( od_node_list&& src ) noexcept
	  : p_head_( src.p_head_ )
	{
		src.p_head_ = nullptr;
	}
	od_node_list&           operator=( const od_node_list& ) = delete;
	constexpr od_node_list& operator=( od_node_list&& src ) noexcept
	{
		od_node_list( std::move( src ) ).swap( *this );
	}
	~od_node_list()
	{
		node_pointer p_cur = p_head_;
		p_head_            = nullptr;
		while ( p_cur != nullptr ) {
			node_pointer p_nxt = p_cur->hph_next_.load();
			delete p_cur;
			p_cur = p_nxt;
		}
	}

	void swap( od_node_list& src ) noexcept
	{
		node_pointer p_tmp = p_head_;
		p_head_            = src.p_head_;
		src.p_head_        = p_tmp;
	}

	void push_front( node_pointer p_nd ) noexcept
	{
		if ( p_nd == nullptr ) return;
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_PUSH_FRONT_FUNCTION_NULLPTR
		if ( p_nd->hph_next_.load() != nullptr ) {
			LogOutput( log_type::WARN, "od_node_list::push_front() receives a od_node<T> that has non nullptr in hph_next_" );
		}
#endif
		p_nd->hph_next_.store( p_head_ );
		p_head_ = p_nd;
	}

	void merge_push_front( od_node_list& src ) noexcept
	{
		node_pointer p_src_head = src.p_head_;
		src.p_head_             = nullptr;

		merge_push_front( p_src_head );
	}

	void merge_push_front( od_node_list&& src ) noexcept
	{
		node_pointer p_src_head = src.p_head_;
		src.p_head_             = nullptr;

		merge_push_front( p_src_head );
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

		p_cur->hph_next_.store( p_head_ );
		p_head_ = p_nd;
	}

	node_pointer pop_front( void ) noexcept
	{
		node_pointer p_ans = p_head_;
		if ( p_ans == nullptr ) return p_ans;

		p_head_ = p_ans->hph_next_.load();
		p_ans->hph_next_.store( nullptr );

		return p_ans;
	}

private:
	node_pointer p_head_ = nullptr;
};

/**
 * @brief list that has hazard_ptr_handler of od_node<T> has header pointer
 *
 * this class support lock-free behavior
 *
 * @tparam T value type kept in od_node class
 */
template <typename T>
class od_node_list_lockfree {
public:
	using node_type    = od_node<T>;
	using value_type   = typename od_node<T>::value_type;
	using node_pointer = od_node<T>*;

	constexpr od_node_list_lockfree( void ) noexcept = default;
	explicit constexpr od_node_list_lockfree( node_pointer p_head_arg ) noexcept
	  : hph_head_( p_head_arg )
	{
	}
	od_node_list_lockfree( const od_node_list_lockfree& ) = delete;
	constexpr od_node_list_lockfree( od_node_list_lockfree&& src ) noexcept
	  : hph_head_( std::move( src.hph_head_ ) )
	{
	}
	od_node_list_lockfree& operator=( const od_node_list_lockfree& ) = delete;
	od_node_list_lockfree& operator=( od_node_list_lockfree&& src )  = delete;
	~od_node_list_lockfree()
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

	/**
	 * @brief p_ndからつながるノードリストを自インスタンスのノードリストの先頭に追加する。
	 *
	 * @param p_nd
	 */
	void merge_push_front( node_pointer p_nd ) noexcept
	{
		if ( p_nd == nullptr ) return;

		node_pointer p_last = p_nd;
		node_pointer p_next = p_last->hph_next_.load();
		while ( p_next != nullptr ) {
			p_last = p_next;
			p_next = p_last->hph_next_.load();
		}

		node_pointer p_expected = hph_head_.load();
		p_last->hph_next_.store( p_expected );
		while ( !hph_head_.compare_exchange_weak( p_expected, p_nd, std::memory_order_release, std::memory_order_relaxed ) ) {
			p_last->hph_next_.store( p_expected );
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
	using hazard_ptr_handler_t = typename od_node<T>::hazard_ptr_handler_t;

	hazard_ptr_handler_t hph_head_;
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_LF_FIFO_HPP_ */
