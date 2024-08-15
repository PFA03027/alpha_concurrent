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
#include <exception>
#include <memory>
#include <type_traits>

#include "alconcurrent/hazard_ptr.hpp"
#include "alconcurrent/lf_mem_alloc.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

template <typename TO_POINTER_T, typename FROM_POINTER_T>
inline TO_POINTER_T safe_static_pointer_down_cast( FROM_POINTER_T p )
{
	static_assert( std::is_pointer<FROM_POINTER_T>::value, "p should be pointer type" );
	static_assert( std::is_pointer<TO_POINTER_T>::value, "TO_POINTER_T should be pointer type" );
	using to_type   = typename std::remove_pointer<TO_POINTER_T>::type;
	using from_type = typename std::remove_pointer<decltype( p )>::type;
	static_assert( std::is_base_of<from_type, to_type>::value, "std::remove_pointer<decltype( p )>::type should be the base class of std::remove_pointer<TO_POINTER_T>::type." );

	return static_cast<TO_POINTER_T>( p );
}

template <typename NODE_T>
class alignas( atomic_variable_align ) od_node_base_if {
protected:
	using node_type    = NODE_T;
	using node_pointer = NODE_T*;

	node_pointer next( void ) const noexcept
	{
		return this->NODE_T::next();
	}

	void set_next( node_pointer p_n ) noexcept
	{
		this->NODE_T::set_next( p_n );
	}

public:
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

template <typename NODE_T>
class alignas( atomic_variable_align ) od_node_base_raw_next : public od_node_base_if<od_node_base_raw_next<NODE_T>> {
public:
	using base_node_if_type         = od_node_base_if<od_node_base_raw_next<NODE_T>>;
	using base_node_if_pointer      = od_node_base_if<od_node_base_raw_next<NODE_T>>*;
	using base_node_if_crtp_type    = typename base_node_if_type::node_type;      // == typename std::remove_reference<decltype(*this)>::type
	using base_node_if_crtp_pointer = typename base_node_if_type::node_pointer;   // == decltype(this)
	using node_type                 = NODE_T;
	using node_pointer              = NODE_T*;

	explicit od_node_base_raw_next( node_pointer p_next_arg = nullptr ) noexcept
	  : p_raw_next_( p_next_arg )
	{
	}
	od_node_base_raw_next( const od_node_base_raw_next& src ) noexcept
	  : p_raw_next_( src.p_raw_next_ )
	{
	}
	od_node_base_raw_next( od_node_base_raw_next&& src ) noexcept
	  : p_raw_next_( src.p_raw_next_ )
	{
		src.p_raw_next_ = nullptr;
	}
	od_node_base_raw_next& operator=( const od_node_base_raw_next& src ) noexcept
	{
		if ( this == &src ) return *this;
		p_raw_next_ = src.p_raw_next_;
		return *this;
	}
	od_node_base_raw_next& operator=( od_node_base_raw_next&& src ) noexcept
	{
		if ( this == &src ) return *this;
		p_raw_next_     = src.p_raw_next_;
		src.p_raw_next_ = nullptr;
		return *this;
	}

	node_pointer next( void ) const noexcept
	{
		return p_raw_next_;
	}

	void set_next( node_pointer p_n ) noexcept
	{
		p_raw_next_ = p_n;
	}

private:
	node_pointer p_raw_next_;
};

template <typename NODE_T>
class alignas( atomic_variable_align ) od_node_base_hazard_handler_next : public od_node_base_if<od_node_base_hazard_handler_next<NODE_T>> {
public:
	using base_node_if_type         = od_node_base_if<od_node_base_hazard_handler_next<NODE_T>>;
	using base_node_if_pointer      = od_node_base_if<od_node_base_hazard_handler_next<NODE_T>>*;
	using base_node_if_crtp_type    = typename base_node_if_type::node_type;      // == typename std::remove_reference<decltype(*this)>::type
	using base_node_if_crtp_pointer = typename base_node_if_type::node_pointer;   // == decltype(this)
	using node_type                 = NODE_T;
	using node_pointer              = NODE_T*;
	using hazard_ptr_handler_t      = hazard_ptr_handler<node_type>;
	using hazard_pointer            = typename hazard_ptr_handler<node_type>::hazard_pointer;

	explicit od_node_base_hazard_handler_next( node_pointer p_next_arg = nullptr ) noexcept
	  : hph_next_( p_next_arg )
	{
	}
	od_node_base_hazard_handler_next( const od_node_base_hazard_handler_next& )            = default;
	od_node_base_hazard_handler_next( od_node_base_hazard_handler_next&& )                 = default;
	od_node_base_hazard_handler_next& operator=( const od_node_base_hazard_handler_next& ) = default;
	od_node_base_hazard_handler_next& operator=( od_node_base_hazard_handler_next&& )      = default;

	node_pointer next( void ) const noexcept
	{
		return hph_next_.load();
	}

	void set_next( node_pointer p_n ) noexcept
	{
		hph_next_.store( p_n );
	}

	hazard_pointer get_hazard_ptr_of_next( void )
	{
		return hph_next_.get();
	}

	hazard_ptr_handler_t& hazard_handler( void ) noexcept
	{
		return hph_next_;
	}

	const hazard_ptr_handler_t& hazard_handler( void ) const noexcept
	{
		return hph_next_;
	}

private:
	hazard_ptr_handler_t hph_next_;
};

template <typename NODE_T>
class alignas( atomic_variable_align ) od_node_base : public od_node_base_hazard_handler_next<NODE_T>, public od_node_base_raw_next<NODE_T> {
public:
	using node_type                          = NODE_T;
	using node_pointer                       = NODE_T*;
	using hazard_ptr_handler_t               = typename od_node_base_hazard_handler_next<NODE_T>::hazard_ptr_handler_t;
	using hazard_pointer                     = typename od_node_base_hazard_handler_next<NODE_T>::hazard_pointer;
	using od_node_base_raw_next_t            = od_node_base_raw_next<NODE_T>;
	using od_node_base_hazard_handler_next_t = od_node_base_hazard_handler_next<NODE_T>;

	od_node_base( node_pointer p_next_arg = nullptr ) noexcept
	  : od_node_base_hazard_handler_next<NODE_T>( p_next_arg )
	  , od_node_base_raw_next<NODE_T>( nullptr )
	{
	}

	void clear_next( void )
	{
		od_node_base_raw_next<NODE_T>::set_next( nullptr );
		od_node_base_hazard_handler_next<NODE_T>::set_next( nullptr );
	}
};

/**
 * @brief list implementation class that has od_node<T> pointer has header pointer
 *
 * @warning this class is not thread safe
 *
 * @tparam NODE_T this type should be the derived class from od_node_base<NODE_T>
 * @tparam NEXT_HANDLING_T this type should be one of the base type of NODE_T
 */
template <typename NODE_T, typename LINK_T>
class alignas( atomic_variable_align ) od_node_list_base_impl {
	static_assert( std::is_base_of<LINK_T, NODE_T>::value, "NODE_T should be a derived class of LINK_T" );

public:
	using node_type    = NODE_T;
	using node_pointer = NODE_T*;

	constexpr od_node_list_base_impl( void ) noexcept       = default;
	od_node_list_base_impl( const od_node_list_base_impl& ) = delete;
	constexpr od_node_list_base_impl( od_node_list_base_impl&& src ) noexcept
	  : p_head_( src.p_head_ )
	  , p_tail_( src.p_tail_ )
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	  , count_( src.count_ )
#endif
	{
		src.p_head_ = nullptr;
		src.p_tail_ = nullptr;
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		src.count_ = 0;
#endif

#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_TAIL_NODE_NEXT_NULLPTR
		if ( p_tail_ != nullptr ) {
			if ( p_tail_->next() != nullptr ) {
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
			if ( p_tail_->next() != nullptr ) {
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
		link_node_pointer p_tmp;

		p_tmp       = p_head_;
		p_head_     = src.p_head_;
		src.p_head_ = p_tmp;

		p_tmp       = p_tail_;
		p_tail_     = src.p_tail_;
		src.p_tail_ = p_tmp;

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		size_t tmp_cnt;
		tmp_cnt    = count_;
		count_     = src.count_;
		src.count_ = tmp_cnt;
#endif

#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_TAIL_NODE_NEXT_NULLPTR
		if ( p_tail_ != nullptr ) {
			if ( p_tail_->next() != nullptr ) {
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
		if ( p_nd->next() != nullptr ) {
			LogOutput( log_type::WARN, "od_node_list_base_impl::push_front() receives a od_node<T> that has non nullptr in next" );
			p_nd->set_next( nullptr );
		}
#endif
		if ( p_head_ == nullptr ) {
			p_tail_ = p_nd;
		}
		link_node_pointer p_nd_link_t = p_nd;
		p_nd_link_t->set_next( safe_static_pointer_down_cast<node_pointer>( p_head_ ) );
		p_head_ = p_nd_link_t;

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		count_++;
#endif
	}

	void push_back( node_pointer p_nd ) noexcept
	{
		if ( p_nd == nullptr ) return;
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_PUSH_FRONT_FUNCTION_NULLPTR
		if ( p_nd->next() != nullptr ) {
			LogOutput( log_type::WARN, "od_node_list_base_impl::push_back() receives a od_node<T> that has non nullptr in next" );
			p_nd->set_next( nullptr );
		}
#endif
		if ( p_head_ == nullptr ) {
			p_head_ = p_nd;
			p_tail_ = p_nd;
		} else {
			p_tail_->set_next( p_nd );
			p_tail_ = p_nd;
		}

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		count_++;
#endif
	}

	void merge_push_front( od_node_list_base_impl&& src ) noexcept
	{
		if ( src.p_head_ == nullptr ) return;

		link_node_pointer p_src_head = src.p_head_;
		link_node_pointer p_src_tail = src.p_tail_;
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		size_t tmp_cnt = src.count_;
		src.count_     = 0;
#endif
		src.p_head_ = nullptr;
		src.p_tail_ = nullptr;

		merge_push_front( p_src_head, p_src_tail );
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		count_ += tmp_cnt;
#endif
	}

	void merge_push_front( node_pointer p_nd ) noexcept
	{
		if ( p_nd == nullptr ) return;

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		size_t tmp_cnt = 0;
#endif
		link_node_pointer p_cur = p_nd;
		link_node_pointer p_nxt = p_cur->next();
		while ( p_nxt != nullptr ) {
			p_cur = p_nxt;
			p_nxt = p_cur->next();
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			tmp_cnt++;
#endif
		}

		merge_push_front( p_nd, p_cur );
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		count_ += tmp_cnt;
#endif
	}

	void merge_push_back( od_node_list_base_impl&& src ) noexcept
	{
		if ( src.p_head_ == nullptr ) return;

		link_node_pointer p_src_head = src.p_head_;
		link_node_pointer p_src_tail = src.p_tail_;
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		size_t tmp_cnt = src.count_;
		src.count_     = 0;
#endif
		src.p_head_ = nullptr;
		src.p_tail_ = nullptr;

		merge_push_back( p_src_head, p_src_tail );
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		count_ += tmp_cnt;
#endif
	}

	void merge_push_back( node_pointer p_nd ) noexcept
	{
		if ( p_nd == nullptr ) return;

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		size_t tmp_cnt = 0;
#endif
		link_node_pointer p_cur = p_nd;
		link_node_pointer p_nxt = p_cur->next();
		while ( p_nxt != nullptr ) {
			p_cur = p_nxt;
			p_nxt = p_cur->next();
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			tmp_cnt++;
#endif
		}

		merge_push_back( p_nd, p_cur );
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		count_ += tmp_cnt;
#endif
	}

	node_pointer pop_front( void ) noexcept
	{
		link_node_pointer p_ans = p_head_;
		if ( p_ans == nullptr ) return nullptr;

		p_head_ = p_ans->next();
		if ( p_head_ == nullptr ) {
			p_tail_ = nullptr;
		}

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		count_--;
#endif

		p_ans->set_next( nullptr );
		return safe_static_pointer_down_cast<node_pointer>( p_ans );
	}

	/**
	 * @brief if pred return true, that node is purged and push it into return value
	 *
	 * @tparam Predicate callable pred(const node_type&) and return bool
	 * @param pred callable pred(const node_type&) and return bool
	 * @return od_node_list_base_impl purged nodes
	 */
	template <class Predicate>
	od_node_list_base_impl split_if( Predicate pred )
	{
		od_node_list_base_impl ans;

#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_TAIL_NODE_NEXT_NULLPTR
		if ( p_tail_ != nullptr ) {
			if ( p_tail_->next() != nullptr ) {
				LogOutput( log_type::ERR, "tail node has non-nullptr in next" );
				std::terminate();
			}
		}
#endif

		link_node_pointer p_pre = nullptr;
		link_node_pointer p_cur = p_head_;
		while ( p_cur != nullptr ) {
			link_node_pointer p_next           = p_cur->next();
			node_pointer      p_next_type_node = safe_static_pointer_down_cast<node_pointer>( p_next );   // this cast is OK, because LINK_T is a base class of NODE_T checked by static_assert in the top of class definition.
			node_pointer      p_cur_type_node  = safe_static_pointer_down_cast<node_pointer>( p_cur );    // this cast is OK, because LINK_T is a base class of NODE_T checked by static_assert in the top of class definition.
			if ( pred( *( static_cast<const node_pointer>( p_cur_type_node ) ) ) ) {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
				count_--;
#endif
				if ( p_pre == nullptr ) {
					p_head_ = p_next;
					if ( p_head_ == nullptr ) {
						p_tail_ = nullptr;
					}

					p_cur->set_next( nullptr );
					ans.push_back( p_cur_type_node );

					p_cur = p_head_;
				} else {
					p_pre->set_next( p_next_type_node );
					if ( p_next == nullptr ) {
						p_tail_ = p_pre;
					}

					p_cur->set_next( nullptr );
					ans.push_back( p_cur_type_node );

					p_cur = p_next;
				}
			} else {
				p_pre = p_cur;
				p_cur = p_next;
			}
		}

		return ans;
	}

	/**
	 * @brief purge all nodes and apply Pred
	 *
	 * Pred becomes an onwer of a node. therefore Pred should not leak the memory of a node pointer
	 *
	 * @tparam Pred Predicate callable pred(const node_type&) and return bool
	 * @param pred callable object like pred(const node_type&) and return bool
	 */
	template <typename Pred>
	void clear( Pred pred )
	{
		link_node_pointer p_cur = p_head_;
		p_head_                 = nullptr;
		p_tail_                 = nullptr;
		while ( p_cur != nullptr ) {
			link_node_pointer p_nxt = p_cur->next();
			p_cur->set_next( nullptr );
			node_pointer p_cur_type_node = safe_static_pointer_down_cast<node_pointer>( p_cur );   // this cast is OK, because LINK_T is a base class of NODE_T checked by static_assert in the top of class definition.
			pred( p_cur_type_node );
			p_cur = p_nxt;
		}

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		count_ = 0;
#endif
	}

	void clear( void )
	{
		clear( std::default_delete<node_type>() );
	}

	bool is_empty( void ) const
	{
		return p_head_ == nullptr;
	}

	size_t profile_info_count( void ) const
	{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
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
		return count_;
#else
		return 0;
#endif
	}

private:
	using link_handler_type = LINK_T;
	using link_node_pointer = LINK_T*;

	void merge_push_front( link_node_pointer p_nd_head, link_node_pointer p_nd_tail ) noexcept
	{
		if ( p_head_ == nullptr ) {
			p_head_ = p_nd_head;
			p_tail_ = p_nd_tail;
		} else {
			p_nd_tail->set_next( safe_static_pointer_down_cast<node_pointer>( p_head_ ) );
			p_head_ = p_nd_head;
		}

#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_TAIL_NODE_NEXT_NULLPTR
		if ( p_tail_ != nullptr ) {
			if ( p_tail_->next() != nullptr ) {
				LogOutput( log_type::ERR, "tail node has non-nullptr in next" );
				std::terminate();
			}
		}
#endif
	}

	void merge_push_back( link_node_pointer p_nd_head, link_node_pointer p_nd_tail ) noexcept
	{
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_TAIL_NODE_NEXT_NULLPTR
		if ( p_nd_tail != nullptr ) {
			if ( p_nd_tail->next() != nullptr ) {
				LogOutput( log_type::ERR, "tail node has non-nullptr in next" );
				std::terminate();
			}
		}
#endif

		if ( p_head_ == nullptr ) {
			p_head_ = p_nd_head;
			p_tail_ = p_nd_tail;
		} else {
			p_tail_->set_next( safe_static_pointer_down_cast<node_pointer>( p_nd_head ) );
			p_tail_ = p_nd_tail;
		}
	}

	link_node_pointer p_head_ = nullptr;
	link_node_pointer p_tail_ = nullptr;
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	size_t count_ = 0;
#endif
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
 * @tparam NODE_T node type
 */
template <typename NODE_T, typename LINK_T>
class alignas( atomic_variable_align ) od_node_stack_lockfree_base {
	static_assert( std::is_base_of<LINK_T, NODE_T>::value, "NODE_T should be a derived class of LINK_T" );

public:
	using node_type    = NODE_T;
	using node_pointer = NODE_T*;

	constexpr od_node_stack_lockfree_base( void ) noexcept
	  : hph_head_()
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	  , count_( 0 )
#endif
	{
	}
	od_node_stack_lockfree_base( const od_node_stack_lockfree_base& ) = delete;
	constexpr od_node_stack_lockfree_base( od_node_stack_lockfree_base&& src ) noexcept
	  : hph_head_( std::move( src.hph_head_ ) )
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	  , count_( src.count_.load( std::memory_order_acquire ) )
#endif
	{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		src.count_.store( 0, std::memory_order_release );
#endif
	}
	od_node_stack_lockfree_base& operator=( const od_node_stack_lockfree_base& ) = delete;
	od_node_stack_lockfree_base& operator=( od_node_stack_lockfree_base&& src )  = delete;
	~od_node_stack_lockfree_base()
	{
		link_node_pointer p_cur = hph_head_.load();
		hph_head_.store( nullptr );
		while ( p_cur != nullptr ) {
			link_node_pointer p_nxt = p_cur->next();
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
		if ( p_nd->next() != nullptr ) {
			LogOutput( log_type::WARN, "od_node_list_lockfree::push_front() receives a od_node<T> that has non nullptr in hph_next_" );
		}
#endif
		link_node_pointer p_nd_link_p = p_nd;
		node_pointer      p_expected  = hph_head_.load();
		p_nd_link_p->set_next( p_expected );
		while ( !hph_head_.compare_exchange_weak( p_expected, p_nd, std::memory_order_release, std::memory_order_relaxed ) ) {
			p_nd_link_p->set_next( p_expected );
		}
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		count_++;
#endif
	}

	/**
	 * @brief pop from front
	 *
	 * @warning because there is a possibility that a returned node is in hazard pointer, hazard_ptr_handler_t hph_next_ in returned node is not set nullptr.
	 * And should not write not only nullptr but also any next pointer information during return pointer is in hazard pointer.
	 *
	 * @return popped node_pointer. please see warning also.
	 */
	node_pointer pop_front( void ) noexcept
	{
		hazard_pointer hp_cur_head = hph_head_.get();
		node_pointer   p_expected  = hp_cur_head.get();
		if ( p_expected == nullptr ) return nullptr;
		link_node_pointer p_expected_link_t = static_cast<link_node_pointer>( p_expected );

		link_node_pointer p_new_head_link_t = p_expected_link_t->next();
		node_pointer      p_new_head        = safe_static_pointer_down_cast<node_pointer>( p_new_head_link_t );
		while ( !hph_head_.compare_exchange_weak( p_expected, p_new_head, std::memory_order_release, std::memory_order_relaxed ) ) {
			hp_cur_head = hph_head_.get();
			p_expected  = hp_cur_head.get();
			if ( p_expected == nullptr ) {
				return nullptr;
			}
			p_expected_link_t = static_cast<link_node_pointer>( p_expected );

			p_new_head_link_t = p_expected_link_t->next();
			p_new_head        = safe_static_pointer_down_cast<node_pointer>( p_new_head_link_t );
		}

		// ここに来た時点で、hp_cur_head で保持されているノードの所有権を確保できた。
		// ※ 確保しているノードへのポインタを他スレッドでも持っているかもしれないが、
		//    メンバ変数 v_ を参照しないアルゴリズムになっているので、以降は参照してよい。
		//    hph_next_ は他スレッドで読みだされているため、書き換えてはならない。
		//    なお、hp_cur_headは、他スレッドでもハザードポインタとして登録中であるため、ハザードポインタとしての登録がなくなるまで破棄してはならない。
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		count_--;
#endif
		return hp_cur_head.get();
	}

	size_t profile_info_count( void ) const
	{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		return count_.load( std::memory_order_acquire );
#else
		return 0;
#endif
	}

private:
	using link_handler_type    = LINK_T;
	using link_node_pointer    = LINK_T*;
	using hazard_ptr_handler_t = typename LINK_T::hazard_ptr_handler_t;
	using hazard_pointer       = typename LINK_T::hazard_pointer;

	hazard_ptr_handler_t hph_head_;
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	std::atomic<size_t> count_;
#endif
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif /* ALCONCURRENT_INC_INTERNAL_OD_NODE_BASE_HPP_ */
