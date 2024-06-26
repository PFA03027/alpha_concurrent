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

#ifndef SRC_HAZARD_PTR_HPP_
#define SRC_HAZARD_PTR_HPP_

#include <pthread.h>

#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <thread>

#include "dynamic_tls.hpp"
#include "internal/alloc_only_allocator.hpp"

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
class hazard_ptr {
public:
	using hzrd_type                       = T;
	using hzrd_pointer                    = T*;
	static constexpr size_t hzrd_max_slot = N;
	static_assert( N > 0, "N should be greater than 0(zero)" );

	constexpr hazard_ptr( void )
	  : my_allocator_( true, 4 * 1024 )
	  , p_allocator_( &my_allocator_ )
	  , head_( p_allocator_ )
	  , p_hzd_ptr_node_( threadlocal_handler_functor( this ) )
	{
		static_assert( std::is_standard_layout<hazard_ptr>::value, "hazard_ptr should be standard-layout type" );
	}

	constexpr hazard_ptr( internal::alloc_only_chamber* p_allocator_arg )
	  : my_allocator_( true, 0 )
	  , p_allocator_( p_allocator_arg )
	  , head_( p_allocator_ )
	  , p_hzd_ptr_node_( threadlocal_handler_functor( this ) )
	{
		static_assert( std::is_standard_layout<hazard_ptr>::value, "hazard_ptr should be standard-layout type" );
	}

	~hazard_ptr( void )
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
			return allocator_arg.allocate( n, sizeof( uintptr_t ) );
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
		threadlocal_handler_functor( hazard_ptr* p_node_list_owner_arg )
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

		hazard_ptr* p_node_list_owner_;
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
 * @brief	scoped reference control support class for hazard_ptr
 *
 * スコープベースでの、参照権の解放制御をサポートするクラスの実体定義クラス
 */
template <typename T, int N>
class hazard_ptr_scoped_ref {
public:
	hazard_ptr_scoped_ref( hazard_ptr<T, N>& ref, int idx_arg )
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
	int                                             idx_;
	typename hazard_ptr<T, N>::node_for_hazard_ptr* p_node_hzd_ptr_;
};

#if 0
/*!
 * @brief	推論補助
 */
template <class HZD_PTR>
hazard_ptr_scoped_ref( HZD_PTR&, int ) -> hazard_ptr_scoped_ref<typename HZD_PTR::hzrd_type, HZD_PTR::hzrd_max_slot>;
#endif
}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_HAZARD_PTR_HPP_ */
