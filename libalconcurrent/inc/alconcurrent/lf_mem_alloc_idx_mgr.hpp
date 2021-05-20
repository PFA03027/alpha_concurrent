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
};

/*!
 * @breif	ハーザードポインタ登録中で滞留している要素を管理するリスト
 *
 * スレッドローカルデータとして管理されるため、排他制御は不要なクラス
 */
struct waiting_element_list {
public:
	waiting_element_list( const int idx_buff_size_arg );
	~waiting_element_list();

	void             reset_and_set_size( const int idx_buff_size_arg );
	idx_mgr_element* pop( void );
	void             push( idx_mgr_element* );

	int  pop_from_tls( void );
	void push_to_tls( const int valid_idx );

private:
	idx_mgr_element* head_;
	idx_mgr_element* tail_;
	int              idx_buff_size_;
	int              idx_top_idx_;
	int*             p_idx_buff_;
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

private:
	static constexpr int hzrd_max_slot_ = 6;
	enum class hazard_ptr_idx : int {
		PUSH_INV_CUR = 0,
		POP_INV_CUR  = 1,
		POP_INV_NEXT = 2,
		PUSH_VAL_CUR = 3,
		POP_VAL_CUR  = 4,
		POP_VAL_NEXT = 5,
	};
	using scoped_hazard_ref = hazard_ptr_scoped_ref<idx_mgr_element, hzrd_max_slot_>;

	int                                         idx_size_;                     //!< 割り当てられたインデックス番号の数
	idx_mgr_element*                            p_idx_mgr_element_array_;      //!< インデックス番号を管理情報を保持する配列
	std::atomic<idx_mgr_element*>               invalid_element_stack_head_;   //!< インデックス番号を所持しない要素を管理するスタック構造の先頭要素へのポインタ
	std::atomic<idx_mgr_element*>               valid_element_stack_head_;     //!< インデックス番号を所持する要素を管理するスタック構造の先頭要素へのポインタ
	dynamic_tls<waiting_element_list>           tls_waiting_list_;             //!< 滞留要素を管理するスレッドローカルリスト
	hazard_ptr<idx_mgr_element, hzrd_max_slot_> hzrd_element_;                 //!< ハザードポインタを管理する構造体。

	idx_mgr_element* stack_pop_element(
		std::atomic<idx_mgr_element*>& head_,                               //!< [in] reference of atomic pointer to stack head
		std::atomic<idx_mgr_element*> idx_mgr_element::*p_next,             //!< [in] member pointer that points to next element
		const hazard_ptr_idx                            hzd_pop_cur_slot,   //!< [in] hazard pointer slot to store pop current pointer
		const hazard_ptr_idx                            hzd_pop_next_slot   //!< [in] hazard pointer slot to store pop next pointer
	);
	void stack_push_element(
		idx_mgr_element*               p_push_element,                      //!< [in] pointer of element to push
		std::atomic<idx_mgr_element*>& head_,                               //!< [in] reference of atomic pointer to stack head
		std::atomic<idx_mgr_element*> idx_mgr_element::*p_next,             //!< [in] member pointer that points to next element
		const hazard_ptr_idx                            hzd_push_cur_slot   //!< [in] hazard pointer slot to store pop current pointer
	);
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif /* INC_ALCONCURRENT_LF_MEM_ALLOC_IDX_MGR_HPP_ */
