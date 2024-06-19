/*!
 * @file	free_node_storage.hpp
 * @brief	free node strage class for lock-free data structure
 * @author	Teruaki Ata
 * @date	Created on 2020/12/31
 * @details
 *
 * Copyright (C) 2020 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef ALCONCURRENT_INC_INTERNAL_FREE_NODE_STORAGE_HPP_
#define ALCONCURRENT_INC_INTERNAL_FREE_NODE_STORAGE_HPP_

#include <atomic>
#include <memory>
#include <mutex>

#include "../hazard_ptr.hpp"
#include "../lf_mem_alloc.hpp"

#include "od_node_base.hpp"
#include "retire_mgr.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

class free_nd_storage;

/*!
 * @brief	lock freeで使用するノードの基本クラス
 *
 * このヘッダファイルで提供する free_node_storage を使用する場合、
 * 利用者側が使用するクラスでのノードクラスをこのクラスから派生させること。
 */
struct node_of_list {
	enum class next_slot_idx : int {
		FREE_ND_LIST_SLOT = 0,   //!<	next slot index for free node list
		TL_LIST_SLOT      = 1    //!<	next slot index for thread local list
	};

	node_of_list( void )
	{
		for ( auto& e : next_ ) {
			e.store( nullptr, std::memory_order_release );
		}
	}

	virtual ~node_of_list()
	{
		return;
	}

	node_of_list* get_next( next_slot_idx cur_slot_idx )
	{
		return next_[(int)cur_slot_idx].load( std::memory_order_acquire );
	}

	void set_next( node_of_list* p_new_next, next_slot_idx cur_slot_idx )
	{
		next_[(int)cur_slot_idx].store( p_new_next, std::memory_order_release );
		return;
	}

	bool next_CAS( node_of_list** pp_expect_ptr, node_of_list* p_desired_ptr, next_slot_idx cur_slot_idx )
	{
		return next_[(int)cur_slot_idx].compare_exchange_weak( *pp_expect_ptr, p_desired_ptr, std::memory_order_acq_rel );
	}

	virtual void release_ownership( void );
	virtual void teardown_by_recycle( void );

	void* operator new( std::size_t n );                               // usual new...(1)
	void* operator new( std::size_t n, std::align_val_t alignment );   // usual new with alignment...(1) C++11/C++14 just ignore. C++17 and after uses this.
	void  operator delete( void* p_mem ) noexcept;                     // usual delete...(2)

	void* operator new[]( std::size_t n );             // usual new...(1)
	void  operator delete[]( void* p_mem ) noexcept;   // usual delete...(2)

	void* operator new( std::size_t n, void* p );          // placement new
	void  operator delete( void* p, void* p2 ) noexcept;   // placement delete...(3)

	static general_mem_allocator_statistics get_statistics( void );

private:
	node_of_list( const node_of_list& )           = delete;
	node_of_list( node_of_list&& )                = delete;
	node_of_list operator=( const node_of_list& ) = delete;
	node_of_list operator=( node_of_list&& )      = delete;

	std::atomic<node_of_list*> next_[2];
};

/*!
 * @brief	フリーノードを管理するクラスで使用するスレッドローカルストレージのリスト型のFIFOキュークラス
 */
class thread_local_fifo_list {
public:
	using node_type    = node_of_list;
	using node_pointer = node_type*;

	thread_local_fifo_list( void );

	~thread_local_fifo_list();

	void push( node_pointer const p_push_node );

	node_pointer pop( void );

	inline bool is_empty( void )
	{
		return head_ == nullptr;
	}

private:
	thread_local_fifo_list( const thread_local_fifo_list& )           = delete;
	thread_local_fifo_list( thread_local_fifo_list&& )                = delete;
	thread_local_fifo_list operator=( const thread_local_fifo_list& ) = delete;
	thread_local_fifo_list operator=( thread_local_fifo_list&& )      = delete;

	static constexpr node_of_list::next_slot_idx next_slot_idx_ = node_of_list::next_slot_idx::TL_LIST_SLOT;

	node_pointer head_;
	node_pointer tail_;
};

/*!
 * @brief	フリーノードを管理するクラスの基本となるリスト型のFIFOキュークラス
 *
 * @note
 * https://www.slideshare.net/kumagi/lock-free-safe?next_slideshow=1 @n
 * 明記はされていないが、必ずノードが１つ残る構造。
 */
class fifo_free_nd_list {
public:
	static constexpr int hzrd_max_slot_ = 5;
	using node_type                     = node_of_list;
	using node_pointer                  = node_type*;

	fifo_free_nd_list( void );

	~fifo_free_nd_list();

	void initial_push( node_pointer const p_push_node );

	void push( node_pointer const p_push_node );

	/*!
	 * @brief	FIFOキューからノードを取り出す。
	 *
	 * @return	取り出された管理ノードへのポインタ。nullptrの場合、FIFOキューが空だったことを示す。管理対象ポインタは、
	 */
	node_pointer pop( void );

	bool check_hazard_list( node_pointer const p_chk_node );

private:
	fifo_free_nd_list( const fifo_free_nd_list& )           = delete;
	fifo_free_nd_list( fifo_free_nd_list&& )                = delete;
	fifo_free_nd_list operator=( const fifo_free_nd_list& ) = delete;
	fifo_free_nd_list operator=( fifo_free_nd_list&& )      = delete;

	using hazard_ptr_storage_t = hazard_ptr_storage<node_of_list, hzrd_max_slot_>;
	using scoped_hazard_ref    = hazard_ptr_scoped_ref<node_of_list, hzrd_max_slot_>;

	enum class hazard_ptr_idx : int {
		PUSH_FUNC_LAST = 0,
		PUSH_FUNC_NEXT = 1,
		POP_FUNC_FIRST = 2,
		POP_FUNC_LAST  = 3,
		POP_FUNC_NEXT  = 4
	};

	static constexpr node_of_list::next_slot_idx next_slot_idx_ = node_of_list::next_slot_idx::FREE_ND_LIST_SLOT;
	std::atomic<node_pointer>                    head_;
	std::atomic<node_pointer>                    tail_;

	hazard_ptr_storage_t hzrd_ptr_;
};

/*!
 * @brief	フリーノードを管理するクラス
 */
class free_nd_storage {
public:
	using node_type    = node_of_list;
	using node_pointer = node_type*;

	free_nd_storage( void );

	~free_nd_storage();

	/*!
	 * @brief	使用済みノードのリサイクル処理を行う。
	 *
	 * まず、スレッドローカルストレージへノードを一時登録した後、ハザードポインタをチェックする。
	 * ハザードポインタに登録されていなければ、teardown_by_recycle()を呼び出した後、共通フリーノードリストへの登録処理を行う。
	 *
	 * @return	共通フリーノードリストへの登録の試みが実施できたかどうかを返す。
	 * @retval	true	ローカルストレージのノードリストにリストが残っていて、フリーノードリストへの登録を試みることができた。
	 * @retval	false	ローカルストレージのノードリストが空で、フリーノードリストへの登録を試みることはできなかった。
	 */
	bool recycle( node_pointer p_retire_node );

	/*!
	 * @brief	フリーノードを取り出す。
	 *
	 * フリーノードリストからallocate()する場合は、setup_by_alloc()を呼び出す。
	 *
	 * フリーノードがなければ、ヒープからメモリをallocate()する。この場合は、setup_by_alloc()相当処理は、コンストラクタ内で実施されていると想定。
	 *
	 * pred は、引数で指定されたノードポインタに対し、呼び出し側から見た使用可否を判定する関数オブジェクト
	 * @li	true 使用可能
	 * @li	false 使用不可能
	 *
	 * pred が、trueを返したフリーノードをallocateするか、見つからなければ、ヒープメモリから新しいノードを確保する。
	 *
	 * predは、下記の呼び出しができること
	 * @li	bool pred(node_pointer p_chk_node)
	 *
	 * ALLOC_NODE_T: 実際にノードとしてallocateする具象クラスを指定する。また、具象クラスは node_of_list からの派生クラスでなければならない。
	 *
	 */
	template <typename ALLOC_NODE_T, typename TFUNC>
	ALLOC_NODE_T* allocate( bool does_allow_allocate, TFUNC pred )
	{
		static_assert( std::is_base_of<node_of_list, ALLOC_NODE_T>::value, "incorrect node type is required. it needs the derived class from node_of_list" );

		auto chk_and_recycle = [this, &pred]( node_pointer p_chk ) -> ALLOC_NODE_T* {
			ALLOC_NODE_T* p_down_casted_ans = dynamic_cast<ALLOC_NODE_T*>( p_chk );   // TODO: lock free ?
			if ( p_down_casted_ans != nullptr ) {
				if ( pred( p_down_casted_ans ) ) {
					return p_down_casted_ans;
				} else {
					this->recycle( p_chk );
				}
			} else {
				// もし、ダウンキャストに失敗したら、そもそも不具合であるし、不要なので、削除する。
				internal::LogOutput( log_type::ERR, "ERROR: fail to down cast. discard the node that have unexpected type." );
				delete p_chk;
			}
			return nullptr;
		};

		{
			std::unique_lock<std::mutex> lk( mtx_rcv_thread_local_fifo_list_, std::try_to_lock );
			if ( lk.owns_lock() ) {
				// 他のスレッドから預託されたノードをフリーノードへ差し回す。
				node_pointer p_ans = rcv_thread_local_fifo_list_.pop();
				if ( p_ans != nullptr ) {
					recycle( p_ans );
				}
			}
		}

		for ( int i = 0; i < num_recycle_exec; i++ ) {
			node_pointer p_ans = node_list_.pop();
			if ( p_ans == nullptr ) {
				if ( recycle( nullptr ) ) {
					continue;
				} else {
					break;   // ローカルストレージが空で、かつフリーノードリストも空なので、フリーノードを取り出せる可能性は低い。よって、ループを抜ける。
				}
			}

			ALLOC_NODE_T* p_down_casted_ans = chk_and_recycle( p_ans );
			if ( p_down_casted_ans != nullptr ) {
				return p_down_casted_ans;
			}
		}

		if ( does_allow_allocate ) {
			// ここに来たら、利用可能なfree nodeがなかったこと示すので、新しいノードをヒープから確保する。setup_by_allocは実行済み
			return allocate_new_node<ALLOC_NODE_T>();
		} else {
			return nullptr;
		}
	}

	template <typename ALLOC_NODE_T>
	void init_and_pre_allocate( size_t pre_alloc_nodes )
	{
		node_pointer p_init_sentinel = (node_pointer)allocate_new_node<ALLOC_NODE_T>();
		p_init_sentinel->teardown_by_recycle();
		node_list_.initial_push( p_init_sentinel );

		for ( size_t i = 0; i < pre_alloc_nodes; i++ ) {
			recycle( (node_pointer)allocate_new_node<ALLOC_NODE_T>() );
		}
	}

	int get_allocated_num( void );

private:
	free_nd_storage( const free_nd_storage& )           = delete;
	free_nd_storage( free_nd_storage&& )                = delete;
	free_nd_storage operator=( const free_nd_storage& ) = delete;
	free_nd_storage operator=( free_nd_storage&& )      = delete;

	struct rcv_fifo_list_handler {
		rcv_fifo_list_handler( free_nd_storage* p_fns_arg )
		  : p_fns_( p_fns_arg )
		{
		}

		uintptr_t allocate( void )
		{
			return reinterpret_cast<uintptr_t>( new thread_local_fifo_list() );
		}

		void deallocate( uintptr_t p_destructing_tls )
		{
			thread_local_fifo_list* p_tls_list = reinterpret_cast<thread_local_fifo_list*>( p_destructing_tls );
			p_fns_->rcv_thread_local_fifo_list( p_tls_list );
			delete p_tls_list;
		}

		free_nd_storage* p_fns_;
	};

	template <typename ALLOC_NODE_T>
	inline ALLOC_NODE_T* allocate_new_node( void )
	{
		static_assert( std::is_base_of<node_of_list, ALLOC_NODE_T>::value == true, "ALLOC_NODE_T is base of node_of_list" );

		internal::LogOutput( log_type::DEBUG, "allocated new node" );

		allocated_node_count_++;
		return new ALLOC_NODE_T();
	}

	inline thread_local_fifo_list* check_local_storage( void )
	{
		thread_local_fifo_list* p_ans = &( tls_fifo_.get_tls_instance() );
		return p_ans;
	}

	/**
	 * @brief スレッド終了時にスレッドローカルリストに残留しているノードを引き取る
	 *
	 * 引き取るのはノードだけで、リストの管理構造体p_rcvはそのまま残るため、必要に応じて破棄する必要あり。
	 *
	 * @param p_rcv フリーノードとして開放待ちとなっているスレッドローカルリストへのポインタ
	 *
	 * @note
	 * スレッド終了時に呼び出される関数であるため、内部でmutexによる排他制御ロックが行われる。
	 */
	void rcv_thread_local_fifo_list( thread_local_fifo_list* p_rcv );

	static constexpr int num_recycle_exec = 16;   //!< recycle処理を行うノード数。処理量を一定化するために、ループ回数を定数化する。２以上とする事。だいたいCPU数程度にすればよい。

	std::mutex             mtx_rcv_thread_local_fifo_list_;
	thread_local_fifo_list rcv_thread_local_fifo_list_;

	std::atomic<int> allocated_node_count_;

	fifo_free_nd_list node_list_;

	dynamic_tls<thread_local_fifo_list, rcv_fifo_list_handler> tls_fifo_;   //!< dynamic_tlsで指定しているrcv_fifo_list_handlerによって、デストラクタ実行時にrcv_thread_local_fifo_list_に残留データをpushする。そのため、メンバ変数rcv_thread_local_fifo_list_よりも後に変数宣言を行っている。
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief node of one direction list
 *
 * @tparam T value type kept in this class
 */
template <typename T>
class od_node : public od_node_base<od_node<T>> {
public:
	using value_type           = T;
	using reference_type       = T&;
	using const_reference_type = const T&;

	template <bool IsDefaultConstructible = std::is_default_constructible<value_type>::value, typename std::enable_if<IsDefaultConstructible>::type* = nullptr>
	od_node( od_node* p_next_arg ) noexcept( std::is_nothrow_default_constructible<value_type>::value )
	  : od_node_base<od_node>()
	  , v_ {}
	{
		od_node_base<od_node<T>>::hph_next_.store( p_next_arg );
	}

	template <bool IsCopyable = std::is_copy_constructible<value_type>::value, typename std::enable_if<IsCopyable>::type* = nullptr>
	od_node( od_node* p_next_arg, const value_type& v_arg ) noexcept( std::is_nothrow_copy_constructible<value_type>::value )
	  : od_node_base<od_node>()
	  , v_( v_arg )
	{
		od_node_base<od_node<T>>::hph_next_.store( p_next_arg );
	}

	template <bool IsMovable = std::is_move_constructible<value_type>::value, typename std::enable_if<IsMovable>::type* = nullptr>
	od_node( od_node* p_next_arg, value_type&& v_arg ) noexcept( std::is_nothrow_move_constructible<value_type>::value )
	  : od_node_base<od_node>()
	  , v_( std::move( v_arg ) )
	{
		od_node_base<od_node<T>>::hph_next_.store( p_next_arg );
	}

	template <typename Arg1st, typename... RemainingArgs,
	          typename RemoveCVArg1st                                                          = typename std::remove_reference<typename std::remove_const<Arg1st>::type>::type,
	          typename std::enable_if<!std::is_same<RemoveCVArg1st, value_type>::value>::type* = nullptr>
	od_node( od_node* p_next_arg, Arg1st&& arg1, RemainingArgs&&... args )
	  : od_node_base<od_node>()
	  , v_( std::forward<Arg1st>( arg1 ), std::forward<RemainingArgs>( args )... )
	{
		od_node_base<od_node<T>>::hph_next_.store( p_next_arg );
	}

	template <bool IsCopyable = std::is_copy_assignable<value_type>::value, typename std::enable_if<IsCopyable>::type* = nullptr>
	void set( const value_type& v_arg, od_node* p_next_arg ) noexcept( std::is_nothrow_copy_assignable<value_type>::value )
	{
		v_ = v_arg;
		od_node_base<od_node<T>>::hph_next_.store( p_next_arg );
	}

	template <bool IsMovable = std::is_move_assignable<value_type>::value, typename std::enable_if<IsMovable>::type* = nullptr>
	void set( value_type&& v_arg, od_node* p_next_arg ) noexcept( std::is_nothrow_move_assignable<value_type>::value )
	{
		v_ = std::move( v_arg );
		od_node_base<od_node<T>>::hph_next_.store( p_next_arg );
	}

	reference_type get( void ) &
	{
		return v_;
	}

	const_reference_type get( void ) const&
	{
		return v_;
	}

	template <bool IsMovable = std::is_move_assignable<value_type>::value, typename std::enable_if<IsMovable>::type* = nullptr>
	value_type get( void ) &&
	{
		return std::move( v_ );
	}

	template <bool IsCopyable                                          = std::is_copy_assignable<value_type>::value,
	          bool IsMovable                                           = std::is_move_assignable<value_type>::value,
	          typename std::enable_if<!IsMovable && IsCopyable>::type* = nullptr>
	value_type get( void ) const&&
	{
		return v_;
	}

private:
	value_type v_;
};

template <typename T>
using od_node_list = od_node_list_base<od_node<T>>;

template <typename T>
using od_node_list_lockable = od_node_list_lockable_base<od_node_list<T>>;

template <typename T>
using od_node_list_lockfree = od_node_list_lockfree_base<od_node<T>>;

template <typename T>
class x_free_od_node_storage {
public:
	using node_type            = od_node<T>;
	using value_type           = typename od_node<T>::value_type;
	using node_pointer         = od_node<T>*;
	using hazard_ptr_handler_t = typename od_node<T>::hazard_ptr_handler_t;

	// set minimum capacity
	void reserve_minimum( size_t reserve_size ) noexcept
	{
		if ( x_free_od_node_storage<T>::g_fn_list_capacity_.load( std::memory_order_acquire ) >= reserve_size ) {
			return;
		}
		if ( reserve_size < 2 ) {
			reserve_size = 2;
		}

		{
			node_pointer p_pre_head = nullptr;
			size_t       i          = 0;
			try {
				for ( i = 0; i < reserve_size / 2; i++ ) {
					node_pointer p_new_head = new node_type { p_pre_head, value_type {} };
					p_pre_head              = p_new_head;
				}
			} catch ( ... ) {
				LogOutput( log_type::ERR, "reserve_minimum finished partly" );
			}
			x_free_od_node_storage<T>::g_fn_list_.lock().ref().merge_push_front( p_pre_head );
			x_free_od_node_storage<T>::g_fn_list_help_flag_.store( false );
			x_free_od_node_storage<T>::g_fn_list_capacity_.fetch_add( i );
		}
		{
			size_t i = 0;
			try {
				for ( i = 0; i < reserve_size / 2; i++ ) {
					node_pointer p_new_head = new node_type { nullptr, value_type {} };
					x_free_od_node_storage<T>::g_fn_list_lockfree_.push_front( p_new_head );
				}
			} catch ( ... ) {
				LogOutput( log_type::ERR, "reserve_minimum finished partly" );
			}
			x_free_od_node_storage<T>::g_fn_list_lockfree_help_flag_.store( false );
			x_free_od_node_storage<T>::g_fn_list_capacity_.fetch_add( i );
		}
	}

	bool recycle( node_pointer p_retire_node )
	{
		internal::retire_mgr::retire( p_retire_node, recycler( &( x_free_od_node_storage<T>::tl_fn_list_ ) ) );

		return false;
	}
	template <bool IsCopyable = std::is_copy_constructible<value_type>::value && std::is_copy_assignable<value_type>::value, typename std::enable_if<IsCopyable>::type* = nullptr>
	node_pointer allocate( const value_type& init_v_arg, node_pointer p_next_node_arg = nullptr )
	{
		// リサイクルストレージからノードを取り出す
		node_pointer p_ans = allocate_from_recycle_storage();
		if ( p_ans != nullptr ) {
			p_ans->set( init_v_arg, p_next_node_arg );
			return p_ans;
		}

		// ノードをリサイクルできなかったので、ノードをアロケートする
		x_free_od_node_storage<T>::g_fn_list_capacity_.fetch_add( 1 );
		return new node_type { p_next_node_arg, init_v_arg };
	}

	template <bool IsMovable = std::is_move_constructible<value_type>::value && std::is_move_assignable<value_type>::value, typename std::enable_if<IsMovable>::type* = nullptr>
	node_pointer allocate( value_type&& init_v_arg, node_pointer p_next_node_arg = nullptr )
	{
		// リサイクルストレージからノードを取り出す
		node_pointer p_ans = allocate_from_recycle_storage();
		if ( p_ans != nullptr ) {
			p_ans->set( std::move( init_v_arg ), p_next_node_arg );
			return p_ans;
		}

		// ノードをリサイクルできなかったので、ノードをアロケートする
		x_free_od_node_storage<T>::g_fn_list_capacity_.fetch_add( 1 );
		return new node_type { p_next_node_arg, std::move( init_v_arg ) };
	}

	static size_t allocated_size( void )
	{
		return x_free_od_node_storage<T>::g_fn_list_capacity_.load( std::memory_order_acquire );
	}

private:
	/**
	 * @brief リサイクル用のストレージからノードのリサイクルを試みる
	 *
	 * @return node_pointer リサイクルストレージから取り出したノード
	 * @retval nullptr ストレージからノードを取り出せなかった。
	 */
	node_pointer allocate_from_recycle_storage() noexcept
	{
		// スレッドローカルストレージからノードをリサイクルする。
		node_pointer p_ans = x_free_od_node_storage<T>::tl_fn_list_.node_list_.pop_front();
		if ( p_ans != nullptr ) {
			return p_ans;
		}

		// グローバルストレージのロック付きリストからノードをリサイクルする。
		auto lk = x_free_od_node_storage<T>::g_fn_list_.try_lock();
		if ( lk.owns_lock() ) {
			p_ans = lk.ref().pop_front();
			if ( p_ans != nullptr ) {
				return p_ans;
			}
			x_free_od_node_storage<T>::g_fn_list_help_flag_.store( true, std::memory_order_release );
		}

		// グローバルストレージのロックフリーリストからノードをリサイクルする。
		p_ans = x_free_od_node_storage<T>::g_fn_list_lockfree_.pop_front();
		while ( p_ans != nullptr ) {
			if ( !internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( p_ans ) ) {
				return p_ans;
			}
			internal::retire_mgr::retire_always_store( p_ans, recycler( &( x_free_od_node_storage<T>::tl_fn_list_ ) ) );
			p_ans = x_free_od_node_storage<T>::g_fn_list_lockfree_.pop_front();
		}
		x_free_od_node_storage<T>::g_fn_list_lockfree_help_flag_.store( true, std::memory_order_release );

		return p_ans;
	}

	/**
	 * @brief スレッドローカルストレージ専用クラス
	 *
	 * @note
	 * このクラスの変数宣言はグローバル変数専用クラスの変数の直後に宣言すること。
	 * この制約は変数のデストラクタが呼ばれる順序を制御するため。
	 */
	class thread_local_od_node_list {
	public:
		constexpr thread_local_od_node_list( od_node_list_lockable<T>& center_list_arg )
		  : center_list_( center_list_arg )
		{
		}

		~thread_local_od_node_list()
		{
			center_list_.lock().ref().merge_push_front( std::move( node_list_ ) );
		}

		od_node_list<T> node_list_;

	private:
		od_node_list_lockable<T>& center_list_;
	};

	static od_node_list_lockfree<T>               g_fn_list_lockfree_;
	static std::atomic<bool>                      g_fn_list_lockfree_help_flag_;
	static od_node_list_lockable<T>               g_fn_list_;
	static std::atomic<bool>                      g_fn_list_help_flag_;
	static std::atomic<size_t>                    g_fn_list_capacity_;   // number of allocated nodes
	static thread_local thread_local_od_node_list tl_fn_list_;

	class recycler {
	public:
		constexpr recycler( void ) noexcept
		  : p_recycle_tl_target_( nullptr )
		{
		}
		constexpr recycler( thread_local_od_node_list* p_recycle_tl_target_arg ) noexcept
		  : p_recycle_tl_target_( p_recycle_tl_target_arg )
		{
		}
		constexpr recycler( const recycler& ) noexcept            = default;
		constexpr recycler( recycler&& ) noexcept                 = default;
		constexpr recycler& operator=( const recycler& ) noexcept = default;
		constexpr recycler& operator=( recycler&& ) noexcept      = default;
		~recycler()                                               = default;

		void operator()( node_pointer p_nd )
		{
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_PUSH_FRONT_FUNCTION_NULLPTR
			p_nd->hph_next_.store( nullptr );   // this line make slower the performance about 3%. therefore this line only enable in case of debug option
#endif
			thread_local_od_node_list* p_this_thread_instance = &( x_free_od_node_storage<T>::tl_fn_list_ );
			if ( p_this_thread_instance == p_recycle_tl_target_ ) {
				if ( p_recycle_tl_target_->node_list_.is_empty() ) {
					// recyclerが呼ばれたスレッドと、戻し先スレッドが同じで、かつローカルストレージが空ならノードを回す。
					p_recycle_tl_target_->node_list_.push_front( p_nd );
					return;
				}
			}

			// グローバルストレージのロックフリーリストでヘルプフラグが立っているので、そこにノードを回す。
			if ( x_free_od_node_storage<T>::g_fn_list_lockfree_help_flag_.load( std::memory_order_acquire ) ) {
				g_fn_list_lockfree_.push_front( p_nd );
				x_free_od_node_storage<T>::g_fn_list_lockfree_help_flag_.store( false, std::memory_order_release );
				return;
			}

			// グローバルストレージのロック付きリストでヘルプフラグが立っているので、そこにノードを回す。
			if ( x_free_od_node_storage<T>::g_fn_list_help_flag_.load( std::memory_order_acquire ) ) {
				auto lk = x_free_od_node_storage<T>::g_fn_list_.try_lock();
				if ( lk.owns_lock() ) {
					lk.ref().push_front( p_nd );
					x_free_od_node_storage<T>::g_fn_list_help_flag_.store( false, std::memory_order_release );
					return;
				}
			}

			if ( p_this_thread_instance == p_recycle_tl_target_ ) {
				// recyclerが呼ばれたスレッドと、戻し先スレッドが同じなので、そのままローカルストレージにノードを回す。
				p_recycle_tl_target_->node_list_.push_front( p_nd );
				return;
			} else {
				// 通常はこちらには来ない。
				// こちらに来るのは、retire_mgrのスレッドローカルストレージに残っていたまま、スレッドが終了した場合に
				// グローバルストレージに移管された後、別スレッドでrecycleが呼ばれた場合にここにくる。
				// あるいは、p_recycle_tl_target_ が nullptrの場合。
				auto lk = x_free_od_node_storage<T>::g_fn_list_.try_lock();
				if ( lk.owns_lock() ) {
					lk.ref().push_front( p_nd );
				} else {
					// ロックが取得できない場合、ロックフリーリストに、ノードをpushする。
					g_fn_list_lockfree_.push_front( p_nd );
					x_free_od_node_storage<T>::g_fn_list_lockfree_help_flag_.store( false, std::memory_order_release );
				}
			}
		}

	private:
		thread_local_od_node_list* p_recycle_tl_target_;
	};
};

template <typename T>
od_node_list_lockfree<T> x_free_od_node_storage<T>::g_fn_list_lockfree_;

template <typename T>
ALCC_INTERNAL_CONSTINIT std::atomic<bool> x_free_od_node_storage<T>::g_fn_list_lockfree_help_flag_( false );

template <typename T>
ALCC_INTERNAL_CONSTINIT od_node_list_lockable<T> x_free_od_node_storage<T>::g_fn_list_;

template <typename T>
ALCC_INTERNAL_CONSTINIT std::atomic<bool> x_free_od_node_storage<T>::g_fn_list_help_flag_( false );

template <typename T>
ALCC_INTERNAL_CONSTINIT std::atomic<size_t> x_free_od_node_storage<T>::g_fn_list_capacity_( 0 );

template <typename T>
thread_local ALCC_INTERNAL_CONSTINIT typename x_free_od_node_storage<T>::thread_local_od_node_list x_free_od_node_storage<T>::tl_fn_list_( x_free_od_node_storage<T>::g_fn_list_ );

}   // namespace internal

/*!
 * @brief	Set parameters in the lock-free memory allocator to enable the function.
 *
 * @warning This is obsolated. So there is no effect.
 */
#if __has_cpp_attribute( deprecated )
[[deprecated( "This is obsolated. So there is no effect." )]]
#endif
void set_param_to_free_nd_mem_alloc(
	const param_chunk_allocation* p_param_array,   //!< [in] pointer to parameter array
	unsigned int                  num              //!< [in] array size
);

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_LF_FIFO_HPP_ */
