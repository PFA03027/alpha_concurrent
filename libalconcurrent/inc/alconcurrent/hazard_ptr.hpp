/*!
 * @file	hazard_ptr.hpp
 * @brief	hazard pointer
 * @author	Teruaki Ata
 * @date	Created on 2020/12/24
 * @details
 *
 *  US Patent of Hazard pointer algrithm: US20040107227A1 is now abandoned.
 *  https://patents.google.com/patent/US20040107227
 *
 * Copyright (C) 2020 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef ALCONCCURRENT_INC_HAZARD_PTR_HPP_
#define ALCONCCURRENT_INC_HAZARD_PTR_HPP_

#include <pthread.h>

#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>

#include "dynamic_tls.hpp"
#include "internal/alloc_only_allocator.hpp"
#include "internal/hazard_ptr_internal.hpp"

namespace alpha {
namespace concurrent {

template <typename T, int N>
class hazard_ptr_scoped_ref;

/*!
 * @brief	hazard pointer support class
 *
 * 削除権が、１つのインスタンスのみに制約できる場合に使用できるハザードポインタ。
 * 参照権に対しては、制限なし。ただし、参照が終了した場合は、ハザードポインタをクリアしなければならない。
 * 参照権のクリアには、clear_hazard_ptr()を使用する。
 *
 * （ハザードポインタ自身はlock-freeであるが）lock-freeとして動作するには、テンプレートパラメータで指定されたクラスのデストラクタがlock-freeでなければならない。
 *
 * インスタンスが削除される前に、アクセスするスレッドは、参照権の破棄としてrelease_owner()を呼び出すこと。
 *
 */
template <typename T, int N>
class hazard_ptr_storage {
public:
	using hzrd_type                       = T;
	using hzrd_pointer                    = T*;
	static constexpr size_t hzrd_max_slot = N;
	static_assert( N > 0, "N should be greater than 0(zero)" );

	constexpr hazard_ptr_storage( void )
	  : my_allocator_( true, 4 * 1024 )
	  , p_allocator_( &my_allocator_ )
	  , head_( p_allocator_ )
	  , p_hzd_ptr_node_( threadlocal_handler_functor( this ) )
	{
		static_assert( std::is_standard_layout<hazard_ptr_storage>::value, "hazard_ptr_storage should be standard-layout type" );
	}

	constexpr hazard_ptr_storage( internal::alloc_only_chamber* p_allocator_arg )
	  : my_allocator_( true, 0 )
	  , p_allocator_( p_allocator_arg )
	  , head_( p_allocator_ )
	  , p_hzd_ptr_node_( threadlocal_handler_functor( this ) )
	{
		static_assert( std::is_standard_layout<hazard_ptr_storage>::value, "hazard_ptr_storage should be standard-layout type" );
	}

	~hazard_ptr_storage( void )
	{
	}

	/*!
	 * @brief	Check whether a pointer is in this hazard list
	 *
	 * @retval	true	p_chk_ptr is in this hazard list.
	 * @retval	false	p_chk_ptr is not in this hazard list.
	 */
	bool check_ptr_in_hazard_list( T* p_chk_ptr )
	{
		return get_head_instance().check_ptr_in_hazard_list( p_chk_ptr );
	}

	int debug_get_glist_size( void )
	{
		return get_head_instance().get_node_count();
	}

	void dump_to_log( log_type lt, char c, int id )
	{
		head_.dump_to_log( lt, c, id );
	}

private:
	////////////////////////////////////////////////////////////////////////////////
	enum class ocupied_status : int {
		UNUSED,
		USING
	};

	/*!
	 * @brief	ハザードポインタとしてキープするためのノード。
	 *
	 * スレッド毎に確保され、使用権が占有される。
	 *
	 * @note
	 * このノードの集合を管理するリストは、ノード自体を削除することを行わない設計となっている。
	 *
	 */
	class node_for_hazard_ptr {
	public:
		node_for_hazard_ptr( void )
		  : status_( ocupied_status::USING )
		  , next_( nullptr )
		{
			clear_hazard_ptr_all();
		}

		/*!
		 * @brief	Make a reservation to acquire the reference right.
		 *
		 * 参照権を獲得予約をするために、ハザードポインタに登録する。
		 *
		 * 本I/Fを呼び出しただけでは、参照権を獲得したことにはならない。 @n
		 * 本I/Fを呼び出した後に、登録したポインタがまだ有効であるかどうかを確認すること。 @n
		 * @li	確認の結果、有効である場合は、参照権を獲得したとみなせる。
		 * @li	確認の結果、有効ではなかった場合、clear_hazard_ptr()を呼び出すか、新しいポインタを新たなハザードポインタとして本I/Fで再設定し、現在保持しているハザードポインタを解放すること。
		 *
		 * @note
		 * 確実なclear_hazard_ptr()呼び出しをサポートするクラスとして、 hazard_ptr_scoped_ref<T> を用意している。
		 */
		void set_hazard_ptr( T* p_target_arg, int idx )
		{
			p_target_[idx].store( p_target_arg, std::memory_order_release );
			return;
		}

		/*!
		 * @brief	Release a reservation to acquire the reference right or the reference right itself.
		 *
		 * @note
		 * 確実なclear_hazard_ptr()呼び出しをサポートするクラスとして、 hazard_ptr_scoped_ref<T> を用意している。
		 */
		void clear_hazard_ptr( int idx )
		{
			p_target_[idx].store( nullptr, std::memory_order_release );
			return;
		}

		void clear_hazard_ptr_all( void )
		{
			for ( auto& e : p_target_ ) {
				e.store( nullptr, std::memory_order_release );
			}
			return;
		}

		/*!
		 * @brief	Check whether a pointer is in this hazard
		 *
		 * @warning before calling, caller should be check get_status() == ocupied_status::USED
		 *
		 * @retval	true	p_chk_ptr is in this hazard.
		 * @retval	false	p_chk_ptr is not in this hazard.
		 */
		inline bool check_hazard_ptr_in_using_node( T* p_chk_ptr )
		{
			for ( auto& e : p_target_ ) {
				if ( e.load( std::memory_order_acquire ) == p_chk_ptr ) return true;
			}

			return false;
		}

		node_for_hazard_ptr* get_next( void )
		{
			return next_.load( std::memory_order_acquire );
		}

		void set_next( node_for_hazard_ptr* p_new_next )
		{
			next_.store( p_new_next, std::memory_order_release );
			return;
		}

		bool next_CAS( node_for_hazard_ptr** pp_expect_ptr, node_for_hazard_ptr* p_desired_ptr )
		{
			return next_.compare_exchange_weak( *pp_expect_ptr, p_desired_ptr );
		}

		void release_owner( void )
		{
			clear_hazard_ptr_all();
			status_.store( ocupied_status::UNUSED, std::memory_order_release );
			return;
		}

		bool try_to_get_owner( void )
		{
			ocupied_status cur;
			cur = get_status();
			if ( cur != ocupied_status::UNUSED ) {
				return false;
			}

			return status_.compare_exchange_strong( cur, ocupied_status::USING, std::memory_order_acq_rel );
		}

		ocupied_status get_status( void )
		{
			return status_.load( std::memory_order_acquire );
		}

		void* operator new( std::size_t n )           = delete;   // usual new...(1)
		void  operator delete( void* p_mem ) noexcept = delete;   // usual delete...(2)	dynamic_tls_content_arrayは、破棄しないクラスなので、メモリ開放しない

		void* operator new[]( std::size_t n )           = delete;   // usual new...(1)
		void  operator delete[]( void* p_mem ) noexcept = delete;   // usual delete...(2)	dynamic_tls_content_arrayは、破棄しないクラスなので、メモリ開放しない

		void* operator new( std::size_t n, internal::alloc_only_chamber& allocator_arg )   // placement new
		{
			return allocator_arg.allocate<sizeof( uintptr_t )>( n );
		}
		void operator delete( void* p, internal::alloc_only_chamber& allocator_arg ) noexcept   // placement delete...(3)
		{
		}

	private:
		std::atomic<ocupied_status>       status_;                                           //!< status for used or not used yet
		std::atomic<node_for_hazard_ptr*> next_;                                             //!< pointer to next node
		std::atomic<T*>                   p_target_[static_cast<size_t>( hzrd_max_slot )];   //!< hazard pointer strage
	};

	struct hazard_node_head {
		constexpr hazard_node_head( internal::alloc_only_chamber* p_allocator_arg )
		  : p_allocator_( p_allocator_arg )
		  , head_( nullptr )
		  , node_count_( 0 )
		{
			static_assert( std::is_standard_layout<hazard_node_head>::value, "hazard_node_head should be standard-layout type" );
		}
		~hazard_node_head()
		{
			node_for_hazard_ptr* p_cur = head_.load( std::memory_order_acquire );
			while ( p_cur != nullptr ) {
				node_for_hazard_ptr* p_nxt = p_cur->get_next();
				node_for_hazard_ptr::operator delete( p_cur, *p_allocator_ );   // 通常のdelete operatorではなく配置deleteを呼び出すために、operator deleteを直接呼び出す。
				p_cur = p_nxt;
			}
			return;
		}

		node_for_hazard_ptr* allocate_hazard_ptr_node( void )
		{
			// 空きノードを探す。
			node_for_hazard_ptr* p_ans = head_.load( std::memory_order_acquire );
			while ( p_ans != nullptr ) {
				if ( p_ans->get_status() == ocupied_status::UNUSED ) {
					if ( p_ans->try_to_get_owner() ) {
						internal::LogOutput( log_type::DEBUG, "node is allocated." );
						return p_ans;
					}
				}
				p_ans = p_ans->get_next();
			}

			// 空きノードが見つからなかったので、新しいノードを用意する。
			p_ans = add_one_new_hazard_ptr_node();

			internal::LogOutput( log_type::DEBUG, "glist is added by allocate_hazard_ptr_node(%p)", p_ans );
			return p_ans;
		}

		/*!
		 * @brief	Check whether a pointer is in this hazard list
		 *
		 * @retval	true	p_chk_ptr is in this hazard list.
		 * @retval	false	p_chk_ptr is not in this hazard list.
		 */
		bool check_ptr_in_hazard_list( T* p_chk_ptr )
		{
			node_for_hazard_ptr* p_ans = head_.load( std::memory_order_acquire );
			while ( p_ans != nullptr ) {
				if ( p_ans->get_status() == ocupied_status::USING ) {
					if ( p_ans->check_hazard_ptr_in_using_node( p_chk_ptr ) ) {
						return true;
					}
				}
				p_ans = p_ans->get_next();
			}
			return false;
		}

		int get_node_count( void )
		{
			return node_count_.load( std::memory_order_acquire );
		}

		void dump_to_log( log_type lt, char c, int id )
		{
			p_allocator_->dump_to_log( lt, c, id );
			internal::LogOutput( lt, "count of node_for_hazard_ptr of hazard_node_head(%p): %d", this, node_count_.load( std::memory_order_acquire ) );
		}

	private:
		/*!
		 * @brief	新しいノードを用意し、リストに追加する。
		 *
		 * 追加したノードは、USING状態で返される。
		 *
		 * @return	追加した新しいノードへのポインタ
		 */
		node_for_hazard_ptr* add_one_new_hazard_ptr_node( void )
		{
			node_for_hazard_ptr* p_ans        = new ( *p_allocator_ ) node_for_hazard_ptr();
			node_for_hazard_ptr* p_next_check = head_.load( std::memory_order_acquire );
			do {
				p_ans->set_next( p_next_check );
			} while ( !head_.compare_exchange_strong( p_next_check, p_ans, std::memory_order_acq_rel ) );   // CASが成功するまで繰り返す。
			node_count_++;

			internal::LogOutput( log_type::DEBUG, "glist is added by add_one_new_hazard_ptr_node(%p)", p_ans );
			return p_ans;
		}

		hazard_node_head( const hazard_node_head& )            = delete;
		hazard_node_head( hazard_node_head&& )                 = delete;
		hazard_node_head& operator=( const hazard_node_head& ) = delete;
		hazard_node_head& operator=( hazard_node_head&& )      = delete;

		internal::alloc_only_chamber*     p_allocator_;   //!< ハザードポインタリストに登録されるノードに対する割り当て専用アロケータへの参照
		std::atomic<node_for_hazard_ptr*> head_;          //!< ハザードポインタリストの先頭ノードへのポインタ
		std::atomic<int>                  node_count_;    //!< ハザードポインタリストに登録されているノード数
	};

	struct threadlocal_handler_functor {
		threadlocal_handler_functor( hazard_ptr_storage* p_node_list_owner_arg )
		  : p_node_list_owner_( p_node_list_owner_arg )
		{
		}

		uintptr_t allocate( void )
		{
			node_for_hazard_ptr* p_ans = p_node_list_owner_->get_head_instance().allocate_hazard_ptr_node();
			if ( p_ans == nullptr ) {
				throw std::bad_alloc();
			}
			return reinterpret_cast<uintptr_t>( p_ans );
		}
		void deallocate( uintptr_t p_destructing_tls )
		{
			node_for_hazard_ptr* p_tmp = reinterpret_cast<node_for_hazard_ptr*>( p_destructing_tls );
			p_tmp->release_owner();
		}

		hazard_ptr_storage* p_node_list_owner_;
	};

	inline hazard_node_head& get_head_instance( void )
	{
		return head_;
	}

	inline node_for_hazard_ptr* get_tls_node_for_hazard_ptr( void )
	{
		return p_hzd_ptr_node_.get_tls_instance();
	}

	internal::alloc_only_chamber                                   my_allocator_;   //!< ハザードポインタリストに登録されるノードに対する割り当て専用アロケータ
	internal::alloc_only_chamber*                                  p_allocator_;    //!< ハザードポインタリストに登録されるノードに対する割り当て専用アロケータへの参照
	hazard_node_head                                               head_;
	dynamic_tls<node_for_hazard_ptr*, threadlocal_handler_functor> p_hzd_ptr_node_;

	friend hazard_ptr_scoped_ref<T, N>;
};

/*!
 * @brief	scoped reference control support class for hazard_ptr_storage
 *
 * スコープベースでの、参照権の解放制御をサポートするクラスの実体定義クラス
 */
template <typename T, int N>
class hazard_ptr_scoped_ref {
public:
	hazard_ptr_scoped_ref( hazard_ptr_storage<T, N>& ref, int idx_arg )
	  : idx_( idx_arg )
	  , p_node_hzd_ptr_( ref.get_tls_node_for_hazard_ptr() )
	{
		if ( idx_arg >= N ) {
			internal::LogOutput( log_type::ERR, "Error: the requested index is over max index." );
			throw std::logic_error( "Error: the requested index is over max index." );
		}
	}

	hazard_ptr_scoped_ref( const hazard_ptr_scoped_ref& orig, int idx_arg )
	  : idx_( idx_arg )
	  , p_node_hzd_ptr_( orig.p_node_hzd_ptr_ )
	{
		if ( idx_arg >= N ) {
			internal::LogOutput( log_type::ERR, "Error: the requested index is over max index." );
			throw std::logic_error( "Error: the requested index is over max index." );
		}
	}

	~hazard_ptr_scoped_ref()
	{
		p_node_hzd_ptr_->clear_hazard_ptr( idx_ );
	}

	void regist_ptr_as_hazard_ptr( T* p_target )
	{
		p_node_hzd_ptr_->set_hazard_ptr( p_target, idx_ );
	}

private:
	int                                                     idx_;
	typename hazard_ptr_storage<T, N>::node_for_hazard_ptr* p_node_hzd_ptr_;
};

template <typename T>
class hazard_ptr_handler;

template <typename T>
class hazard_ptr_w_mark_handler;

template <typename T>
class hazard_ptr {
public:
	using element_type = T;
	using pointer      = T*;

	hazard_ptr( pointer p_arg = nullptr )
	  : p_( p_arg )
	  , os_( internal::hazard_ptr_mgr::AssignHazardPtrSlot( p_arg ) )
	{
	}
	~hazard_ptr() = default;
	ALCC_INTERNAL_CONSTEXPR_CONSTRUCTOR_BODY hazard_ptr( const hazard_ptr& src )
	  : p_( src.p_ )
	  , os_( internal::hazard_ptr_mgr::AssignHazardPtrSlot( src.p_ ) )
	{
	}
	ALCC_INTERNAL_CONSTEXPR_CONSTRUCTOR_BODY hazard_ptr( hazard_ptr&& src )
	  : p_( src.p_ )
	  , os_( std::move( src.os_ ) )
	{
#if defined( ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR ) || defined( ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION )
		if ( os_ == nullptr ) {
			internal::LogOutput( log_type::ERR, "slot of hazard pointer in hazard_ptr is nullptr" );
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION
			std::terminate();
#endif
		}
#endif
		src.p_  = nullptr;
		src.os_ = internal::hazard_ptr_mgr::AssignHazardPtrSlot( nullptr );
	}
	hazard_ptr& operator=( const hazard_ptr& src )
	{
#if defined( ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR ) || defined( ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION )
		if ( os_ == nullptr ) {
			internal::LogOutput( log_type::ERR, "slot of hazard pointer in hazard_ptr is nullptr" );
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION
			std::terminate();
#endif
		}
#endif

		if ( this == &src ) return *this;

		p_ = src.p_;
		os_->store( src.os_->load( std::memory_order_acquire ), internal::hzrd_slot_memory_order_for_store );
		// nullptrが特別扱いされるため、src側がnullptrだった場合に自然に対応できるようにload()を使う。
		// p == nullptrで判定しても良いが、どちら効率的かは不明。。。

		return *this;
	}
	hazard_ptr& operator=( hazard_ptr&& src )
	{
		if ( this == &src ) return *this;

		swap( src );

		return *this;
	}

	void swap( hazard_ptr& src )
	{
		auto p_tmp = p_;
		p_         = src.p_;
		src.p_     = p_tmp;
		os_.swap( src.os_ );
	}

	template <bool IsVoid = std::is_same<T, void>::value, typename std::enable_if<!IsVoid>::type* = nullptr>
	auto operator->() const noexcept -> typename std::add_pointer<typename std::enable_if<!IsVoid, T>::type>::type
	{
		return p_;
	}

	template <bool IsVoid = std::is_same<T, void>::value, typename std::enable_if<!IsVoid>::type* = nullptr>
	auto operator*() const noexcept -> typename std::add_lvalue_reference<typename std::enable_if<!IsVoid, T>::type>::type
	{
		return *p_;
	}

	T* get() const noexcept
	{
		return p_;
	}

	template <typename CAST_T>
	auto get_pointer_by_static_cast( void ) const -> typename std::conditional<std::is_const<T>::value, typename std::remove_const<CAST_T>::type*, CAST_T*>::type
	{
#ifdef ALCONCURRENT_CONF_REPLACE_STATIC_CAST_OF_ZDPTR_TO_DYNAMIC_CASTH
		return dynamic_cast<typename std::conditional<std::is_const<T>, typename std::remove_const<CAST_T>::type*, CAST_T*>::type>( p_ );
#else
		return static_cast<typename std::conditional<std::is_const<T>::value, typename std::remove_const<CAST_T>::type*, CAST_T*>::type>( p_ );
#endif
	}

	template <typename CAST_T>
	auto get_pointer_by_dynamic_cast( void ) const -> typename std::conditional<std::is_const<T>::value, typename std::remove_const<CAST_T>::type*, CAST_T*>::type
	{
		return dynamic_cast<typename std::conditional<std::is_const<T>::value, typename std::remove_const<CAST_T>::type*, CAST_T*>::type>( p_ );
	}

	void store( pointer p_arg ) noexcept
	{
#if defined( ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR ) || defined( ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION )
		if ( os_ == nullptr ) {
			internal::LogOutput( log_type::ERR, "slot of hazard pointer in hazard_ptr is nullptr, p_=%p", p_ );
			bt_info cur_bt;
			RECORD_BACKTRACE_GET_BACKTRACE( cur_bt );
			cur_bt.dump_to_log( log_type::ERR, 'd', 1 );
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION
			throw std::logic_error( "slot of hazard pointer in hazard_ptr is nullptr" );
#else
#endif
		}
#endif

		p_ = p_arg;
		reflect_from_p();
	}

private:
#if defined( ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR ) || defined( ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION )
#else
	constexpr
#endif
	hazard_ptr( T* p_arg, internal::hzrd_slot_ownership_t os_arg )
	  : p_( p_arg )
	  , os_( std::move( os_arg ) )
	{
		// 事前条件： os_argには、p_argが格納されていること。
#if defined( ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR ) || defined( ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION )
		static std::atomic<int> id_cnt( 1 );
		int                     cur_id = id_cnt.fetch_add( 1 );
		if ( os_ == nullptr ) {
			internal::LogOutput( log_type::ERR, "slot of hazard pointer in hazard_ptr is nullptr, os_.get()=%p vs p_=%p", os_.get(), p_ );
			bt_info cur_bt;
			RECORD_BACKTRACE_GET_BACKTRACE( cur_bt );
			cur_bt.dump_to_log( log_type::ERR, 'a', cur_id );
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION
			throw std::logic_error( "slot of hazard pointer in hazard_ptr is nullptr" );
#else
#endif
		}
		if ( p_ == nullptr ) {
			if ( os_->load( std::memory_order_acquire ) != reinterpret_cast<void*>( static_cast<std::uintptr_t>( 1U ) ) ) {
				internal::LogOutput( log_type::ERR, "p_ is nullptr, but slot of hazard pointer in hazard_ptr is not 1U, os_.get()=%p vs p_=%p", os_.get(), p_ );
				bt_info cur_bt;
				RECORD_BACKTRACE_GET_BACKTRACE( cur_bt );
				cur_bt.dump_to_log( log_type::ERR, 'b', cur_id );
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION
				throw std::logic_error( "p_ is nullptr, but slot of hazard pointer in hazard_ptr is not 1U" );
#else
#endif
			}
		} else {
			if ( os_->load( std::memory_order_acquire ) != reinterpret_cast<void*>( p_ ) ) {
				internal::LogOutput( log_type::ERR, "slot of hazard pointer in hazard_ptr is not same to p_, os_.get()=%p vs p_=%p", os_.get(), p_ );
				bt_info cur_bt;
				RECORD_BACKTRACE_GET_BACKTRACE( cur_bt );
				cur_bt.dump_to_log( log_type::ERR, 'c', cur_id );
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION
				throw std::logic_error( "slot of hazard pointer in hazard_ptr is not same to p_" );
#else
#endif
			}
		}
#endif
	}

	void reflect_from_p( void ) noexcept
	{
#if defined( ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR ) || defined( ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION )
		if ( os_ == nullptr ) {
			internal::LogOutput( log_type::ERR, "slot of hazard pointer in hazard_ptr is nullptr, p_=%p", p_ );
			bt_info cur_bt;
			RECORD_BACKTRACE_GET_BACKTRACE( cur_bt );
			cur_bt.dump_to_log( log_type::ERR, 'd', 1 );
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION
			throw std::logic_error( "slot of hazard pointer in hazard_ptr is nullptr" );
#else
#endif
		}
#endif

		os_->store( ( p_ == nullptr ) ? reinterpret_cast<pointer>( static_cast<std::uintptr_t>( 1U ) ) : p_, internal::hzrd_slot_memory_order_for_store );
	}

	pointer                         p_;
	internal::hzrd_slot_ownership_t os_;

	friend class hazard_ptr_handler<T>;
	friend class hazard_ptr_w_mark_handler<T>;

	template <class T1, class T2>
	friend constexpr bool operator==( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator!=( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator<( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator<=( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator>( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator>=( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept;

	template <class T1>
	friend constexpr bool operator==( const hazard_ptr<T1>& a, std::nullptr_t ) noexcept;
	template <class T1>
	friend constexpr bool operator!=( const hazard_ptr<T1>& a, std::nullptr_t ) noexcept;
	template <class T1>
	friend constexpr bool operator==( std::nullptr_t, const hazard_ptr<T1>& a ) noexcept;
	template <class T1>
	friend constexpr bool operator!=( std::nullptr_t, const hazard_ptr<T1>& a ) noexcept;

	template <class T1, class T2>
	friend constexpr bool operator==( const hazard_ptr<T1>& a, const T2* b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator!=( const hazard_ptr<T1>& a, const T2* b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator<( const hazard_ptr<T1>& a, const T2* b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator<=( const hazard_ptr<T1>& a, const T2* b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator>( const hazard_ptr<T1>& a, const T2* b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator>=( const hazard_ptr<T1>& a, const T2* b ) noexcept;

	template <class T1, class T2>
	friend constexpr bool operator==( const T1* a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator!=( const T1* a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator<( const T1* a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator<=( const T1* a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator>( const T1* a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator>=( const T1* a, const hazard_ptr<T2>& b ) noexcept;
};

template <class T1, class T2>
constexpr bool operator==( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept
{
	return reinterpret_cast<void*>( a.p_ ) == reinterpret_cast<void*>( b.p_ );
}

template <class T1, class T2>
constexpr bool operator!=( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept
{
	return reinterpret_cast<void*>( a.p_ ) != reinterpret_cast<void*>( b.p_ );
}

template <class T1, class T2>
constexpr bool operator<( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept
{
	return reinterpret_cast<void*>( a.p_ ) < reinterpret_cast<void*>( b.p_ );
}

template <class T1, class T2>
constexpr bool operator<=( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept
{
	return reinterpret_cast<void*>( a.p_ ) <= reinterpret_cast<void*>( b.p_ );
}

template <class T1, class T2>
constexpr bool operator>( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept
{
	return reinterpret_cast<void*>( a.p_ ) > reinterpret_cast<void*>( b.p_ );
}

template <class T1, class T2>
constexpr bool operator>=( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept
{
	return reinterpret_cast<void*>( a.p_ ) >= reinterpret_cast<void*>( b.p_ );
}

template <class T1>
constexpr bool operator==( const hazard_ptr<T1>& a, std::nullptr_t ) noexcept
{
	return reinterpret_cast<void*>( a.p_ ) == nullptr;
}

template <class T1>
constexpr bool operator!=( const hazard_ptr<T1>& a, std::nullptr_t ) noexcept
{
	return reinterpret_cast<void*>( a.p_ ) != nullptr;
}

template <class T1>
constexpr bool operator==( std::nullptr_t, const hazard_ptr<T1>& a ) noexcept
{
	return reinterpret_cast<void*>( a.p_ ) == nullptr;
}

template <class T1>
constexpr bool operator!=( std::nullptr_t, const hazard_ptr<T1>& a ) noexcept
{
	return reinterpret_cast<void*>( a.p_ ) != nullptr;
}

template <class T1, class T2>
constexpr bool operator==( const hazard_ptr<T1>& a, const T2* b ) noexcept
{
	return a.p_ == b;
}
template <class T1, class T2>
constexpr bool operator!=( const hazard_ptr<T1>& a, const T2* b ) noexcept
{
	return a.p_ != b;
}
template <class T1, class T2>
constexpr bool operator<( const hazard_ptr<T1>& a, const T2* b ) noexcept
{
	return a.p_ < b;
}
template <class T1, class T2>
constexpr bool operator<=( const hazard_ptr<T1>& a, const T2* b ) noexcept
{
	return a.p_ <= b;
}
template <class T1, class T2>
constexpr bool operator>( const hazard_ptr<T1>& a, const T2* b ) noexcept
{
	return a.p_ > b;
}
template <class T1, class T2>
constexpr bool operator>=( const hazard_ptr<T1>& a, const T2* b ) noexcept
{
	return a.p_ >= b;
}

template <class T1, class T2>
constexpr bool operator==( const T1* a, const hazard_ptr<T2>& b ) noexcept
{
	return a == b.p_;
}
template <class T1, class T2>
constexpr bool operator!=( const T1* a, const hazard_ptr<T2>& b ) noexcept
{
	return a != b.p_;
}
template <class T1, class T2>
constexpr bool operator<( const T1* a, const hazard_ptr<T2>& b ) noexcept
{
	return a < b.p_;
}
template <class T1, class T2>
constexpr bool operator<=( const T1* a, const hazard_ptr<T2>& b ) noexcept
{
	return a <= b.p_;
}
template <class T1, class T2>
constexpr bool operator>( const T1* a, const hazard_ptr<T2>& b ) noexcept
{
	return a > b.p_;
}
template <class T1, class T2>
constexpr bool operator>=( const T1* a, const hazard_ptr<T2>& b ) noexcept
{
	return a >= b.p_;
}

template <typename T>
class hazard_ptr_handler {
public:
	using element_type   = T;
	using pointer        = T*;
	using hazard_pointer = hazard_ptr<T>;

#if 0
	hazard_ptr_handler( void ) noexcept
	  : ap_target_p_()
	{
		ap_target_p_.store( nullptr, std::memory_order_release );
	}
	explicit hazard_ptr_handler( T* p_desired ) noexcept
	  : ap_target_p_()
	{
		ap_target_p_.store( p_desired, std::memory_order_release );
	}
	hazard_ptr_handler( const hazard_ptr_handler& src ) noexcept
	  : ap_target_p_()
	{
		ap_target_p_.store( src.ap_target_p_.load( std::memory_order_acquire ), std::memory_order_release );
	}
#else
	constexpr hazard_ptr_handler( void ) noexcept
	  : ap_target_p_( nullptr )
	{
	}
	explicit constexpr hazard_ptr_handler( T* p_desired ) noexcept
	  : ap_target_p_( p_desired )
	{
	}
	constexpr hazard_ptr_handler( const hazard_ptr_handler& src ) noexcept
	  : ap_target_p_( src.ap_target_p_.load( std::memory_order_acquire ) )
	{
	}
#endif
	hazard_ptr_handler( hazard_ptr_handler&& src ) noexcept
	  : ap_target_p_()
	{
		pointer p_expect = src.ap_target_p_.load( std::memory_order_acquire );
		do {
			ap_target_p_.store( p_expect, std::memory_order_release );
		} while ( !src.ap_target_p_.compare_exchange_weak( p_expect, nullptr, std::memory_order_acq_rel, std::memory_order_acquire ) );
	}
	hazard_ptr_handler& operator=( const hazard_ptr_handler& src ) noexcept
	{
		if ( this == &src ) return *this;

		ap_target_p_.store( src.ap_target_p_.load( std::memory_order_acquire ), std::memory_order_release );

		return *this;
	}
	hazard_ptr_handler& operator=( hazard_ptr_handler&& src ) noexcept
	{
		if ( this == &src ) return *this;

		pointer p_expect = src.ap_target_p_.load( std::memory_order_acquire );
		do {
			ap_target_p_.store( p_expect, std::memory_order_release );
		} while ( !src.ap_target_p_.compare_exchange_weak( p_expect, nullptr, std::memory_order_acq_rel, std::memory_order_acquire ) );

		return *this;
	}

	hazard_pointer get( void )
	{
#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
		internal::call_count_hazard_ptr_get_++;
#endif

#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
		internal::loop_count_in_hazard_ptr_get_++;
#endif
		hazard_pointer hp_ans( ap_target_p_.load( std::memory_order_acquire ) );
		while ( !ap_target_p_.compare_exchange_strong( hp_ans.p_, hp_ans.p_, std::memory_order_acq_rel, std::memory_order_acquire ) ) {
#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
			internal::loop_count_in_hazard_ptr_get_++;
#endif
			hp_ans.reflect_from_p();
		}

		return hp_ans;
	}

	hazard_pointer get_to_verify_exchange( void )
	{
		return hazard_pointer( ap_target_p_.load( std::memory_order_acquire ) );
	}
	bool verify_exchange( hazard_pointer& hp )
	{
		pointer p_expect = ap_target_p_.load( std::memory_order_acquire );
		bool    ret      = ( p_expect == hp );
		if ( !ret ) {
			hp.store( p_expect );
		}
		return ret;
	}

	void reuse( hazard_pointer& hp_reuse )
	{
#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
		internal::call_count_hazard_ptr_get_++;
#endif

		pointer p_expect = ap_target_p_.load( std::memory_order_acquire );
		do {
#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
			internal::loop_count_in_hazard_ptr_get_++;
#endif
			hp_reuse.store( p_expect );
			if ( p_expect == nullptr ) {
				return;
			}
		} while ( !ap_target_p_.compare_exchange_strong( p_expect, p_expect, std::memory_order_acq_rel, std::memory_order_acquire ) );
		// TODO: is there any Redundancy ? この方法に冗長性はないか？

		return;
	}

	void reuse_to_verify_exchange( hazard_pointer& hp_reuse ) const noexcept
	{
		pointer p_expect = ap_target_p_.load( std::memory_order_acquire );
		hp_reuse.store( p_expect );

		return;
	}

	pointer load( std::memory_order order = std::memory_order_acquire ) const noexcept
	{
		return ap_target_p_.load( order );
	}

	void store( pointer p_desired, std::memory_order order = std::memory_order_release ) noexcept
	{
		ap_target_p_.store( p_desired, order );
	}

	inline bool compare_exchange_weak( pointer&          expected,
	                                   pointer           desired,
	                                   std::memory_order success = std::memory_order_acq_rel,
	                                   std::memory_order failure = std::memory_order_acquire ) noexcept
	{
		return ap_target_p_.compare_exchange_weak( expected, desired, success, failure );
	}

	inline bool compare_exchange_strong( pointer&          expected,
	                                     pointer           desired,
	                                     std::memory_order success = std::memory_order_acq_rel,
	                                     std::memory_order failure = std::memory_order_acquire ) noexcept
	{
		return ap_target_p_.compare_exchange_strong( expected, desired, success, failure );
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_weak を使用する。
	 *
	 * 現在の値と expected_hzd_ptr が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、
	 * そうでなければ failure メモリオーダーで expected_hzd_ptr を現在の値で置き換える。
	 * また、failureケースの置き換え時に自身の現在の値との一致を再確認し、再確認成功後、関数が完了する。
	 * そのため、compare_exchange_strong_to_verify_exchangeよりも処理が重い。
	 *
	 * @param expected_hzd_ptr 自身が保持していると期待されるハザードポインタへの参照
	 * @param desired CAS操作で置き換える新たなポインタ値
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @param failure CAS操作失敗時に使用するメモリオーダー
	 * @return true CAS操作に成功。また、expected_hzd_ptrは呼び出し前の状態を維持している。
	 * @return false CAS操作に失敗。自身が保持するあらなたポインタでexpected_hzd_ptrを置き換えられている。
	 */
	inline bool compare_exchange_weak( hazard_pointer&   expected_hzd_ptr,
	                                   pointer           desired,
	                                   std::memory_order success = std::memory_order_acq_rel,
	                                   std::memory_order failure = std::memory_order_acquire ) noexcept
	{
		bool ret = ap_target_p_.compare_exchange_weak( expected_hzd_ptr.p_, desired, success, failure );
		if ( !ret ) {
			// 置き換えに失敗した場合、新たな値が、expected_hzd_ptr.p_に設定されている。
			// しかし、まだハザードポインタ用のスロットには反映されていないため、更新を行う。
			do {
				expected_hzd_ptr.reflect_from_p();
			} while ( !ap_target_p_.compare_exchange_weak( expected_hzd_ptr.p_, expected_hzd_ptr.p_, success, failure ) );
		}
		return ret;
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_strong を使用する。
	 *
	 * 現在の値と expected_hzd_ptr が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、
	 * そうでなければ failure メモリオーダーで expected_hzd_ptr を現在の値で置き換える。
	 * また、failureケースの置き換え時に自身の現在の値との一致を再確認し、再確認成功後、関数が完了する。
	 * そのため、compare_exchange_strong_to_verify_exchangeよりも処理が重い。
	 *
	 * @param expected_hzd_ptr 自身が保持していると期待されるハザードポインタへの参照
	 * @param desired CAS操作で置き換える新たなポインタ値
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @param failure CAS操作失敗時に使用するメモリオーダー
	 * @return true CAS操作に成功。また、expected_hzd_ptrは呼び出し前の状態を維持している。
	 * @return false CAS操作に失敗。自身が保持するあらなたポインタでexpected_hzd_ptrを置き換えられている。
	 */
	inline bool compare_exchange_strong( hazard_pointer&   expected_hzd_ptr,
	                                     pointer           desired,
	                                     std::memory_order success = std::memory_order_acq_rel,
	                                     std::memory_order failure = std::memory_order_acquire ) noexcept
	{
		bool ret = ap_target_p_.compare_exchange_strong( expected_hzd_ptr.p_, desired, success, failure );
		if ( !ret ) {
			// 置き換えに失敗した場合、新たな値が、expected_hzd_ptr.p_に設定されている。
			// しかし、まだハザードポインタ用のスロットには反映されていないため、更新を行う。
			do {
				expected_hzd_ptr.reflect_from_p();
			} while ( !ap_target_p_.compare_exchange_strong( expected_hzd_ptr.p_, expected_hzd_ptr.p_, success, failure ) );
		}
		return ret;
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_weak を使用する。
	 *
	 * 現在の値と expected_hzd_ptr が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、
	 *
	 * なお、expected_hzd_ptr 使用できない状態となるため、expected_hzd_ptr を使い捨てにする場合に使用するI/F。
	 *
	 * @param expected_hzd_ptr 自身が保持していると期待されるハザードポインタへの参照。呼び出し完了後、hazard_ptrとして使用できない。
	 * @param desired CAS操作で置き換える新たなポインタ値
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @return true CAS操作に成功。
	 * @return false CAS操作に失敗。
	 */
	inline bool compare_exchange_weak( hazard_pointer&&  expected_hzd_ptr,
	                                   pointer           desired,
	                                   std::memory_order success = std::memory_order_acq_rel ) noexcept
	{
		bool ret = ap_target_p_.compare_exchange_weak( expected_hzd_ptr.p_, desired, success, std::memory_order_relaxed );
		return ret;
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_strong を使用する。
	 *
	 * 現在の値と expected_hzd_ptr が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、
	 *
	 * なお、expected_hzd_ptr 使用できない状態となるため、expected_hzd_ptr を使い捨てにする場合に使用するI/F。
	 *
	 * @param expected_hzd_ptr 自身が保持していると期待されるハザードポインタへの参照。呼び出し完了後、hazard_ptrとして使用できない。
	 * @param desired CAS操作で置き換える新たなポインタ値
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @return true CAS操作に成功。
	 * @return false CAS操作に失敗。
	 */
	inline bool compare_exchange_strong( hazard_pointer&&  expected_hzd_ptr,
	                                     pointer           desired,
	                                     std::memory_order success = std::memory_order_acq_rel ) noexcept
	{
		bool ret = ap_target_p_.compare_exchange_strong( expected_hzd_ptr.p_, desired, success, std::memory_order_relaxed );
		return ret;
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_weak を使用する。
	 *
	 * 現在の値と expected_hzd_ptr が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、expected_hzd_ptr に、desired を反映する。
	 * そうでなければ failure メモリオーダーで expected_hzd_ptr を現在の値で置き換える。
	 *
	 * @param expected_hzd_ptr 自身が保持していると期待されるハザードポインタへの参照
	 * @param desired CAS操作で置き換える新たなポインタ値
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @param failure CAS操作失敗時に使用するメモリオーダー
	 * @return true CAS操作に成功。expected_hzd_ptr は、desired で置き換えられている。
	 * @return false CAS操作に失敗。自身が保持する現在のポインタでexpected_hzd_ptrを置き換えられている。
	 */
	inline bool compare_exchange_weak_to_verify_exchange1( hazard_pointer&   expected_hzd_ptr,
	                                                       pointer           desired,
	                                                       std::memory_order success = std::memory_order_acq_rel,
	                                                       std::memory_order failure = std::memory_order_acquire ) noexcept

	{
		bool ret = ap_target_p_.compare_exchange_weak( expected_hzd_ptr.p_, desired, success, failure );
		if ( ret ) {
			// 置き換えに成功した場合、desiredがまだハザードポインタ用のスロットには反映されていないため、更新を行う。
			expected_hzd_ptr.store( desired );
		} else {
			// 置き換えに失敗した場合、新たな値が、expected_hzd_ptr.p_に設定されている。
			// しかし、まだハザードポインタ用のスロットには反映されていないため、更新を行う。
			expected_hzd_ptr.reflect_from_p();
		}
		return ret;
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_weak を使用する。
	 *
	 * 現在の値と expected_hzd_ptr が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、
	 * そうでなければ failure メモリオーダーで expected_hzd_ptr を現在の値で置き換える
	 *
	 * @param expected_hzd_ptr 自身が保持していると期待されるハザードポインタへの参照
	 * @param desired CAS操作で置き換える新たなポインタ値
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @param failure CAS操作失敗時に使用するメモリオーダー
	 * @return true CAS操作に成功。また、expected_hzd_ptrは呼び出し前の状態を維持している。
	 * @return false CAS操作に失敗。自身が保持するあらなたポインタでexpected_hzd_ptrを置き換えられている。
	 */
	inline bool compare_exchange_weak_to_verify_exchange2( hazard_pointer&   expected_hzd_ptr,
	                                                       pointer           desired,
	                                                       std::memory_order success = std::memory_order_acq_rel,
	                                                       std::memory_order failure = std::memory_order_acquire ) noexcept
	{
		bool ret = ap_target_p_.compare_exchange_weak( expected_hzd_ptr.p_, desired, success, failure );
		if ( !ret ) {
			// 置き換えに失敗した場合、新たな値が、expected_hzd_ptr.p_に設定されている。
			// しかし、まだハザードポインタ用のスロットには反映されていないため、更新を行う。
			expected_hzd_ptr.reflect_from_p();
		}
		return ret;
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_strong を使用する。
	 *
	 * 現在の値と expected_hzd_ptr が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、expected_hzd_ptr に、desired を反映する。
	 * そうでなければ failure メモリオーダーで expected_hzd_ptr を現在の値で置き換える。
	 *
	 * @param expected_hzd_ptr 自身が保持していると期待されるハザードポインタへの参照
	 * @param desired CAS操作で置き換える新たなポインタ値
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @param failure CAS操作失敗時に使用するメモリオーダー
	 * @return true CAS操作に成功。expected_hzd_ptr は、desired で置き換えられている。
	 * @return false CAS操作に失敗。自身が保持する現在のポインタでexpected_hzd_ptrを置き換えられている。
	 */
	inline bool compare_exchange_strong_to_verify_exchange1( hazard_pointer&   expected_hzd_ptr,
	                                                         pointer           desired,
	                                                         std::memory_order success = std::memory_order_acq_rel,
	                                                         std::memory_order failure = std::memory_order_acquire ) noexcept
	{
		bool ret = ap_target_p_.compare_exchange_strong( expected_hzd_ptr.p_, desired, success, failure );
		if ( ret ) {
			// 置き換えに成功した場合、desiredがまだハザードポインタ用のスロットには反映されていないため、更新を行う。
			expected_hzd_ptr.store( desired );
		} else {
			// 置き換えに失敗した場合、新たな値が、expected_hzd_ptr.p_に設定されている。
			// しかし、まだハザードポインタ用のスロットには反映されていないため、更新を行う。
			expected_hzd_ptr.reflect_from_p();
		}
		return ret;
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_strong を使用する。
	 *
	 * 現在の値と expected_hzd_ptr が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、
	 * そうでなければ failure メモリオーダーで expected_hzd_ptr を現在の値で置き換える
	 *
	 * @param expected_hzd_ptr 自身が保持していると期待されるハザードポインタへの参照
	 * @param desired CAS操作で置き換える新たなポインタ値
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @param failure CAS操作失敗時に使用するメモリオーダー
	 * @return true CAS操作に成功。また、expected_hzd_ptrは呼び出し前の状態を維持している。
	 * @return false CAS操作に失敗。自身が保持するあらなたポインタでexpected_hzd_ptrを置き換えられている。
	 */
	inline bool compare_exchange_strong_to_verify_exchange2( hazard_pointer&   expected_hzd_ptr,
	                                                         pointer           desired,
	                                                         std::memory_order success = std::memory_order_acq_rel,
	                                                         std::memory_order failure = std::memory_order_acquire ) noexcept
	{
		bool ret = ap_target_p_.compare_exchange_strong( expected_hzd_ptr.p_, desired, success, failure );
		if ( !ret ) {
			// 置き換えに失敗した場合、新たな値が、expected_hzd_ptr.p_に設定されている。
			// しかし、まだハザードポインタ用のスロットには反映されていないため、更新を行う。
			expected_hzd_ptr.reflect_from_p();
		}
		return ret;
	}

private:
	std::atomic<pointer> ap_target_p_;
};

template <typename T>
class hazard_ptr_w_mark_handler {
public:
	using element_type   = T;
	using pointer        = T*;
	using hazard_pointer = hazard_ptr<T>;

	hazard_ptr_w_mark_handler( void ) noexcept
	  : a_target_addr_()
	{
		a_target_addr_.store( static_cast<addr_markable>( 0U ), std::memory_order_release );
	}
	explicit hazard_ptr_w_mark_handler( T* p_desired ) noexcept
	  : a_target_addr_()
	{
		a_target_addr_.store( reinterpret_cast<addr_markable>( p_desired ), std::memory_order_release );
	}
	hazard_ptr_w_mark_handler( const hazard_ptr_w_mark_handler& src ) noexcept
	  : a_target_addr_()
	{
		a_target_addr_.store( src.a_target_addr_.load( std::memory_order_acquire ), std::memory_order_release );
	}
	hazard_ptr_w_mark_handler( hazard_ptr_w_mark_handler&& src ) noexcept
	  : a_target_addr_()
	{
		addr_markable addr_expect = src.a_target_addr_.load( std::memory_order_acquire );
		do {
			a_target_addr_.store( addr_expect, std::memory_order_release );
		} while ( !src.a_target_addr_.compare_exchange_weak( addr_expect, static_cast<addr_markable>( 0U ), std::memory_order_acq_rel, std::memory_order_acquire ) );
	}
	hazard_ptr_w_mark_handler& operator=( const hazard_ptr_w_mark_handler& src ) noexcept
	{
		if ( this == &src ) return *this;

		a_target_addr_.store( src.a_target_addr_.load( std::memory_order_acquire ), std::memory_order_release );

		return *this;
	}
	hazard_ptr_w_mark_handler& operator=( hazard_ptr_w_mark_handler&& src ) noexcept
	{
		if ( this == &src ) return *this;

		constexpr addr_markable clear_val   = 0;
		addr_markable           addr_expect = src.a_target_addr_.load( std::memory_order_acquire );
		do {
			a_target_addr_.store( addr_expect, std::memory_order_release );
		} while ( !src.a_target_addr_.compare_exchange_weak( addr_expect, clear_val, std::memory_order_acq_rel, std::memory_order_acquire ) );

		return *this;
	}

	std::tuple<hazard_pointer, bool> get( void )
	{
#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
		internal::call_count_hazard_ptr_get_++;
#endif

		hazard_pointer ans_hp( nullptr, internal::hazard_ptr_mgr::AssignHazardPtrSlot( nullptr ) );
		bool           ans_b = false;

		addr_markable addr_expect = a_target_addr_.load( std::memory_order_acquire );

		do {
#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
			internal::loop_count_in_hazard_ptr_get_++;
#endif
			std::tuple<pointer, bool> pointer_w_mark_expect = unzip_addr_markable_to_tuple( addr_expect );
			pointer                   p_expect              = std::get<0>( pointer_w_mark_expect );
			ans_b                                           = std::get<1>( pointer_w_mark_expect );

			ans_hp.store( p_expect );
			if ( p_expect == nullptr ) {
				ans_b = false;
				break;
			}

		} while ( !a_target_addr_.compare_exchange_weak( addr_expect, addr_expect, std::memory_order_acq_rel, std::memory_order_acquire ) );
		// TODO: is there any Redundancy ? この方法に冗長性はないか？

		return std::tuple<hazard_pointer, bool> { std::move( ans_hp ), ans_b };
	}

	void reuse( std::tuple<hazard_pointer, bool>& hp_reuse )
	{
#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
		internal::call_count_hazard_ptr_get_++;
#endif

		addr_markable addr_expect = a_target_addr_.load( std::memory_order_acquire );

		do {
#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
			internal::loop_count_in_hazard_ptr_get_++;
#endif
			std::tuple<pointer, bool> pointer_w_mark_expect = unzip_addr_markable_to_tuple( addr_expect );
			pointer                   p_expect              = std::get<0>( pointer_w_mark_expect );
			bool                      expect_mark           = std::get<1>( pointer_w_mark_expect );

			if ( p_expect == nullptr ) {
				std::get<0>( hp_reuse ).store( p_expect );
				std::get<1>( hp_reuse ) = false;
				return;
			}
			std::get<0>( hp_reuse ).store( p_expect );
			std::get<1>( hp_reuse ) = expect_mark;
		} while ( !a_target_addr_.compare_exchange_weak( addr_expect, addr_expect, std::memory_order_acq_rel, std::memory_order_acquire ) );
		// TODO: is there any Redundancy ? この方法に冗長性はないか？

		return;
	}

	std::tuple<pointer, bool> load( std::memory_order order = std::memory_order_acquire ) const noexcept
	{
		return unzip_addr_markable_to_tuple( a_target_addr_.load( order ) );
	}

	void store( const std::tuple<pointer, bool>& desired, std::memory_order order = std::memory_order_release ) noexcept
	{
		a_target_addr_.store( zip_tuple_to_addr_markable( desired ), order );
	}

	bool compare_exchange_weak( std::tuple<pointer, bool>&       expected,
	                            const std::tuple<pointer, bool>& desired,
	                            std::memory_order                success = std::memory_order_acq_rel,
	                            std::memory_order                failure = std::memory_order_acquire ) noexcept
	{
		addr_markable addr_expected = zip_tuple_to_addr_markable( expected );
		addr_markable addr_desired  = zip_tuple_to_addr_markable( desired );
		bool          ret           = a_target_addr_.compare_exchange_weak( addr_expected, addr_desired, success, failure );
		if ( !ret ) {
			expected = unzip_addr_markable_to_tuple( addr_expected );
		}
		return ret;
	}

	bool compare_exchange_weak( std::tuple<pointer, bool>&       expected,
	                            const std::tuple<pointer, bool>& desired,
	                            std::memory_order                order = std::memory_order_seq_cst ) noexcept
	{
		addr_markable addr_expected = zip_tuple_to_addr_markable( expected );
		addr_markable addr_desired  = zip_tuple_to_addr_markable( desired );
		bool          ret           = a_target_addr_.compare_exchange_weak( addr_expected, addr_desired, order );
		if ( !ret ) {
			expected = unzip_addr_markable_to_tuple( addr_expected );
		}
		return ret;
	}

	bool compare_exchange_strong( std::tuple<pointer, bool>&       expected,
	                              const std::tuple<pointer, bool>& desired,
	                              std::memory_order                success = std::memory_order_acq_rel,
	                              std::memory_order                failure = std::memory_order_acquire ) noexcept
	{
		addr_markable addr_expected = zip_tuple_to_addr_markable( expected );
		addr_markable addr_desired  = zip_tuple_to_addr_markable( desired );
		bool          ret           = a_target_addr_.compare_exchange_strong( addr_expected, addr_desired, success, failure );
		if ( !ret ) {
			expected = unzip_addr_markable_to_tuple( addr_expected );
		}
		return ret;
	}

	bool compare_exchange_strong( std::tuple<pointer, bool>&       expected,
	                              const std::tuple<pointer, bool>& desired,
	                              std::memory_order                order = std::memory_order_seq_cst ) noexcept
	{
		addr_markable addr_expected = zip_tuple_to_addr_markable( expected );
		addr_markable addr_desired  = zip_tuple_to_addr_markable( desired );
		bool          ret           = a_target_addr_.compare_exchange_strong( addr_expected, addr_desired, order );
		if ( !ret ) {
			expected = unzip_addr_markable_to_tuple( addr_expected );
		}
		return ret;
	}

private:
	using addr_markable = std::uintptr_t;

	static addr_markable zip_tuple_to_addr_markable( const std::tuple<pointer, bool>& tp )
	{
		// reinterpret_castはconstexprの中では使えない。
		addr_markable ans = reinterpret_cast<addr_markable>( std::get<0>( tp ) );
		if ( std::get<1>( tp ) ) {
			ans |= static_cast<addr_markable>( 1U );
		} /*
		else {
		    ans &= ~(static_cast<addr_markable>(1U));
		} */
		return ans;
	}
	static std::tuple<pointer, bool> unzip_addr_markable_to_tuple( addr_markable addr )
	{
		// reinterpret_castはconstexprの中では使えない。
		pointer p_ans = reinterpret_cast<pointer>( addr & ( ~( static_cast<addr_markable>( 1U ) ) ) );
		bool    b_ans = ( ( addr & static_cast<addr_markable>( 1U ) ) == 0 ) ? false : true;
		return std::tuple<pointer, bool> { p_ans, b_ans };
	}

	std::atomic<addr_markable> a_target_addr_;
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_HAZARD_PTR_HPP_ */
