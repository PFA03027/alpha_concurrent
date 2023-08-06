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

struct slot_info {
	std::atomic<slot_info*> p_next_;   //!< stackリストとして繋がる次のslot_infoへのポインタ
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	bt_info alloc_bt_info_;            //!< backtrace information when is allocated
	bt_info free_bt_info_;             //!< backtrace information when is free
#endif

	constexpr slot_info( slot_info* p_next_arg = nullptr )
	  : p_next_( p_next_arg )
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	  , alloc_bt_info_()   //!< backtrace information when is allocated
	  , free_bt_info_()    //!< backtrace information when is free
#endif
	{
	}
};

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
	const size_t                                                   alloc_size_;                     //!< 自身のslot_arrayのために確保した領域のバイト数
	const size_t                                                   num_of_slots_;                   //!< 自身のslot_arrayで管理しているslot数
	const size_t                                                   expected_n_per_slot_;            //!< 自身のslot_arrayで管理しているslotが期待しているallocateサイズ
	const size_t                                                   slot_size_of_this_;              //!< 自身のslot_arrayで管理しているslot1つ分のバイト数
	std::atomic<chunk_header_multi_slot*>                          p_owner_chunk_header_;           //!< 自身のslot_arrayの所有権を持っているchunk_header_multi_slotへのポインタ
	std::atomic<slot_info*>                                        p_free_slot_stack_head_;         //!< 未使用状態のslot_infoのスタックの、先頭slot_infoへのポインタ
	std::mutex                                                     mtx_consignment_stack_;          //!< スレッド終了時のハザード中のスロットを受け取るスタックの排他制御用mutex
	slot_info*                                                     p_consignment_stack_head_;       //!< スレッド終了時のハザード中のスロットを受け取るスタックの、先頭slot_infoへのポインタ
	dynamic_tls<slot_info*, threadlocal_slot_info_pointer_handler> tls_p_hazard_slot_stack_head_;   //!< ハザード中のスロットを受け取るスレッド毎のスタックの、先頭slot_infoへのポインタ
	slot_header_of_array* const                                    p_top_of_slots_;                 //!< スロット配列の先頭へのポインタ
	slot_info                                                      slot_container_[0];              //!< 以降のアドレスに可変長サイズのslot_infoとslotの配列を保持するメモリ領域が続く。

	slot_array_mgr( chunk_header_multi_slot* p_owner, size_t num_of_slots, size_t n );

	inline slot_header_of_array* get_pointer_of_slot( size_t idx ) const
	{
		if ( idx >= num_of_slots_ ) {
			std::string errlog = "over slot range. num of slot is ";
			errlog += std::to_string( num_of_slots_ );
			errlog += "but required idx is ";
			errlog += std::to_string( idx );
			throw std::out_of_range( errlog );
		}
		uintptr_t offset_bytes    = static_cast<uintptr_t>( idx * slot_size_of_this_ );
		uintptr_t addr_idxed_slot = reinterpret_cast<uintptr_t>( p_top_of_slots_ ) + offset_bytes;
		return reinterpret_cast<slot_header_of_array*>( addr_idxed_slot );
	}

	inline slot_header_of_array* get_next_pointer_of_slot( slot_header_of_array* p_cur ) const
	{
		uintptr_t addr_idxed_slot = reinterpret_cast<uintptr_t>( p_cur ) + static_cast<uintptr_t>( slot_size_of_this_ );
		return reinterpret_cast<slot_header_of_array*>( addr_idxed_slot );
	}

	inline void* allocate( size_t idx, size_t n, size_t req_alignsize )
	{
		return get_pointer_of_slot( idx )->allocate( slot_size_of_this_, n, req_alignsize );
	}

	static size_t get_slot_idx_from_assignment_p( void* p_mem );

	void* operator new( std::size_t n );                                                                             // usual new...(1)   このクラスでは使用してはならないnew operator
	void  operator delete( void* p_mem ) noexcept;                                                                   // usual delete...(2)

	void* operator new[]( std::size_t n );                                                                           // usual new...(1)   このクラスでは使用してはならないnew operator
	void  operator delete[]( void* p_mem ) noexcept;                                                                 // usual delete...(2)   このクラスでは使用してはならないdelete operator

	void* operator new( std::size_t n_of_slot_array_mgr, size_t num_of_slots_, size_t expected_alloc_n_per_slot );   // placement new    可変長部分の領域も確保するnew operator
	void  operator delete( void* p, void* p2 ) noexcept;                                                             // placement delete...(3)   このクラスでは使用してはならないdelete operator。このdelete operator自身は何もしない。

	static constexpr size_t calc_total_slot_info_array_bytes( size_t num_of_slots )
	{
		return sizeof( slot_info[num_of_slots] );
	}
	static constexpr size_t calc_one_slot_bytes( size_t n )
	{
		return calc_total_slot_size_of_slot_header_of<slot_header_of_array>( n, default_slot_alignsize );
	}
	static constexpr size_t calc_total_slot_array_bytes( size_t num_of_slots, size_t n )
	{
		return calc_one_slot_bytes( n ) * num_of_slots;
	}
	static constexpr size_t calc_total_slot_array_mgr_bytes( size_t num_of_slots, size_t n )
	{
		return calc_total_slot_info_array_bytes( num_of_slots ) + calc_total_slot_array_bytes( num_of_slots, n );
	}
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
