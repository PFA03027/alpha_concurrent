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

namespace alpha {
namespace concurrent {

namespace internal {

/*!
 * @breif	chunk control status
 */
enum class chunk_control_status {
	EMPTY,                 //!< chunk header has no allocated chunk memory.
	RESERVED_ALLOCATION,   //!< chunk header has no allocated chunk memory. But some one start to allocation
	NORMAL,                //!< allow to allocate the memory from this chunk
	RESERVED_DELETION,     //!< does not allow to allocate the memory from this chunk. But if needed to reuse this chunk, allow to change NORMAL
	DELETION,              //!< does not allow to access any more except GC. After shift to this state, chunk memory will be free after confirmed accesser is zero.
};

/*!
 * @breif	slot status
 */
enum class slot_status {
	INVALID,         //!< invalid queue slot
	SLOT_RESERVED,   //!< this queue slot will use soon
	VALID_IDX,       //!< free idx is valid in this slot
	SOLD_OUT,        //!< index in this queue slot is sold out
};




/*!
 * @breif	使用可能なインデックス番号を管理する配列で使用する各要素のデータ構造
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
 * @breif	ハーザードポインタ登録中で滞留している要素を管理するリスト
 *
 * スレッドローカルデータとして管理されるため、排他制御は不要なクラス
 *
 * @note
 * スレッドローカルストレージは、情報の伝搬が難しいため、バッファの再構築の要否を判定するために、バッファ構築のバージョン情報を使用使用する方式を採る。
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
 * @breif	ハーザードポインタ登録中で滞留している要素を管理するリスト
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
 * @breif	インデックス管理スロットのロックフリーストレージクラス
 */
class idx_element_storage_mgr {
public:
	/*!
	 * @breif	コンストラクタ
	 */
	idx_element_storage_mgr(
		std::atomic<idx_mgr_element*> idx_mgr_element::*p_next_ptr_offset_arg   //!< [in] nextポインタを保持しているメンバ変数へのメンバ変数ポインタ
	);

	/*!
	 * @breif	ストレージから１つの要素を取り出す
	 *
	 * @return	pointer to the poped element
	 * @retval	nullptr	no available element
	 */
	idx_mgr_element* pop_element( void );

	/*!
	 * @breif	ストレージへ１つの要素を戻す
	 */
	void push_element(
		idx_mgr_element* p_push_element   //!< [in] pointer of element to push
	);

	int get_collision_cnt( void ) const
	{
		return collision_cnt_.load();
	}

private:
	static constexpr int hzrd_max_slot_ = 3;
	enum class hazard_ptr_idx : int {
		PUSH_CUR = 0,
		POP_CUR  = 1,
		POP_NEXT = 2,
	};
	using scoped_hazard_ref = hazard_ptr_scoped_ref<idx_mgr_element, hzrd_max_slot_>;

	int                                         idx_size_;
	dynamic_tls<waiting_element_list>           tls_waiting_list_;   //!< 滞留要素を管理するスレッドローカルリスト
	hazard_ptr<idx_mgr_element, hzrd_max_slot_> hzrd_element_;       //!< ハザードポインタを管理する構造体。

	std::atomic<idx_mgr_element*> head_;                                  //!< リスト構造のヘッドへのポインタ
	std::atomic<idx_mgr_element*> tail_;                                  //!< リスト構造の終端要素へのポインタ
	std::atomic<idx_mgr_element*> idx_mgr_element::*p_next_ptr_offset_;   //!< リスト構造の次のノードへのポインタを格納したメンバ変数へのメンバ変数ポインタ

	// statistics
	std::atomic<int> collision_cnt_;   //!< ロックフリーアルゴリズム内で発生した衝突回数

	/*!
	 * @breif	ストレージから１つの要素を取り出す
	 *
	 * @return	pointer to the poped element
	 * @retval	nullptr	no available element
	 */
	idx_mgr_element* pop_element_from_list( void );

	/*!
	 * @breif	ストレージへ１つの要素を戻す
	 */
	void push_element_to_list(
		idx_mgr_element* p_push_element   //!< [in] pointer of element to push
	);
};

/*!
 * @breif	使用可能なインデックス番号を管理する準ロックフリークラス
 */
struct idx_mgr {
	/*!
	 * @breif	コンストラクタ
	 */
	idx_mgr(
		const int idx_size_arg = -1   //!< [in] 用意するインデックス番号の数。-1の場合、割り当てを保留する。
	);

	/*!
	 * @breif	デストラクタ
	 */
	~idx_mgr();

	/*!
	 * @breif	用意するインデックス番号の数を設定する。
	 *
	 * 既に設定済みの場合、現時点までの状態を破棄し、指定された数のインデックス番号を再度構築し直す。
	 *
	 * @note
	 * このI/Fはスレッドセーフではありません。
	 */
	void set_idx_size( const int idx_size_arg );

	/*!
	 * @breif	利用可能なインデックス番号を取り出す。
	 *
	 * @return	利用可能なインデックス番号
	 * @retval	-1	利用可能なインデックス番号がない
	 *
	 * @note
	 * このI/Fはスレッドセーフで、ロックフリーです。
	 */
	int pop( void );

	/*!
	 * @breif	使用が完了したインデックス番号を返却する
	 *
	 * @note
	 * このI/Fはスレッドセーフで、ロックフリーです。
	 */
	void push(
		const int idx_arg   //!< [in] 返却するインデックス番号
	);

	/*!
	 * @breif	dump for debug
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
	int                           idx_size_;                  //!< 割り当てられたインデックス番号の数
	int                           idx_size_ver_;              //!< 割り当てられたインデックス番号の数の情報のバージョン番号
	idx_mgr_element*              p_idx_mgr_element_array_;   //!< インデックス番号を管理情報を保持する配列
	idx_element_storage_mgr       invalid_element_storage_;   //!< インデックス番号を所持しない要素を管理するストレージ
	idx_element_storage_mgr       valid_element_storage_;     //!< インデックス番号を所持する要素を管理するストレージ
	dynamic_tls<waiting_idx_list> tls_waiting_idx_list_;      //!< 滞留インデックスを管理するスレッドローカルリスト
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif /* INC_ALCONCURRENT_LF_MEM_ALLOC_IDX_MGR_HPP_ */
