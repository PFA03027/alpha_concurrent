/**
 * @file lf_mem_alloc_slot_array.hpp
 * @author Teruaki Ata <PFA03027@nifty.com>
 * @brief header file of slot array and slot array manager
 * @version 0.1
 * @date 2023-08-06
 *
 * @copyright Copyright (c) 2023 Teruaki Ata <PFA03027@nifty.com>
 *
 */

#ifndef LF_MEM_ALLOC_SLOT_ARRAY_HPP_
#define LF_MEM_ALLOC_SLOT_ARRAY_HPP_

#include <atomic>
#include <cstddef>
#include <mutex>
#include <type_traits>

#include "alconcurrent/dynamic_tls.hpp"

#include "lf_mem_alloc_basic_allocator.hpp"
#include "lf_mem_alloc_slot.hpp"

namespace alpha {
namespace concurrent {

namespace internal {
////////////////////////////////////////////////////////////////////////////////////////////////////////

class chunk_header_multi_slot;

struct threadlocal_slot_info_pointer_handler {
	/**
	 * @brief 型Tのオブジェクトを生成するファクトリ関数
	 *
	 * 型Tのオブジェクトはデフォルトコンストラクタで生成される
	 */
	uintptr_t allocate( void )
	{
		return 0;
	}

	/*!
	 * @brief スレッド終了時に、allocateで生成されたオブジェクトの破棄処理を行う関数
	 */
	void deallocate(
		uintptr_t p_data   //!< [in] allocateで生成されたオブジェクトへのポインタ
	)
	{
		return;
	}
};

struct slot_array_mgr {
	const size_t                                                              alloc_size_;                     //!< 自身のslot_arrayのために確保した領域のバイト数
	const size_t                                                              num_of_slots_;                   //!< 自身のslot_arrayで管理しているslot数
	const size_t                                                              expected_n_per_slot_;            //!< 自身のslot_arrayで管理しているslotが期待しているallocateサイズ
	const size_t                                                              slot_container_size_of_this_;    //!< 自身のslot_arrayで管理しているslot_container1つ分のバイト数
	std::atomic<chunk_header_multi_slot*>                                     p_owner_chunk_header_;           //!< 自身のslot_arrayの所有権を持っているchunk_header_multi_slotへのポインタ
	std::atomic<slot_header_of_array*>                                        p_free_slot_stack_head_;         //!< 未使用状態のslot_header_of_arrayスタックの、先頭slot_header_of_arrayへのポインタ
	std::mutex                                                                mtx_consignment_stack_;          //!< スレッド終了時のハザード中のスロットを受け取るスタックの排他制御用mutex
	slot_header_of_array*                                                     p_consignment_stack_head_;       //!< スレッド終了時のハザード中のスロットを受け取るスタックの、先頭slot_header_of_arrayへのポインタ
	dynamic_tls<slot_header_of_array*, threadlocal_slot_info_pointer_handler> tls_p_hazard_slot_stack_head_;   //!< ハザード中のスロットを受け取るスレッド毎のスタックの、先頭slot_header_of_arrayへのポインタ
	slot_container* const                                                     p_slot_container_top;            //!< slot_container配列の先頭へのポインタ
	slot_header_of_array                                                      slot_header_array_[0];           //!< 以降のアドレスにslot_header_of_arrayの可変長サイズ配列を保持するメモリ領域が続く。

	/**
	 * @brief Construct a new slot array mgr object
	 *
	 * @param p_owner pointer to chunk_header_multi_slot that is owner of this slot_array_mgr
	 * @param num_of_slots number of slots to allocate
	 * @param n expected allocatable memory size of a slot
	 */
	slot_array_mgr( chunk_header_multi_slot* p_owner, size_t num_of_slots, size_t n );

	/**
	 * @brief allocate and generate slot_array_mgr instance
	 *
	 * @note
	 * slot_array_mgrのインスタンスの生成は、直接配置new演算子を使えば構築可能だが、間違えないように構築用のmakeするI/Fを用意する
	 *
	 * @param p_owner pointer to chunk_header_multi_slot that is owner of this slot_array_mgr
	 * @param num_of_slots number of slots to allocate
	 * @param n expected allocatable memory size of a slot
	 * @return slot_array_mgr* pointer to slot_array_mgr instance
	 */
	static inline slot_array_mgr* make_instance( chunk_header_multi_slot* p_owner, size_t num_of_slots, size_t n )
	{
		return new ( num_of_slots, n ) alpha::concurrent::internal::slot_array_mgr( p_owner, num_of_slots, n );
	}

	/**
	 * @brief Get the pointer of slot_container object
	 *
	 * @param idx index number
	 * @return slot_container* the pointer of slot object
	 *
	 * @exception std::out_of_range if idx is over the number of slots of this instance
	 */
	inline const slot_container* unchk_get_pointer_of_slot_container( size_t idx ) const
	{
		uintptr_t addr_top                = reinterpret_cast<uintptr_t>( p_slot_container_top );
		uintptr_t addr_idx_slot_container = addr_top + slot_container_size_of_this_ * idx;
		return reinterpret_cast<const slot_container*>( addr_idx_slot_container );
	}
	inline slot_container* unchk_get_pointer_of_slot_container( size_t idx )
	{
		uintptr_t addr_top                = reinterpret_cast<uintptr_t>( p_slot_container_top );
		uintptr_t addr_idx_slot_container = addr_top + slot_container_size_of_this_ * idx;
		return reinterpret_cast<slot_container*>( addr_idx_slot_container );
	}

	/**
	 * @brief Get the pointer of slot object
	 *
	 * @param idx index number
	 * @return slot_header_of_array* the pointer of slot object
	 *
	 * @exception std::out_of_range if idx is over the number of slots of this instance
	 */
	inline const slot_header_of_array* get_pointer_of_slot( size_t idx ) const
	{
		if ( idx >= num_of_slots_ ) {
			char buff[128];
			snprintf( buff, 128, "over slot range. num of slot is %zu. but required idx is %zu", num_of_slots_, idx );
			throw std::out_of_range( buff );
		}
		return &( slot_header_array_[idx] );
	}
	inline slot_header_of_array* get_pointer_of_slot( size_t idx )
	{
		if ( idx >= num_of_slots_ ) {
			char buff[128];
			snprintf( buff, 128, "over slot range. num of slot is %zu. but required idx is %zu", num_of_slots_, idx );
			throw std::out_of_range( buff );
		}
		return &( slot_header_array_[idx] );
	}

	inline slot_header_of_array* begin_slot_array( void )
	{
		return &( slot_header_array_[0] );
		;
	}

	inline const slot_header_of_array* begin_slot_array( void ) const
	{
		return &( slot_header_array_[0] );
		;
	}

	inline slot_header_of_array* end_slot_array( void )
	{
		return &( slot_header_array_[num_of_slots_] );
	}

	inline const slot_header_of_array* end_slot_array( void ) const
	{
		return &( slot_header_array_[num_of_slots_] );
	}

	/**
	 * @brief Get the next pointer of slot object
	 *
	 * If p_cur is bigger than end_slot_array(), return p_cur.
	 *
	 * @param p_cur the current pointer of slot object
	 * @return slot_header_of_array* the next pointer of slot object
	 */
	inline slot_header_of_array* get_next_pointer_of_slot( slot_header_of_array* p_cur ) const
	{
		if ( p_cur >= end_slot_array() ) {
			return p_cur;
		}

		p_cur++;
		return p_cur;
	}

	/**
	 * @brief Get the next pointer of slot object
	 *
	 * @warning this I/F does not check the out of range error
	 *
	 * @param p_cur the current pointer of slot object
	 * @return slot_header_of_array* the next pointer of slot object
	 */
	inline slot_header_of_array* unchk_get_next_pointer_of_slot( slot_header_of_array* p_cur ) const
	{
		p_cur++;
		return p_cur;
	}

	/**
	 * @brief Get the previous pointer of slot object
	 *
	 * @warning this I/F does not check the out of range error
	 *
	 * @param p_cur the current pointer of slot object
	 * @return slot_header_of_array* the previous pointer of slot object
	 */
	inline slot_header_of_array* unchk_get_pre_pointer_of_slot( slot_header_of_array* p_cur ) const
	{
		p_cur--;
		return p_cur;
	}

	/**
	 * @brief 指定されたインデックス番号スロットからメモリ割り当てを行う。
	 *
	 * @warning
	 * 割り当て済みかどうかのチェックは行わない。
	 *
	 * @param idx 割り当てを行ってほしいスロットのidx番号
	 * @param n 割り当ててほしいメモリサイズ
	 * @param req_alignsize アライメント要求値
	 * @return void* 割り当てられたメモリへのポインタ。nullptrの場合、スロットのサイズ不足等の理由により割り当て失敗。
	 */
	inline void* allocate( size_t idx, size_t n, size_t req_alignsize )
	{
		return get_pointer_of_slot( idx )->allocate( unchk_get_pointer_of_slot_container( idx ), slot_container_size_of_this_, n, req_alignsize );
	}

	static size_t get_slot_idx_from_assignment_p( void* p_mem );

	void* operator new( std::size_t n );                                                                             // usual new...(1)   このクラスでは使用してはならないnew operator
	void  operator delete( void* p_mem ) noexcept;                                                                   // usual delete...(2)

	void* operator new[]( std::size_t n );                                                                           // usual new...(1)   このクラスでは使用してはならないnew operator
	void  operator delete[]( void* p_mem ) noexcept;                                                                 // usual delete...(2)   このクラスでは使用してはならないdelete operator

	void* operator new( std::size_t n_of_slot_array_mgr, size_t num_of_slots_, size_t expected_alloc_n_per_slot );   // placement new    可変長部分の領域も確保するnew operator
	void  operator delete( void* p, void* p2 ) noexcept;                                                             // placement delete...(3)   このクラスでは使用してはならないdelete operator。このdelete operator自身は何もしない。

	static constexpr size_t calc_one_slot_container_bytes( size_t n )
	{
		return slot_container::calc_slot_container_size( n, default_slot_alignsize );
	}
	static constexpr size_t calc_total_slot_container_array_bytes( size_t num_of_slots, size_t n )
	{
		return calc_one_slot_container_bytes( n ) * num_of_slots;
	}
};

static_assert( std::is_standard_layout<slot_array_mgr>::value, "slot_array_mgr should be standard-layout type" );

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
