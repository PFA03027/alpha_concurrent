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
#include "lf_mem_alloc_lifo_free_node_list.hpp"
#include "lf_mem_alloc_slot.hpp"

namespace alpha {
namespace concurrent {

namespace internal {
////////////////////////////////////////////////////////////////////////////////////////////////////////

class chunk_header_multi_slot;

struct slot_array_mgr {
	const size_t                          alloc_size_;                    //!< 自身のslot_array_mgrのために確保した領域のバイト数
	const size_t                          num_of_slots_;                  //!< 自身のslot_array_mgrで管理しているslot数
	const size_t                          expected_n_per_slot_;           //!< 自身のslot_array_mgrで管理しているslotが期待しているallocateサイズ
	const size_t                          slot_container_size_of_this_;   //!< 自身のslot_array_mgrで管理しているslot_container1つ分のバイト数
	std::atomic<chunk_header_multi_slot*> p_owner_chunk_header_;          //!< 自身のslot_array_mgrの所有権を持っているchunk_header_multi_slotへのポインタ
	free_node_stack<slot_header_of_array> free_slots_storage_;            //!< 割り当てていないslot_header_of_arrayのリストを管理する
	slot_container* const                 p_slot_container_top;           //!< slot_container配列の先頭へのポインタ
	slot_header_of_array                  slot_header_array_[0];          //!< 以降のアドレスにslot_header_of_arrayの可変長サイズ配列とslot_containerを保持するメモリ領域が続く。

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
	 * slot_array_mgrのインスタンスの生成は、配置new演算子を使えば直接構築可能だが、間違えないように構築用のmakeするI/Fを用意する
	 *
	 * @param p_owner pointer to chunk_header_multi_slot that is owner of this slot_array_mgr
	 * @param num_of_slots number of slots to allocate
	 * @param n expected allocatable memory size of a slot
	 * @return slot_array_mgr* pointer to slot_array_mgr instance
	 */
	static inline slot_array_mgr* make_instance( chunk_header_multi_slot* p_owner, size_t num_of_slots, size_t n )
	{
		return new ( num_of_slots, n ) slot_array_mgr( p_owner, num_of_slots, n );
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

	/**
	 * @brief 空きスロットからメモリ割り当てを試みる
	 *
	 * @param n 必要なメモリサイズ
	 * @param req_alignsize アライメント要求値
	 * @return void* 割り当てられたメモリへのアドレス。
	 * @retval nullptr 空きスロットがなく割り当てできなかった。
	 */
	void* allocate( size_t n, size_t req_alignsize = default_slot_alignsize )
	{
		if ( ( expected_n_per_slot_ + default_slot_alignsize ) < ( n + req_alignsize ) ) {
			// サイズ不足のため、確保に失敗
			return nullptr;
		}
		slot_header_of_array* p_free_slot = free_slots_storage_.pop();
		if ( p_free_slot == nullptr ) {
			// フリースロットがないため、確保に失敗
			return nullptr;
		}

		bool_size_t chk_ret_free_slot_idx = get_slot_idx_from_slot_header_of_array( p_free_slot );
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
		if ( !chk_ret_free_slot_idx.is_ok_ ) {
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_EXCEPTION
			std::string errlog = "recieved free slot is not belong to this slot_array_mgr";
			throw std::logic_error( errlog );
#else
			internal::LogOutput( log_type::ERR, "recieved free slot is not belong to this slot_array_mgr" );
			return nullptr;
#endif
		}
#endif

		return p_free_slot->allocate( unchk_get_pointer_of_slot_container( chk_ret_free_slot_idx.idx_ ), slot_container_size_of_this_, n, req_alignsize );
	}

	/**
	 * @brief 割り当てられたメモリに対応するslot_header_of_arrayの解放処理を行う
	 *
	 * @warning
	 * 対応するslot_array_mgrであることが確認済みであること。
	 *
	 * @param p_used_slot 割り当てられたメモリに対応するslot_header_of_arrayへのポインタ
	 */
	void deallocate( slot_header_of_array* p_used_slot )
	{
		p_used_slot->deallocate();
		free_slots_storage_.push( p_used_slot );
	}

	bool_size_t get_slot_idx_from_slot_header_of_array( slot_header_of_array* p_slot_header );

	void dump( int indent = 0 );

	void* operator new( std::size_t n );                                                                             // usual new...(1)   このクラスでは使用してはならないnew operator
	void  operator delete( void* p_mem ) noexcept;                                                                   // usual delete...(2)

	void* operator new[]( std::size_t n );                                                                           // usual new...(1)   このクラスでは使用してはならないnew operator
	void  operator delete[]( void* p_mem ) noexcept;                                                                 // usual delete...(2)   このクラスでは使用してはならないdelete operator

	void* operator new( std::size_t n_of_slot_array_mgr, size_t num_of_slots_, size_t expected_alloc_n_per_slot );   // placement new    可変長部分の領域も確保するnew operator
	void  operator delete( void* p, void* p2 ) noexcept;                                                             // placement delete...(3)   このクラスでは使用してはならないdelete operator。このdelete operator自身は何もしない。

private:
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

	inline slot_header_of_array* begin_slot_array( void )
	{
		return &( slot_header_array_[0] );
	}

	inline const slot_header_of_array* begin_slot_array( void ) const
	{
		return &( slot_header_array_[0] );
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
