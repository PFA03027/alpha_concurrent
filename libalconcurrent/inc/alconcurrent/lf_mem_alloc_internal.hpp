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

#include "conf_logger.hpp"
#include "dynamic_tls.hpp"
#include "hazard_ptr.hpp"

#include "lf_mem_alloc_type.hpp"

//#define ALCONCURRENT_CONF_SELECT_SHARED_CHUNK_LIST

namespace alpha {
namespace concurrent {

namespace internal {

/*!
 * @brief	chunk control status
 */
enum class chunk_control_status {
	EMPTY,                 //!< chunk header has no allocated chunk memory.
	RESERVED_ALLOCATION,   //!< chunk header has no allocated chunk memory. But some one start to allocation
	NORMAL,                //!< allow to allocate the memory from this chunk
	RESERVED_DELETION,     //!< does not allow to allocate the memory from this chunk. But if needed to reuse this chunk, allow to change NORMAL
	DELETION,              //!< does not allow to access any more except GC. After shift to this state, chunk memory will be free after confirmed accesser is zero.
};

/*!
 * @brief statistics information of alloc/dealloc
 *
 */
struct chms_statistics {
	chms_statistics( void )
	  : chunk_num_( 0 )
	  , total_slot_cnt_( 0 )
	  , free_slot_cnt_( 0 )
	  , consum_cnt_( 0 )
	  , max_consum_cnt_( 0 )
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	  , alloc_req_cnt_( 0 )
	  , alloc_req_err_cnt_( 0 )
	  , dealloc_req_cnt_( 0 )
	  , dealloc_req_err_cnt_( 0 )
	  , alloc_collision_cnt_( 0 )
	  , dealloc_collision_cnt_( 0 )
#endif
	{
	}
	chms_statistics( const chms_statistics& ) = default;
	chms_statistics( chms_statistics&& )      = default;
	chms_statistics& operator=( const chms_statistics& ) = default;
	chms_statistics& operator=( chms_statistics&& ) = default;

	chunk_statistics get_statistics( void ) const;

	std::atomic<unsigned int> chunk_num_;        //!< number of chunks
	std::atomic<unsigned int> total_slot_cnt_;   //!< total slot count
	std::atomic<unsigned int> free_slot_cnt_;    //!< free slot count
	std::atomic<unsigned int> consum_cnt_;       //!< current count of allocated slots
	std::atomic<unsigned int> max_consum_cnt_;   //!< max count of allocated slots
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	std::atomic<unsigned int> alloc_req_cnt_;           //!< allocation request count
	std::atomic<unsigned int> alloc_req_err_cnt_;       //!< fail count for allocation request
	std::atomic<unsigned int> dealloc_req_cnt_;         //!< deallocation request count
	std::atomic<unsigned int> dealloc_req_err_cnt_;     //!< fail count for deallocation request
	std::atomic<unsigned int> alloc_collision_cnt_;     //!< count of a collision of allocation in lock-free algorithm
	std::atomic<unsigned int> dealloc_collision_cnt_;   //!< count of a collision of deallocation in lock-free algorithm
#endif
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
		std::atomic<idx_mgr_element*> idx_mgr_element::*p_next_ptr_offset_arg   //!< [in] nextポインタを保持しているメンバ変数へのメンバ変数ポインタ
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
		,
		std::atomic<unsigned int>* p_collition_counter   //!< [in] 衝突が発生した回数を記録するアトミック変数へのポインタ
#endif
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

#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	/*!
	 * @brief	リスト操作時の衝突回数を取得する。
	 */
	int get_collision_cnt( void ) const
	{
		return p_collision_cnt_->load();
	}
#endif

private:
	static constexpr int hzrd_max_slot_ = 3;
	enum class hazard_ptr_idx : int {
		PUSH_CUR = 0,
		POP_CUR  = 1,
		POP_NEXT = 2,
	};
	using scoped_hazard_ref = hazard_ptr_scoped_ref<idx_mgr_element, hzrd_max_slot_>;

	struct rcv_el_by_thread_terminating {
		rcv_el_by_thread_terminating( idx_element_storage_mgr* p_elst_arg )
		  : p_elst_( p_elst_arg )
		{
		}

		bool release( waiting_element_list& destructing_tls )
		{
			p_elst_->rcv_wait_element_by_thread_terminating( &destructing_tls );
			return true;
		}

		void destruct( waiting_element_list& destructing_tls )
		{
			return;
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

	dynamic_tls<waiting_element_list, rcv_el_by_thread_terminating> tls_waiting_list_;   //!< 滞留要素を管理するスレッドローカルリスト
	hazard_ptr<idx_mgr_element, hzrd_max_slot_>                     hzrd_element_;       //!< ハザードポインタを管理する構造体。

	std::atomic<idx_mgr_element*> head_;                                  //!< リスト構造のヘッドへのポインタ
	std::atomic<idx_mgr_element*> tail_;                                  //!< リスト構造の終端要素へのポインタ
	std::atomic<idx_mgr_element*> idx_mgr_element::*p_next_ptr_offset_;   //!< リスト構造の次のノードへのポインタを格納したメンバ変数へのメンバ変数ポインタ

	std::mutex           mtx_rcv_wait_element_list_;   //!< tls_waiting_list_のスレッドローカルストレージが破棄される際に滞留中の要素を受け取るメンバ変数rcv_wait_element_list_の排他制御用Mutex
	waiting_element_list rcv_wait_element_list_;       //!< tls_waiting_list_のスレッドローカルストレージが破棄される際に滞留中の要素を受け取る

#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	std::atomic<unsigned int>* p_collision_cnt_;   //!< ロックフリーアルゴリズム内で発生した衝突回数を記録する変数への参照
#endif
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
		const int idx_size_arg   //!< [in] 用意するインデックス番号の数。-1の場合、割り当てを保留する。
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
		,
		std::atomic<unsigned int>* p_alloc_collision_cnt_arg,    //!< [in] 衝突回数をカウントする変数へのポインタ。ポインタ先のインスタンスは、このインスタンス以上のライフタイムを持つこと
		std::atomic<unsigned int>* p_dealloc_collision_cnt_arg   //!< [in] 衝突回数をカウントする変数へのポインタ。ポインタ先のインスタンスは、このインスタンス以上のライフタイムを持つこと
#endif
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
	void set_idx_size( const int idx_size_arg );

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

#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	int get_collision_cnt_invalid_storage( void ) const
	{
		return invalid_element_storage_.get_collision_cnt();
	}
	int get_collision_cnt_valid_storage( void ) const
	{
		return valid_element_storage_.get_collision_cnt();
	}
#endif

private:
	struct rcv_idx_by_thread_terminating {
		rcv_idx_by_thread_terminating( idx_mgr* p_elst_arg )
		  : p_elst_( p_elst_arg )
		{
		}

		bool release( waiting_idx_list& destructing_tls )
		{
			p_elst_->rcv_wait_idx_by_thread_terminating( &destructing_tls );
			return true;
		}

		void destruct( waiting_idx_list& destructing_tls )
		{
			return;
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

	int idx_size_;       //!< 割り当てられたインデックス番号の数
	int idx_size_ver_;   //!< 割り当てられたインデックス番号の数の情報のバージョン番号

#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	std::atomic<unsigned int>* p_alloc_collision_cnt_;     //!< idxをpopする時に発生した衝突回数をカウントする変数へのポインタ
	std::atomic<unsigned int>* p_dealloc_collision_cnt_;   //!< idxをpushする時に発生した衝突回数をカウントする変数へのポインタ
#endif
	idx_mgr_element*                                             p_idx_mgr_element_array_;   //!< インデックス番号を管理情報を保持する配列
	idx_element_storage_mgr                                      invalid_element_storage_;   //!< インデックス番号を所持しない要素を管理するストレージ
	idx_element_storage_mgr                                      valid_element_storage_;     //!< インデックス番号を所持する要素を管理するストレージ
	dynamic_tls<waiting_idx_list, rcv_idx_by_thread_terminating> tls_waiting_idx_list_;      //!< 滞留インデックスを管理するスレッドローカルリスト

	std::mutex       mtx_rcv_waiting_idx_list_;   //!< スレッドローカルストレージに滞留しているインデックスで、スレッド終了時に受け取るバッファ(rcv_waiting_idx_list_)の排他制御を行う
	waiting_idx_list rcv_waiting_idx_list_;
};

struct slot_chk_result;

/*!
 * @brief	management information of a chunk
 */
class chunk_header_multi_slot {
public:
	std::atomic<chunk_header_multi_slot*>       p_next_chunk_;      //!< pointer to next chunk header. chunk header does not free. therefore we don't need to consider ABA.
	std::atomic<internal::chunk_control_status> status_;            //!< chunk status for GC
	std::atomic<int>                            num_of_accesser_;   //!< number of accesser

	/*!
	 * @brief	constructor
	 */
	chunk_header_multi_slot(
		const param_chunk_allocation& ch_param_arg,               //!< [in] chunk allocation paramter
		chms_statistics*              p_chms_stat_arg = nullptr   //!< [in] pointer to statistics inforation to store activity
	);

	/*!
	 * @brief	destructor
	 */
	~chunk_header_multi_slot();

	/*!
	 * @brief	allocate new memory slot
	 *
	 * @return	pointer to an allocated memory slot
	 * @retval	non-nullptr	success to allocate and it is a pointer to an allocated memory
	 * @retval	nullptr		fail to allocate
	 */
	inline void* allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
		const char* caller_src_fname,   //!< [in] caller side source file name
		const int   caller_lineno,      //!< [in] caller side line number
		const char* caller_func_name    //!< [in] function name calling this I/F
#else
		void
#endif
	)
	{
		void* p_ans = allocate_mem_slot_impl(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
			caller_src_fname, caller_lineno, caller_func_name
#endif
		);
		if ( p_ans != nullptr ) {
			statistics_.free_slot_cnt_--;
			auto cur     = statistics_.consum_cnt_.fetch_add( 1 ) + 1;
			auto cur_max = statistics_.max_consum_cnt_.load( std::memory_order_acquire );
			if ( cur > cur_max ) {
				statistics_.max_consum_cnt_.compare_exchange_strong( cur_max, cur );
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
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
		,
		const char* caller_src_fname,   //!< [in] caller side source file name
		const int   caller_lineno,      //!< [in] caller side line number
		const char* caller_func_name    //!< [in] function name calling this I/F
#endif
	)
	{
		bool ans = recycle_mem_slot_impl(
			p_recycle_slot
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
			,
			caller_src_fname, caller_lineno, caller_func_name
#endif
		);
		if ( ans ) {
			statistics_.free_slot_cnt_++;
			statistics_.consum_cnt_--;
		}
		return ans;
	}

	bool set_delete_reservation( void );
	bool unset_delete_reservation( void );
	bool exec_deletion( void );
	bool exec_allocation( void );

	/*!
	 * @brief	allocate new chunk
	 */
	bool alloc_new_chunk(
		const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
	);

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
	void* allocate_mem_slot_impl(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
		const char* caller_src_fname,   //!< [in] caller side source file name
		const int   caller_lineno,      //!< [in] caller side line number
		const char* caller_func_name    //!< [in] function name calling this I/F
#else
		void
#endif
	);

	/*!
	 * @brief	recycle memory slot
	 *
	 * @retval	true	success to recycle.
	 * @retval	false	fail to recycle. Normally p_recycle_slot does not belong to this chunk
	 */
	bool recycle_mem_slot_impl(
		void* p_recycle_slot   //!< [in] pointer to the memory slot to recycle.
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
		,
		const char* caller_src_fname,   //!< [in] caller side source file name
		const int   caller_lineno,      //!< [in] caller side line number
		const char* caller_func_name    //!< [in] function name calling this I/F
#endif
	);

	chms_statistics  statistics_imp_;   //!< statistics
	chms_statistics& statistics_;       //!< reference to store statistics information

	param_chunk_allocation slot_conf_;       //!< allocation configuration paramter. value is corrected internally.
	std::size_t            size_of_chunk_;   //!< size of a chunk

	idx_mgr free_slot_idx_mgr_;   //<! manager of free slot index

	std::atomic_bool* p_free_slot_mark_;   //!< if slot is free, this is marked as true. This is prior information than free slot idx stack.
	void*             p_chunk_;            //!< pointer to an allocated memory as a chunk
};

struct slot_chk_result {
	bool                     correct_;   //!< slot header result
	chunk_header_multi_slot* p_chms_;    //!< pointer to chunk_header_multi_slot. if correct is true and this pointer value is nullptr, it is allocated by malloc with slot_header
};

struct slot_header {
	std::atomic<chunk_header_multi_slot*> at_p_chms_;   //!< pointer to chunk_header_multi_slot that is an owner of this slot
	std::atomic<std::uintptr_t>           at_mark_;     //!< checker mark
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
	const char* p_caller_src_fname_;   //!< caller side source file name
	int         caller_lineno_;        //!< caller side line number
	const char* p_caller_func_name_;   //!< function name calling this I/F
#endif

	void set_addr_of_chunk_header_multi_slot(
		chunk_header_multi_slot* p_chms_arg   //!< [in] pointer to a parent "chunk_header_multi_slot"
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
		,
		const char* caller_src_fname,   //!< [in] caller side source file name
		const int   caller_lineno,      //!< [in] caller side line number
		const char* caller_func_name    //!< [in] function name calling this I/F
#endif
	);

#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
	void set_caller_context_info(
		const char* caller_src_fname,   //!< [in] caller side source file name
		const int   caller_lineno,      //!< [in] caller side line number
		const char* caller_func_name    //!< [in] function name calling this I/F
	);
#endif

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
	chunk_list(
		const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
	);

	/*!
	 * @brief	destructor
	 */
	~chunk_list();

	/*!
	 * @brief	allocate new memory slot
	 *
	 * @return	pointer to an allocated memory slot
	 * @retval	non-nullptr	success to allocate and it is a pointer to an allocated memory
	 * @retval	nullptr		fail to allocate
	 */
	void* allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
		const char* caller_src_fname,   //!< [in] caller side source file name
		const int   caller_lineno,      //!< [in] caller side line number
		const char* caller_func_name    //!< [in] function name calling this I/F
#else
		void
#endif
	);

	/*!
	 * @brief	recycle memory slot
	 *
	 * @retval	true	success to recycle.
	 * @retval	false	fail to recycle. Normally p_recycle_slot does not belong to this chunk
	 */
	bool recycle_mem_slot(
		void* p_recycle_slot   //!< [in] pointer to the memory slot to recycle.
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
		,
		const char* caller_src_fname,   //!< [in] caller side source file name
		const int   caller_lineno,      //!< [in] caller side line number
		const char* caller_func_name    //!< [in] function name calling this I/F
#endif
	);

	/*!
	 * @brief	get statistics
	 */
	chunk_statistics get_statistics( void ) const;

private:
#ifdef ALCONCURRENT_CONF_SELECT_SHARED_CHUNK_LIST
#else
	struct threadlocal_chunk_header_multi_slot_list_free {
		bool release(
			chunk_header_multi_slot*& data   //!< [in/out] ファンクタの処理対象となるインスタンスへの参照
		)
		{
			// 他のスレッドが割り当てたメモリを使用中であるため、スレッド終了時には領域を解放しない。
			return false;
		}
		void destruct(
			chunk_header_multi_slot*& data   //!< [in/out] ファンクタの処理対象となるインスタンスへの参照
		)
		{
			// インスタンスが削除されるため、リンクリストを削除する。
			chunk_header_multi_slot* p_chms = data;
			while ( p_chms != nullptr ) {
				chunk_header_multi_slot* p_next_chms = p_chms->p_next_chunk_.load();
				delete p_chms;
				p_chms = p_next_chms;
			}
			data = nullptr;
			return;
		}
	};
#endif

	unsigned int              size_of_one_piece_;   //!< size of one piece in a chunk
	std::atomic<unsigned int> num_of_pieces_;       //!< number of pieces in a chunk

#ifdef ALCONCURRENT_CONF_SELECT_SHARED_CHUNK_LIST
	std::atomic<chunk_header_multi_slot*> p_top_chunk_;   //!< pointer to chunk_header that is top of list.
#else
	dynamic_tls<chunk_header_multi_slot*, threadlocal_chunk_header_multi_slot_list_free> tls_p_top_chunk_;   //!< thread loacal pointer to chunk_header that is top of list.
#endif
	dynamic_tls<chunk_header_multi_slot*> tls_p_hint_chunk;   //!< thread loacal pointer to chunk_header that is success to allocate recently for a thread.

	chms_statistics statistics_;   //!< statistics
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif /* INC_ALCONCURRENT_LF_MEM_ALLOC_IDX_MGR_HPP_ */
