/*!
 * @file	lf_mem_alloc_idx_mgr.hpp
 * @brief	internal component of semi lock-free memory allocator
 * @author	Teruaki Ata
 * @date	Created on 2021/05/18
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef INC_ALCONCURRENT_LF_MEM_ALLOC_IDX_MGR_HPP_
#define INC_ALCONCURRENT_LF_MEM_ALLOC_IDX_MGR_HPP_

#include <atomic>
#include <cstdlib>
#include <list>
#include <memory>

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/dynamic_tls.hpp"
#include "alconcurrent/hazard_ptr.hpp"

#include "alconcurrent/lf_mem_alloc_type.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

constexpr size_t MAX_ALLOC_SIZE_LIMIT = (size_t)( 2UL * 1024UL * 1024UL * 1024UL );
constexpr size_t NON_OWNERED_TL_ID    = 0;

/*!
 * @brief	chunk control status
 */
enum class chunk_control_status : int {
	EMPTY,                   //!< chunk header has no allocated chunk memory.
	RESERVED_ALLOCATION,     //!< chunk header has no allocated chunk memory. But some one start to allocation
	NORMAL,                  //!< allow to allocate the memory from this chunk
	RESERVED_DELETION,       //!< does not allow to allocate the memory from this chunk. But if needed to reuse this chunk, allow to change NORMAL
	ANNOUNCEMENT_DELETION,   //!< does not allow to allocate the memory from this chunk. And some one start to deletion trail.
	DELETION,                //!< does not allow to access any more except GC. After shift to this state, chunk memory will be free after confirmed accesser is zero.
};

/*!
 * @brief statistics information of alloc/dealloc
 *
 */
struct chunk_list_statistics {
	constexpr chunk_list_statistics( void )
	  : chunk_num_( 0 )
	  , valid_chunk_num_( 0 )
	  , total_slot_cnt_( 0 )
	  , free_slot_cnt_( 0 )
	  , consum_cnt_( 0 )
	  , max_consum_cnt_( 0 )
	  , alloc_req_cnt_( 0 )
	  , alloc_req_err_cnt_( 0 )
	  , dealloc_req_cnt_( 0 )
	  , dealloc_req_err_cnt_( 0 )
	  , alloc_collision_cnt_( 0 )
	  , dealloc_collision_cnt_( 0 )
	{
	}
	constexpr chunk_list_statistics( const chunk_list_statistics& )            = default;
	constexpr chunk_list_statistics( chunk_list_statistics&& )                 = default;
	constexpr chunk_list_statistics& operator=( const chunk_list_statistics& ) = default;
	constexpr chunk_list_statistics& operator=( chunk_list_statistics&& )      = default;

	chunk_statistics get_statistics( void ) const;

	std::atomic<unsigned int> chunk_num_;               //!< number of chunks
	std::atomic<unsigned int> valid_chunk_num_;         //!< number of valid chunks
	std::atomic<unsigned int> total_slot_cnt_;          //!< total slot count
	std::atomic<unsigned int> free_slot_cnt_;           //!< free slot count
	std::atomic<unsigned int> consum_cnt_;              //!< current count of allocated slots
	std::atomic<unsigned int> max_consum_cnt_;          //!< max count of allocated slots
	std::atomic<unsigned int> alloc_req_cnt_;           //!< allocation request count
	std::atomic<unsigned int> alloc_req_err_cnt_;       //!< fail count for allocation request
	std::atomic<unsigned int> dealloc_req_cnt_;         //!< deallocation request count
	std::atomic<unsigned int> dealloc_req_err_cnt_;     //!< fail count for deallocation request
	std::atomic<unsigned int> alloc_collision_cnt_;     //!< count of a collision of allocation in lock-free algorithm
	std::atomic<unsigned int> dealloc_collision_cnt_;   //!< count of a collision of deallocation in lock-free algorithm
};

/*!
 * @brief	使用可能なインデックス番号を管理する配列で使用する各要素のデータ構造
 *
 * 下記のスタック構造、あるいはリスト構造で管理される。
 * @li インデックスを持たない要素
 * @li インデックスを所持する要素
 * @li インデックスを持たないがハーザードポインタ登録中で滞留している要素
 */
struct idx_mgr_element {
	int                           idx_;                          //!< インデックス番号
	std::atomic<idx_mgr_element*> p_invalid_idx_next_element_;   //!< インデックス番号を所持しない要素をスタック構造で管理するための次要素へのポインタ
	std::atomic<idx_mgr_element*> p_valid_idx_next_element_;     //!< インデックス番号を所持する要素をスタック構造で管理するための次要素へのポインタ
	idx_mgr_element*              p_waiting_next_element_;       //!< ハザードポインタに登録されている間保持しておくスレッドローカルリストで使用する次のノードへのポインタ

	idx_mgr_element( void );
	void dump( void ) const;
};

/*!
 * @brief	ハーザードポインタ登録中で滞留している要素を管理するリスト
 *
 * スレッドローカルデータとして管理されるため、排他制御は不要なクラス
 */
struct waiting_element_list {
public:
	waiting_element_list( void );
	~waiting_element_list();

	idx_mgr_element* pop( void );
	void             push( idx_mgr_element* );

	void dump( void ) const;

private:
	idx_mgr_element* head_;
	idx_mgr_element* tail_;
};

/*!
 * @brief	インデックス管理スロットのロックフリーストレージクラス
 */
class idx_element_storage_mgr {
public:
	/*!
	 * @brief	コンストラクタ
	 */
	idx_element_storage_mgr(
		std::atomic<idx_mgr_element*> idx_mgr_element::*p_next_ptr_offset_arg,   //!< [in] nextポインタを保持しているメンバ変数へのメンバ変数ポインタ
		std::atomic<unsigned int>*                      p_collition_counter      //!< [in] 衝突が発生した回数を記録するアトミック変数へのポインタ
	);

	/*!
	 * @brief	ストレージから１つの要素を取り出す
	 *
	 * @return	pointer to the poped element
	 * @retval	nullptr	no available element
	 */
	idx_mgr_element* pop_element( void );

	/*!
	 * @brief	ストレージへ１つの要素を戻す
	 */
	void push_element(
		idx_mgr_element* p_push_element   //!< [in] pointer of element to push
	);

	/*!
	 * @brief	保持しているidx_mgr_elementをクリアする。
	 */
	void clear( void );

	/*!
	 * @brief	リスト操作時の衝突回数を取得する。
	 */
	int get_collision_cnt( void ) const
	{
		return p_collision_cnt_->load();
	}

	void dump( void ) const;

private:
	static constexpr int hzrd_max_slot_ = 3;
	enum class hazard_ptr_idx : int {
		PUSH_CUR = 0,
		POP_CUR  = 1,
		POP_NEXT = 2,
	};
	using scoped_hazard_ref = hazard_ptr_scoped_ref<idx_mgr_element, hzrd_max_slot_>;

	struct rcv_el_threadlocal_handler {
		rcv_el_threadlocal_handler( idx_element_storage_mgr* p_elst_arg )
		  : p_elst_( p_elst_arg )
		{
		}

		uintptr_t allocate( void )
		{
			return reinterpret_cast<uintptr_t>( new waiting_element_list() );
		}

		void deallocate( uintptr_t p_destructing_tls )
		{
			waiting_element_list* p_tmp = reinterpret_cast<waiting_element_list*>( p_destructing_tls );
			p_elst_->rcv_wait_element_by_thread_terminating( p_tmp );
			delete p_tmp;
		}

		idx_element_storage_mgr* p_elst_;
	};

	/*!
	 * @brief	ストレージから１つの要素を取り出す
	 *
	 * @return	pointer to the poped element
	 * @retval	nullptr	no available element
	 */
	idx_mgr_element* pop_element_from_list( void );

	/*!
	 * @brief	ストレージへ１つの要素を戻す
	 */
	void push_element_to_list(
		idx_mgr_element* p_push_element   //!< [in] pointer of element to push
	);

	/*!
	 * @brief	tls_waiting_list_のスレッドローカルストレージが破棄される際(=スレッドが終了する場合)に滞留中の要素を受け取る
	 *
	 * @note
	 * スレッドセーフではあるが、ロックによる排他制御が行われる。
	 */
	void rcv_wait_element_by_thread_terminating( waiting_element_list* p_el_list );

	std::mutex           mtx_rcv_wait_element_list_;                                   //!< tls_waiting_list_のスレッドローカルストレージが破棄される際に滞留中の要素を受け取るメンバ変数rcv_wait_element_list_の排他制御用Mutex
	waiting_element_list rcv_wait_element_list_;                                       //!< tls_waiting_list_のスレッドローカルストレージが破棄される際に滞留中の要素を受け取る

	dynamic_tls<waiting_element_list, rcv_el_threadlocal_handler> tls_waiting_list_;   //!< 滞留要素を管理するスレッドローカルリスト。dynamic_tlsで指定しているrcv_el_threadlocal_handlerによって、デストラクタ実行時にrcv_wait_element_list_に残留データをpushする。そのため、メンバ変数rcv_wait_element_list_よりも後に変数宣言を行っている。
	hazard_ptr<idx_mgr_element, hzrd_max_slot_>                   hzrd_element_;       //!< ハザードポインタを管理する構造体。

	std::atomic<idx_mgr_element*> head_;                                               //!< リスト構造のヘッドへのポインタ
	std::atomic<idx_mgr_element*> tail_;                                               //!< リスト構造の終端要素へのポインタ
	std::atomic<idx_mgr_element*> idx_mgr_element::*p_next_ptr_offset_;                //!< リスト構造の次のノードへのポインタを格納したメンバ変数へのメンバ変数ポインタ

	std::atomic<unsigned int>* p_collision_cnt_;                                       //!< ロックフリーアルゴリズム内で発生した衝突回数を記録する変数への参照
};

/*!
 * @brief	ハーザードポインタ登録中でindexを登録するフリーの要素がない場合に、滞留している要素を管理するリスト
 *
 * スレッドローカルデータとして管理されることを想定しているクラスであるため、排他制御を行わないクラス。
 *
 * @note
 * スレッドローカルストレージは、情報の伝搬が難しいため、バッファの再構築の要否を判定するために、バッファ構築のバージョン情報を使用する方式を採る。
 */
struct waiting_idx_list {
public:
	waiting_idx_list( const int idx_buff_size_arg, const int ver_arg );
	~waiting_idx_list();

	int  pop_from_tls( const int idx_buff_size_arg, const int ver_arg );
	void push_to_tls( const int valid_idx, const int idx_buff_size_arg, const int ver_arg );

	void dump( void ) const;

private:
	int  ver_;
	int  idx_buff_size_;
	int  idx_top_idx_;
	int* p_idx_buff_;

	void chk_reset_and_set_size( const int idx_buff_size_arg, const int ver_arg );
};

/*!
 * @brief	使用可能なインデックス番号を管理する準ロックフリークラス
 */
struct idx_mgr {
	/*!
	 * @brief	コンストラクタ
	 */
	idx_mgr(
		const int                  idx_size_arg,                 //!< [in] 用意するインデックス番号の数。0以下の場合、割り当てを保留する。
		std::atomic<unsigned int>* p_alloc_collision_cnt_arg,    //!< [in] 衝突回数をカウントする変数へのポインタ。ポインタ先のインスタンスは、このインスタンス以上のライフタイムを持つこと
		std::atomic<unsigned int>* p_dealloc_collision_cnt_arg   //!< [in] 衝突回数をカウントする変数へのポインタ。ポインタ先のインスタンスは、このインスタンス以上のライフタイムを持つこと
	);

	/*!
	 * @brief	デストラクタ
	 */
	~idx_mgr();

	/*!
	 * @brief	用意するインデックス番号の数を設定する。
	 *
	 * 指定された数のインデックス番号を構築する。 @n
	 * 既に設定済みの場合、現時点までの状態を破棄し、指定された数のインデックス番号を再度構築し直す。
	 *
	 * @note
	 * このI/Fはスレッドセーフではありません。
	 */
	void set_idx_size(
		const int idx_size_arg   //!< [in] 用意するインデックス番号の数。0以下の場合、現在のバッファを解放の上、バッファの再確保を保留する。
	);

	/*!
	 * @brief	clear
	 */
	void clear( void );

	/*!
	 * @brief	利用可能なインデックス番号を取り出す。
	 *
	 * @return	利用可能なインデックス番号
	 * @retval	-1	利用可能なインデックス番号がない
	 *
	 * @note
	 * このI/Fはスレッドセーフで、ロックフリーです。
	 */
	int pop( void );

	/*!
	 * @brief	使用が完了したインデックス番号を返却する
	 *
	 * @note
	 * このI/Fはスレッドセーフで、ロックフリーです。
	 */
	void push(
		const int idx_arg   //!< [in] 返却するインデックス番号
	);

	/*!
	 * @brief	dump for debug
	 */
	void dump( void );

	int get_collision_cnt_invalid_storage( void ) const
	{
		return invalid_element_storage_.get_collision_cnt();
	}
	int get_collision_cnt_valid_storage( void ) const
	{
		return valid_element_storage_.get_collision_cnt();
	}

private:
	struct rcv_idx_by_thread_terminating {
		rcv_idx_by_thread_terminating( idx_mgr* p_elst_arg )
		  : p_elst_( p_elst_arg )
		{
		}

		uintptr_t allocate( void )
		{
			return reinterpret_cast<uintptr_t>( new waiting_idx_list( p_elst_->idx_size_.load( std::memory_order_acquire ), p_elst_->idx_size_ver_.load( std::memory_order_acquire ) ) );
		}

		void deallocate( uintptr_t p_destructing_tls )
		{
			waiting_idx_list* p_tmp = reinterpret_cast<waiting_idx_list*>( p_destructing_tls );
			p_elst_->rcv_wait_idx_by_thread_terminating( p_tmp );
			delete p_tmp;
		}

		idx_mgr* p_elst_;
	};

	/*!
	 * @brief	tls_waiting_idx_list_のスレッドローカルストレージが破棄される際(=スレッドが終了する場合)に滞留中の要素を受け取る
	 *
	 * @note
	 * スレッドセーフではあるが、ロックによる排他制御が行われる。
	 */
	void rcv_wait_idx_by_thread_terminating( waiting_idx_list* p_idx_list );

	std::atomic<int> idx_size_;                                                              //!< 割り当てられたインデックス番号の数
	std::atomic<int> idx_size_ver_;                                                          //!< 割り当てられたインデックス番号の数の情報のバージョン番号

	std::mutex       mtx_rcv_waiting_idx_list_;                                              //!< スレッドローカルストレージに滞留しているインデックスで、スレッド終了時に受け取るバッファ(rcv_waiting_idx_list_)の排他制御を行う
	waiting_idx_list rcv_waiting_idx_list_;                                                  //!< スレッドローカルストレージに滞留しているインデックスで、スレッド終了時に受け取るバッファ

	std::atomic<unsigned int>* p_alloc_collision_cnt_;                                       //!< idxをpopする時に発生した衝突回数をカウントする変数へのポインタ
	std::atomic<unsigned int>* p_dealloc_collision_cnt_;                                     //!< idxをpushする時に発生した衝突回数をカウントする変数へのポインタ

	idx_mgr_element*                                             p_idx_mgr_element_array_;   //!< インデックス番号を管理情報を保持する配列
	idx_element_storage_mgr                                      invalid_element_storage_;   //!< インデックス番号を所持しない要素を管理するストレージ
	idx_element_storage_mgr                                      valid_element_storage_;     //!< インデックス番号を所持する要素を管理するストレージ
	dynamic_tls<waiting_idx_list, rcv_idx_by_thread_terminating> tls_waiting_idx_list_;      //!< 滞留インデックスを管理するスレッドローカルリスト。dynamic_tlsで指定しているrcv_idx_by_thread_terminatingによって、デストラクタ実行時にrcv_waiting_idx_list_に残留データをpushする。そのため、メンバ変数rcv_waiting_idx_list_よりも後に変数宣言を行っている。
};

struct slot_chk_result;

/*!
 * @brief	management information of a chunk
 */
class chunk_header_multi_slot {
public:
	std::atomic<chunk_header_multi_slot*> p_next_chunk_;      //!< pointer to next chunk header. chunk header does not free. therefore we don't need to consider ABA.
	std::atomic<chunk_control_status>     status_;            //!< chunk status for GC
	std::atomic<unsigned int>             owner_tl_id_;       //!< owner tl_id_
	mutable std::atomic<int>              num_of_accesser_;   //!< number of accesser to slot buffer

	/*!
	 * @brief	constructor
	 */
	chunk_header_multi_slot(
		const param_chunk_allocation& ch_param_arg,      //!< [in] chunk allocation paramter
		const unsigned int            owner_tl_id_arg,   //!< [in] owner tl_id_
		chunk_list_statistics*        p_chms_stat_arg    //!< [in] pointer to statistics inforation to store activity
	);

	/*!
	 * @brief	destructor
	 */
	~chunk_header_multi_slot();

	/*!
	 * @brief	allocate new memory slot
	 *
	 * @pre status_ must be chunk_control_status::NORMAL
	 *
	 * @return	pointer to an allocated memory slot
	 * @retval	non-nullptr	success to allocate and it is a pointer to an allocated memory
	 * @retval	nullptr		fail to allocate
	 */
	inline void* allocate_mem_slot( void )
	{
		void* p_ans = allocate_mem_slot_impl();
		if ( p_ans != nullptr ) {
			p_statistics_->free_slot_cnt_--;
			auto cur     = p_statistics_->consum_cnt_.fetch_add( 1, std::memory_order_acq_rel ) + 1;
			auto cur_max = p_statistics_->max_consum_cnt_.load( std::memory_order_acquire );
			if ( cur > cur_max ) {
				p_statistics_->max_consum_cnt_.compare_exchange_strong( cur_max, cur );
			}
		}
		return p_ans;
	}

	/*!
	 * @brief	recycle memory slot
	 *
	 * @retval	true	success to recycle.
	 * @retval	false	fail to recycle. Normally p_recycle_slot does not belong to this chunk
	 */
	inline bool recycle_mem_slot(
		void* p_recycle_slot   //!< [in] pointer to the memory slot to recycle.
	)
	{
		bool ans = recycle_mem_slot_impl( p_recycle_slot );
		if ( ans ) {
			p_statistics_->free_slot_cnt_++;
			p_statistics_->consum_cnt_--;
		}
		return ans;
	}

	/*!
	 * @brief	allocate new chunk
	 *
	 * @pre status_ must be chunk_control_status::EMPTY
	 *
	 * @retval true success to allocate a chunk memory
	 * @retval false fail to allocate a chunk memory
	 *
	 * @post owner_tl_id_ is owner_tl_id_arg, and status_ is chunk_control_status::NORMAL
	 */
	bool alloc_new_chunk(
		const param_chunk_allocation& ch_param_arg,     //!< [in] chunk allocation paramter
		const unsigned int            owner_tl_id_arg   //!< [in] owner tl_id_
	);

	/*!
	 * @brief	try to get ownership of this chunk
	 *
	 * @pre owner_tl_id_ is owner_tl_id_arg, and status_ is chunk_control_status::NORMAL or chunk_control_status::RESERVED_DELETION
	 *
	 * @retval non-nullptr success to get ownership and allocate a memory slot
	 * @retval false fail to get ownership or fail to allocate a memory slot
	 *
	 * @post status_ is chunk_control_status::NORMAL
	 */
	inline void* try_allocate_mem_slot_from_reserved_deletion(
		const unsigned int owner_tl_id_arg   //!< [in] owner tl_id_
	)
	{
		unsigned int cur_tl_id = owner_tl_id_.load( std::memory_order_acquire );
		return try_allocate_mem_slot_impl( cur_tl_id, owner_tl_id_arg );
	}

	/*!
	 * @brief	try to get ownership of this chunk
	 *
	 * @pre owner_tl_id_ is NON_OWNERED_TL_ID, and status_ is chunk_control_status::NORMAL or chunk_control_status::RESERVED_DELETION
	 *
	 * @retval non-nullptr success to get ownership and allocate a memory slot
	 * @retval false fail to get ownership or fail to allocate a memory slot
	 *
	 * @post owner_tl_id_ is owner_tl_id_arg, and status_ is chunk_control_status::NORMAL
	 */
	inline void* try_get_ownership_allocate_mem_slot(
		const unsigned int owner_tl_id_arg   //!< [in] owner tl_id_
	)
	{
		return try_allocate_mem_slot_impl( NON_OWNERED_TL_ID, owner_tl_id_arg );
	}

	bool set_delete_reservation( void );
	bool unset_delete_reservation( void );
	bool exec_deletion( void );

	const param_chunk_allocation& get_param_chunk_allocation( void ) const
	{
		return slot_conf_;
	}

	/*!
	 * @brief	get chunk_header_multi_slot address from void* address that allocate_mem_slot() returns.
	 *
	 * @return	check result and pointer to chunk
	 */
	static slot_chk_result get_chunk(
		void* p_addr   //!< [in] void* address that allocate_mem_slot() returns
	);

	/*!
	 * @brief	get statistics
	 */
	chunk_statistics get_statistics( void ) const;

	/*!
	 * @brief	dump for debug
	 */
	void dump( void );

private:
	enum class slot_status_mark : int {
		FREE,      //!< slot is free status(not used)
		INUSE,     //!< slot is used
		DISCARED   //!< slot is allocated, then free already. This is used for debuging double free bug
	};

	static std::size_t get_size_of_one_slot(
		const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
	);

	void set_slot_allocation_conf(
		const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
	);

	/*!
	 * @brief	allocate new memory slot
	 *
	 * @return	pointer to an allocated memory slot
	 * @retval	non-nullptr	success to allocate and it is a pointer to an allocated memory
	 * @retval	nullptr		fail to allocate
	 */
	void* allocate_mem_slot_impl( void );

	/*!
	 * @brief	recycle memory slot
	 *
	 * @retval	true	success to recycle.
	 * @retval	false	fail to recycle. Normally p_recycle_slot does not belong to this chunk
	 */
	bool recycle_mem_slot_impl(
		void* p_recycle_slot   //!< [in] pointer to the memory slot to recycle.
	);

	/*!
	 * @brief	try to get ownership of this chunk
	 *
	 * @pre owner_tl_id_ is expect_tl_id_arg, and status_ is chunk_control_status::NORMAL or chunk_control_status::RESERVED_DELETION
	 *
	 * @retval non-nullptr success to get ownership and allocate a memory slot
	 * @retval false fail to get ownership or fail to allocate a memory slot
	 *
	 * @post owner_tl_id_ is owner_tl_id_arg, and status_ is chunk_control_status::NORMAL
	 */
	void* try_allocate_mem_slot_impl(
		const unsigned int expect_tl_id_arg,   //!< [in] owner tl_id_
		const unsigned int owner_tl_id_arg     //!< [in] owner tl_id_
	);

	chunk_list_statistics* p_statistics_;               //!< statistics

	param_chunk_allocation slot_conf_;                  //!< allocation configuration paramter. value is corrected internally.
	std::size_t            size_of_chunk_;              //!< allocated memory size of this chunk

	idx_mgr free_slot_idx_mgr_;                         //<! manager of free slot index

	std::atomic<slot_status_mark>* p_free_slot_mark_;   //!< if slot is free, this is marked as true. This is prior information than free slot idx stack.
	void*                          p_chunk_;            //!< pointer to an allocated memory as a chunk
	size_t                         allocated_size_;
};

struct slot_chk_result {
	bool                     correct_;   //!< slot header check result. true: check result is valid.
	chunk_header_multi_slot* p_chms_;    //!< pointer to chunk_header_multi_slot. if correct_ is true and this pointer value is nullptr, it is allocated by malloc with slot_header
};

struct slot_header {
	std::atomic<chunk_header_multi_slot*> at_p_chms_;   //!< pointer to chunk_header_multi_slot that is an owner of this slot
	std::atomic<std::uintptr_t>           at_mark_;     //!< checker mark
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	bt_info alloc_bt_info_;                             //!< backtrace information when is allocated
	bt_info free_bt_info_;                              //!< backtrace information when is free
#endif

	void set_addr_of_chunk_header_multi_slot(
		chunk_header_multi_slot* p_chms_arg   //!< [in] pointer to a parent "chunk_header_multi_slot"
	);

	slot_chk_result chk_header_data( void ) const;
};

/*!
 * @brief	chunk list that has same allocation parameter
 */
class chunk_list {
public:
	/*!
	 * @brief	constructor
	 */
	constexpr chunk_list(
		const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
		)
	  : chunk_param_( ch_param_arg )
	  , p_top_chunk_()
	  , tls_hint_( tl_chunk_param_destructor( this ) )
	  , statistics_()
	{
		return;
	}

	/*!
	 * @brief	destructor
	 */
	~chunk_list() = default;

	/*!
	 * @brief	allocate new memory slot
	 *
	 * @return	pointer to an allocated memory slot
	 * @retval	non-nullptr	success to allocate and it is a pointer to an allocated memory
	 * @retval	nullptr		fail to allocate
	 */
	void* allocate_mem_slot( void );

	/*!
	 * @brief	recycle memory slot
	 *
	 * @retval	true	success to recycle.
	 * @retval	false	fail to recycle. Normally p_recycle_slot does not belong to this chunk
	 */
	bool recycle_mem_slot(
		void* p_recycle_slot   //!< [in] pointer to the memory slot to recycle.
	);

	/*!
	 * @brief	free the deletable buffers
	 */
	void prune( void );

	/*!
	 * @brief	get statistics
	 */
	chunk_statistics get_statistics( void ) const;

	const param_chunk_allocation chunk_param_;

private:
	/**
	 * @brief スレッド毎のチャンク操作のためのヒント情報、および所有者となるchunk_listへの情報を保持する構造体
	 */
	struct tl_chunk_param {
		chunk_list* const        p_owner_chunk_list_;   //!< 所有者となるchunk_listへのポインタ
		const unsigned int       tl_id_;                //!< スレッド毎に構造体を区別するためおID
		unsigned int             num_of_pieces_;        //!< 現在のメモリ領域割り当てに使用したスロット数
		chunk_header_multi_slot* tls_p_hint_chunk;      //!< 所有者のスレッドが最初に参照するべきchunk_header_multi_slotへのポインタ。

		tl_chunk_param(
			chunk_list*  p_owner_chunk_list_arg,   //!< [in] pointer to onwer chunk_list
			unsigned int init_num_of_pieces_arg    //!< [in] initiall number of slots for allocation
		);

	private:
		static unsigned int              get_new_tl_id( void );
		static std::atomic<unsigned int> tl_id_counter_;   //!< 各スレッド毎に生成されるIDをIDジェネレータ
	};
	/**
	 * @brief それぞれのスレッド終了時に実行する処理を担うfunctor
	 */
	struct tl_chunk_param_destructor {
		constexpr explicit tl_chunk_param_destructor( chunk_list* p_chlst_arg )
		  : p_chlst_( p_chlst_arg )
		{
		}

		uintptr_t allocate( void )
		{
			return reinterpret_cast<uintptr_t>( new tl_chunk_param( p_chlst_, p_chlst_->chunk_param_.num_of_pieces_ ) );
		}

		/*!
		 * @brief スレッド終了時等に呼び出されるI/F
		 */
		void deallocate( uintptr_t p_destructing_tls )
		{
			tl_chunk_param* p_tmp = reinterpret_cast<tl_chunk_param*>( p_destructing_tls );

			p_tmp->p_owner_chunk_list_->release_all_of_ownership( p_tmp->tl_id_, nullptr );

			delete p_tmp;
		}

		chunk_list* p_chlst_;
	};

	// TODO 仮実装
	class atomic_push_list {
	public:
		class forward_iterator {
		public:
			constexpr forward_iterator( chunk_header_multi_slot* p_arg )
			  : p_( p_arg )
			{
			}

			chunk_header_multi_slot& operator*( void )
			{
				return *p_;
			}
			chunk_header_multi_slot& operator->( void )
			{
				return *p_;
			}
			forward_iterator& operator++( void )
			{
				p_ = p_->p_next_chunk_.load( std::memory_order_acquire );
				return *this;
			}
			bool operator!=( const forward_iterator& v )
			{
				return p_ != v.p_;
			}

		private:
			chunk_header_multi_slot* p_;
		};

		constexpr atomic_push_list( void )
		  : p_top_( nullptr )
		{
		}
		~atomic_push_list()
		{
			chunk_header_multi_slot* p_chms = p_top_.load( std::memory_order_acquire );
			while ( p_chms != nullptr ) {
				chunk_header_multi_slot* p_next_chms = p_chms->p_next_chunk_.load( std::memory_order_acquire );
				delete p_chms;
				p_chms = p_next_chms;
			}
		}

		chunk_header_multi_slot* load( std::memory_order order )
		{
			return p_top_.load( order );
		}

		void push( chunk_header_multi_slot* p_new_chms )
		{
			chunk_header_multi_slot* p_cur_top = p_top_.load( std::memory_order_acquire );
			do {
				p_new_chms->p_next_chunk_.store( p_cur_top, std::memory_order_release );
			} while ( !p_top_.compare_exchange_weak( p_cur_top, p_new_chms ) );
		}

		inline forward_iterator begin( void )
		{
			return forward_iterator( p_top_.load( std::memory_order_acquire ) );
		}
		inline forward_iterator end( void )
		{
			return forward_iterator( nullptr );
		}

	private:
		std::atomic<chunk_header_multi_slot*> p_top_;
	};

	void mark_as_reserved_deletion(
		unsigned int             target_tl_id_arg,    //!< [in] 削除予約対象に変更するtl_id_
		chunk_header_multi_slot* p_non_deletion_arg   //!< [in] 削除予約対象としないchunk_header_multi_slotへのポインタ
	);

	void release_all_of_ownership(
		unsigned int                   target_tl_id_arg,         //!< [in] オーナー権を開放する対象のtl_id_
		const chunk_header_multi_slot* p_non_release_chunk_arg   //!< [in] オーナー権解放の対象外とするchunk_header_multi_slotへのポインタ。指定しない場合は、nullptrを指定すること。
	);

	unsigned int get_cur_max_slot_size(
		unsigned int target_tl_id_arg   //!< [in] オーナー権を開放する対象のtl_id_
	);

	atomic_push_list                                       p_top_chunk_;   //!< pointer to chunk_header that is top of list.
	dynamic_tls<tl_chunk_param, tl_chunk_param_destructor> tls_hint_;      //!< thread local pointer to chunk_header that is success to allocate recently for a thread.
	                                                                       //!< tls_hint_は、p_top_chunk_を参照しているため、この2つのメンバ変数の宣言順(p_top_chunk_の次にtls_hint_)を入れ替えてはならない。
	                                                                       //!< 入れ替えてしまった場合、デストラクタでの解放処理で、解放済みのメモリ領域にアクセスしてしまう。

	chunk_list_statistics statistics_;                                     //!< statistics
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif /* INC_ALCONCURRENT_LF_MEM_ALLOC_IDX_MGR_HPP_ */
