/*!
 * @file	free_node_storage.hpp
 * @brief
 * @author	Teruaki Ata
 * @date	Created on 2020/12/31
 * @details
 *
 * Copyright (C) 2020 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef SRC_FREE_NODE_STORAGE_HPP_
#define SRC_FREE_NODE_STORAGE_HPP_

#include <atomic>
#include <memory>

#include "hazard_ptr.hpp"
#ifdef USE_LOCK_FREE_MEM_ALLOC
#include "lf_mem_alloc.hpp"
#endif

namespace alpha {
namespace concurrent {

namespace internal {
/*!
 * @breif	lock freeで使用するノードの基本クラス
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
		return next_[(int)cur_slot_idx].compare_exchange_weak( *pp_expect_ptr, p_desired_ptr );
	}

#ifdef USE_LOCK_FREE_MEM_ALLOC
	void* operator new( std::size_t n );             // usual new...(1)
	void  operator delete( void* p_mem ) noexcept;   // usual new...(2)

	void* operator new[]( std::size_t n );             // usual new...(1)
	void  operator delete[]( void* p_mem ) noexcept;   // usual new...(2)

	void* operator new( std::size_t n, void* p );          // placement new
	void  operator delete( void* p, void* p2 ) noexcept;   // placement delete...(3)

	static std::list<chunk_statistics> get_statistics(void);
#endif

private:
	node_of_list( const node_of_list& ) = delete;
	node_of_list( node_of_list&& )      = delete;
	node_of_list operator=( const node_of_list& ) = delete;
	node_of_list operator=( node_of_list&& ) = delete;

	std::atomic<node_of_list*> next_[2];
};

/*!
 * @breif	フリーノードを管理するクラスで使用するスレッドローカルストレージのリスト型のFIFOキュークラス
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
	thread_local_fifo_list( const thread_local_fifo_list& ) = delete;
	thread_local_fifo_list( thread_local_fifo_list&& )      = delete;
	thread_local_fifo_list operator=( const thread_local_fifo_list& ) = delete;
	thread_local_fifo_list operator=( thread_local_fifo_list&& ) = delete;

	static constexpr node_of_list::next_slot_idx next_slot_idx_ = node_of_list::next_slot_idx::TL_LIST_SLOT;

	node_pointer head_;
	node_pointer tail_;
};

/*!
 * @breif	フリーノードを管理するクラスの基本となるリスト型のFIFOキュークラス
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
	 * @breif	FIFOキューからノードを取り出す。
	 *
	 * @return	取り出された管理ノードへのポインタ。nullptrの場合、FIFOキューが空だったことを示す。管理対象ポインタは、
	 */
	node_pointer pop( void );

	bool check_hazard_list( node_pointer const p_chk_node );

private:
	fifo_free_nd_list( const fifo_free_nd_list& ) = delete;
	fifo_free_nd_list( fifo_free_nd_list&& )      = delete;
	fifo_free_nd_list operator=( const fifo_free_nd_list& ) = delete;
	fifo_free_nd_list operator=( fifo_free_nd_list&& ) = delete;

	using hazard_ptr_storage = hazard_ptr<node_of_list, hzrd_max_slot_>;
	using scoped_hazard_ref  = hazard_ptr_scoped_ref<node_of_list, hzrd_max_slot_>;

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

	hazard_ptr_storage hzrd_ptr_;
};

/*!
 * @breif	フリーノードを管理するクラス
 */
class free_nd_storage {
public:
	using node_type    = node_of_list;
	using node_pointer = node_type*;

	free_nd_storage( void );

	~free_nd_storage();

	/*!
	 * @breif	ローカルストレージのノードリストからフリーノードリストへの登録を試みる
	 *
	 * @return	フリーノードリストへの登録の試みが実施できたかどうかを返す。
	 * @retval	true	ローカルストレージのノードリストにリストが残っていて、フリーノードリストへの登録を試みることができた。
	 * @retval	false	ローカルストレージのノードリストが空で、フリーノードリストへの登録を試みることはできなかった。
	 */
	bool recycle( node_pointer p_retire_node );

	/*!
	 * @breif	フリーノードを取り出す。
	 *
	 * フリーノードがなければ、ヒープからメモリをallocate()する。
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

		for ( int i = 0; i < num_recycle_exec; i++ ) {
			node_pointer p_ans = node_list_.pop();
			if ( p_ans == nullptr ) {
				if ( recycle( nullptr ) ) {
					continue;
				} else {
					break;   // ローカルストレージが空で、かつフリーノードリストも空なので、フリーノードを取り出せる可能性は低い。よって、ループを抜ける。
				}
			}

			ALLOC_NODE_T* p_down_casted_ans = dynamic_cast<ALLOC_NODE_T*>( p_ans );   // TODO: lock free ?
			if ( p_down_casted_ans != nullptr ) {
				if ( pred( p_down_casted_ans ) ) {
					return p_down_casted_ans;
				} else {
					recycle( p_ans );
				}
			} else {
				// もし、ダウンキャストに失敗したら、そもそも不具合であるし、不要なので、削除する。
				LogOutput( log_type::ERR, "Info: fail to down cast. discard the node that have unexpected type." );
				delete p_ans;
			}
		}

		if ( does_allow_allocate ) {
			// ここに来たら、利用可能なfree nodeがなかったこと示すので、新しいノードをヒープから確保する。
			return allocate_new_node<ALLOC_NODE_T>();
		} else {
			return nullptr;
		}
	}

	template <typename ALLOC_NODE_T>
	void init_and_pre_allocate( int pre_alloc_nodes )
	{
		node_list_.initial_push( (node_pointer)allocate_new_node<ALLOC_NODE_T>() );

		for ( int i = 0; i < pre_alloc_nodes; i++ ) {
			recycle( (node_pointer)allocate_new_node<ALLOC_NODE_T>() );
		}
	}

	int get_allocated_num( void );

private:
	free_nd_storage( const free_nd_storage& ) = delete;
	free_nd_storage( free_nd_storage&& )      = delete;
	free_nd_storage operator=( const free_nd_storage& ) = delete;
	free_nd_storage operator=( free_nd_storage&& ) = delete;

	template <typename ALLOC_NODE_T>
	inline ALLOC_NODE_T* allocate_new_node( void )
	{
		static_assert( std::is_base_of<node_of_list, ALLOC_NODE_T>::value == true, "ALLOC_NODE_T is base of node_of_list" );

		LogOutput( log_type::DEBUG, "allocated new node" );

		allocated_node_count_++;
		return new ALLOC_NODE_T();
	}

	static void destr_fn( void* parm );

	inline thread_local_fifo_list* check_local_storage( void )
	{
		thread_local_fifo_list* p_ans = &( tls_fifo_.get_tls_instance() );
		return p_ans;
	}

	static constexpr int num_recycle_exec = 16;   //!< recycle処理を行うノード数。処理量を一定化するために、ループ回数を定数化する。２以上とする事。だいたいCPU数程度にすればよい。

	std::atomic<int> allocated_node_count_;

	fifo_free_nd_list node_list_;

	dynamic_tls<thread_local_fifo_list> tls_fifo_;

	//	friend pthread_key_create;
};

}   // namespace internal

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_LF_FIFO_HPP_ */
