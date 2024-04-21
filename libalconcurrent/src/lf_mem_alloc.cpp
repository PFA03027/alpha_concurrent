/*!
 * @file	lf_mem_alloc.cpp
 * @brief	semi lock-free memory allocater
 * @author	Teruaki Ata
 * @date	Created on 2021/05/14
 * @details
 *
 * Copyright (C) 2021 by alpha Teruaki Ata <PFA03027@nifty.com>
 */

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/internal/lf_mem_alloc_internal.hpp"
#include "alconcurrent/lf_mem_alloc.hpp"

#include "mmap_allocator.hpp"
#include "utility.hpp"

#include "lf_mem_alloc_basic_allocator.hpp"
#include "lf_mem_alloc_slot.hpp"
#include "lf_mem_alloc_slot_array.hpp"

namespace alpha {
namespace concurrent {

bool test_platform_std_atomic_lockfree_condition( void )
{
	bool ans;

	ans = ( ATOMIC_BOOL_LOCK_FREE == 2 );
	ans = ans && std::atomic<internal::chunk_control_status> { internal::chunk_control_status::EMPTY }.is_lock_free();
	ans = ans && std::atomic<internal::chunk_header_multi_slot*> { nullptr }.is_lock_free();
	ans = ans && ( ATOMIC_INT_LOCK_FREE == 2 );

	// using fp_t = void ( * )( void* );
	ans = ans && std::atomic<void ( * )( void* )>().is_lock_free();

	return ans;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
namespace internal {

chunk_statistics chunk_list_statistics::get_statistics( void ) const
{
	chunk_statistics ans { 0 };

	ans.chunk_num_       = chunk_num_.load( std::memory_order_acquire );
	ans.valid_chunk_num_ = valid_chunk_num_.load( std::memory_order_acquire );
	ans.total_slot_cnt_  = total_slot_cnt_.load( std::memory_order_acquire );
	ans.free_slot_cnt_   = free_slot_cnt_.load( std::memory_order_acquire );
	ans.consum_cnt_      = consum_cnt_.load( std::memory_order_acquire );
	ans.max_consum_cnt_  = max_consum_cnt_.load( std::memory_order_acquire );
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	ans.alloc_req_cnt_         = alloc_req_cnt_.load( std::memory_order_acquire );
	ans.error_alloc_req_cnt_   = alloc_req_err_cnt_.load( std::memory_order_acquire );
	ans.dealloc_req_cnt_       = dealloc_req_cnt_.load( std::memory_order_acquire );
	ans.error_dealloc_req_cnt_ = dealloc_req_err_cnt_.load( std::memory_order_acquire );
	ans.alloc_collision_cnt_   = alloc_collision_cnt_.load( std::memory_order_acquire );
	ans.dealloc_collision_cnt_ = dealloc_collision_cnt_.load( std::memory_order_acquire );
#endif

	return ans;
}

////////////////////////////////////////////////////////////////////////////////
struct slot_header {
	std::atomic<chunk_header_multi_slot*> at_p_chms_;   //!< pointer to chunk_header_multi_slot that is an owner of this slot
	std::atomic<std::uintptr_t>           at_mark_;     //!< checker mark
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	bt_info alloc_bt_info_;   //!< backtrace information when is allocated
	bt_info free_bt_info_;    //!< backtrace information when is free
#endif

	void set_addr_of_chunk_header_multi_slot(
		chunk_header_multi_slot* p_chms_arg   //!< [in] pointer to a parent "chunk_header_multi_slot"
	);
};

void slot_header::set_addr_of_chunk_header_multi_slot( chunk_header_multi_slot* p_chms_arg )
{
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	RECORD_BACKTRACE_GET_BACKTRACE( alloc_bt_info_ );
	if ( p_chms_arg != nullptr ) {
		RECORD_BACKTRACE_INVALIDATE_BACKTRACE( free_bt_info_ );
	} else {
		// mallocで確保された領域なので、free_bt_info_側の情報は、クリアする。
		free_bt_info_ = bt_info {};
	}
#endif

	// 今は簡単な仕組みで実装する
	at_p_chms_.store( p_chms_arg, std::memory_order_release );
	at_mark_.store( 0 - reinterpret_cast<std::uintptr_t>( p_chms_arg ) - 1, std::memory_order_release );

	return;
}

////////////////////////////////////////////////////////////////////////////////
chunk_header_multi_slot::chunk_header_multi_slot(
	const param_chunk_allocation& ch_param_arg,      //!< [in] chunk allocation paramter
	const unsigned int            owner_tl_id_arg,   //!< [in] owner tl_id_
	chunk_list_statistics*        p_chms_stat_arg    //!< [in] pointer to statistics inforation to store activity
	)
  : p_next_chunk_( nullptr )
  , status_( chunk_control_status::EMPTY )
  , owner_tl_id_( owner_tl_id_arg )
  , num_of_accesser_( 0 )
  , p_statistics_( p_chms_stat_arg )
  , slot_conf_ {}
  , p_slot_array_mgr_( nullptr )
{
	assert( p_statistics_ != nullptr );

	p_statistics_->chunk_num_++;

	alloc_new_chunk( ch_param_arg, owner_tl_id_ );

	return;
}

chunk_header_multi_slot::~chunk_header_multi_slot()
{
	p_statistics_->chunk_num_--;

	delete p_slot_array_mgr_;

	return;
}

constexpr std::size_t get_slot_header_size( void )
{
#if ( __cpp_constexpr >= 201304 )
	std::size_t tmp = sizeof( slot_header ) / default_slot_alignsize;

	std::size_t ans = ( tmp + 1 ) * default_slot_alignsize;

	return ans;
#else
	return ( ( sizeof( slot_header ) / default_slot_alignsize ) + 1 ) * default_slot_alignsize;
#endif
}

void chunk_header_multi_slot::set_slot_allocation_conf(
	const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
)
{
	// access可能状態なので、accesserのカウントアップを行い、access 開始を表明
	scoped_inout_counter_atomic_int cnt_inout( num_of_accesser_ );

	// 補正を行う
	std::size_t tmp               = ch_param_arg.size_of_one_piece_ / default_slot_alignsize;
	slot_conf_.size_of_one_piece_ = ( tmp + 1 ) * default_slot_alignsize;
	slot_conf_.num_of_pieces_     = ( ch_param_arg.num_of_pieces_ >= 2 ) ? ch_param_arg.num_of_pieces_ : 2;

	return;
}

bool chunk_header_multi_slot::alloc_new_chunk(
	const param_chunk_allocation& ch_param_arg,     //!< [in] chunk allocation paramter
	const unsigned int            owner_tl_id_arg   //!< [in] owner tl_id_
)
{
	// access可能状態なので、accesserのカウントアップを行い、access 開始を表明
	scoped_inout_counter_atomic_int cnt_inout( num_of_accesser_ );

	// get ownership to allocate a chunk	// access権を取得を試みる
	chunk_control_status expect = chunk_control_status::EMPTY;
	bool                 result = status_.compare_exchange_strong( expect, chunk_control_status::RESERVED_ALLOCATION, std::memory_order_acq_rel );
	if ( !result ) return false;
	// resultが真となったので、以降、このchunkに対する、領域確保の処理実行権を保有。

	set_slot_allocation_conf( ch_param_arg );

	// slot_array_mgrの確保
	p_slot_array_mgr_ = slot_array_mgr::make_instance( this, slot_conf_.num_of_pieces_, slot_conf_.size_of_one_piece_ );

	owner_tl_id_.store( owner_tl_id_arg, std::memory_order_release );

	p_statistics_->valid_chunk_num_.fetch_add( 1, std::memory_order_acq_rel );
	p_statistics_->total_slot_cnt_.fetch_add( slot_conf_.num_of_pieces_, std::memory_order_acq_rel );
	p_statistics_->free_slot_cnt_.fetch_add( slot_conf_.num_of_pieces_, std::memory_order_acq_rel );

	status_.store( chunk_control_status::NORMAL, std::memory_order_release );

	return true;
}

void* chunk_header_multi_slot::allocate_mem_slot_impl( size_t req_size, size_t req_align )
{
	// access可能状態かどうかを事前チェック
	if ( status_.load( std::memory_order_acquire ) != chunk_control_status::NORMAL ) return nullptr;

	// access可能状態なので、accesserのカウントアップを行い、access 開始を表明
	scoped_inout_counter_atomic_int cnt_inout( num_of_accesser_ );

	// access権を取得を試みる
	if ( status_.load( std::memory_order_acquire ) != chunk_control_status::NORMAL ) return nullptr;

#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	p_statistics_->alloc_req_cnt_++;
#endif

	void* p_ans = p_slot_array_mgr_->allocate( req_size, req_align );
	if ( p_ans == nullptr ) {
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
		p_statistics_->alloc_req_err_cnt_++;
#endif
		return nullptr;
	}

	return p_ans;
}

bool chunk_header_multi_slot::recycle_mem_slot_impl(
	void* p_recycle_addr   //!< [in] pointer to the memory slot to recycle.
)
{
	if ( p_recycle_addr == nullptr ) {
		return false;
	}

	// 自分のものかを確認する
	auto chk_ret = slot_container::get_slot_header_from_assignment_p( p_recycle_addr );
	if ( !chk_ret.is_ok_ ) {
#if 0
		// ヘッダが壊れているようなので、アドレス情報から、スロット情報を引き出せるか試す。
		if ( reinterpret_cast<uintptr_t>( p_recycle_addr ) < reinterpret_cast<uintptr_t>( p_slot_array_mgr_->p_slot_container_top ) ) {
			// slot_containerの範囲外なので、あきらめる。
			return false;
		}
		uintptr_t addr_diff   = reinterpret_cast<uintptr_t>( p_recycle_addr ) - reinterpret_cast<uintptr_t>( p_slot_array_mgr_->p_slot_container_top );
		uintptr_t slots_count = addr_diff / p_slot_array_mgr_->slot_container_size_of_this_;
		if ( slots_count >= p_slot_array_mgr_->num_of_slots_ ) {
			// slot_containerの範囲外なので、あきらめる。
			return false;
		}
		// ここに到達した時点で、自分のものであること判明

#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
		p_statistics_->dealloc_req_cnt_.fetch_add( 1, std::memory_order_acq_rel );
#endif
		// OK. correct address
		p_slot_array_mgr_->deallocate( p_slot_array_mgr_->get_pointer_of_slot( slots_count ) );
		return true;
#else
		return false;
#endif
	}

	if ( chk_ret.p_ush_->check_type() ) {
		// allocタイプなので、自分のものではない。
		return false;
	}
	slot_array_mgr* p_rcv_sam = chk_ret.p_ush_->arrayh_.mh_.get_mgr_pointer();
	if ( p_rcv_sam == nullptr ) {
		return false;
	}
	if ( p_rcv_sam->p_owner_chunk_header_ != this ) {
		return false;
	}
	// ここに到達した時点で、自分のものであること判明

	return unchk_recycle_mem_slot_impl( p_rcv_sam, &( chk_ret.p_ush_->arrayh_ ) );
}

bool chunk_header_multi_slot::unchk_recycle_mem_slot_impl(
	slot_array_mgr*       p_rcv_sam,
	slot_header_of_array* p_recycle_addr   //!< [in] pointer to the memory slot to recycle.
)
{
	// access可能状態かどうかを事前チェック
	if ( ( static_cast<unsigned int>( status_.load( std::memory_order_acquire ) ) & RecycleGroupStatusMask ) == 0 ) {
		return false;   // すでにaccessできない状態になっている。
	}

	// access可能状態なので、accesserのカウントアップを行い、access 開始を表明
	scoped_inout_counter_atomic_int cnt_inout( num_of_accesser_ );

	if ( ( static_cast<unsigned int>( status_.load( std::memory_order_acquire ) ) & RecycleGroupStatusMask ) == 0 ) {
		return false;   // すでにaccessできない状態になっている。
	}

	// 自分が保持するスロットであることが判明済みのため、即座に解放処理を開始する。
	p_rcv_sam->deallocate( p_recycle_addr );

	p_statistics_->free_slot_cnt_.fetch_add( 1, std::memory_order_acq_rel );
	p_statistics_->consum_cnt_.fetch_sub( 1, std::memory_order_acq_rel );

	return true;
}

void* chunk_header_multi_slot::try_allocate_mem_slot_impl(
	const unsigned int expect_tl_id_arg,   //!< [in] owner tl_id_
	const unsigned int owner_tl_id_arg,    //!< [in] owner tl_id_
	const size_t       req_size,           //!< [in] requested size
	const size_t       req_align           //!< [in] requested align size
)
{
	// access可能状態かどうかを事前チェック
	if ( ( static_cast<unsigned int>( status_.load( std::memory_order_acquire ) ) & TryAllocGroupStatusMask ) == 0 ) {
		return nullptr;   // すでにaccessできない状態になっている。
	}

	// access可能状態なので、accesserのカウントアップを行い、access 開始を表明
	scoped_inout_counter_atomic_int cnt_inout( num_of_accesser_ );

	// access権を取得を試みる
	chunk_control_status cur_as_st = status_.load( std::memory_order_acquire );
	if ( ( static_cast<unsigned int>( cur_as_st ) & TryAllocGroupStatusMask ) == 0 ) {
		return nullptr;   // すでにaccessできない状態になっている。
	}

	// オーナー権の確認 or 確保を試みる
	unsigned int expect_tl_id = expect_tl_id_arg;
	if ( !owner_tl_id_.compare_exchange_strong( expect_tl_id, owner_tl_id_arg, std::memory_order_acq_rel ) ) {
		return nullptr;
	}

	// NORMAL状態へ復帰を試みる
	if ( cur_as_st == chunk_control_status::RESERVED_DELETION ) {
		if ( !unset_delete_reservation() ) {
			return nullptr;
		}
	}

	// オーナー権の確保と、NORMAL状態への復帰に成功したので、メモリスロットの確保を試みる
	return allocate_mem_slot( req_size, req_align );
}

bool chunk_header_multi_slot::set_delete_reservation( void )
{
	// access可能状態なので、accesserのカウントアップを行い、access 開始を表明
	scoped_inout_counter_atomic_int cnt_inout( num_of_accesser_ );

	chunk_control_status expect = chunk_control_status::NORMAL;
	bool                 result = std::atomic_compare_exchange_strong( &status_, &expect, chunk_control_status::RESERVED_DELETION );
	return result;
}

bool chunk_header_multi_slot::unset_delete_reservation( void )
{
	// access可能状態なので、accesserのカウントアップを行い、access 開始を表明
	scoped_inout_counter_atomic_int cnt_inout( num_of_accesser_ );

	chunk_control_status expect = chunk_control_status::RESERVED_DELETION;
	bool                 result = std::atomic_compare_exchange_strong( &status_, &expect, chunk_control_status::NORMAL );
	return result;
}

bool chunk_header_multi_slot::exec_deletion( void )
{
	// access者がいないことを確認する
	if ( num_of_accesser_.load( std::memory_order_acquire ) != 0 ) return false;

	if ( owner_tl_id_.load( std::memory_order_acquire ) != NON_OWNERED_TL_ID ) {
		// 明確な所有者スレッドが存在するため、削除対象としない。
		return false;
	}

	chunk_control_status expect;
	bool                 result;

	// 削除予約付けを試みる。
	expect = chunk_control_status::NORMAL;
	result = status_.compare_exchange_strong( expect, chunk_control_status::RESERVED_DELETION, std::memory_order_acq_rel );
	if ( !result ) {
		// 削除予約付けに失敗した。ただし、すでに、削除予約付け済みかもしれないため、チェックする。
		if ( expect != chunk_control_status::RESERVED_DELETION ) {
			// すでに何らかのほかの状態に遷移済みのため、削除を取りやめる。
			return false;
		}
	}
	for ( size_t i = 0; i < p_slot_array_mgr_->num_of_slots_; i++ ) {
		if ( p_slot_array_mgr_->slot_header_array_[i].mh_.offset_to_tail_padding_.load( std::memory_order_acquire ) != 0 ) {
			// 使用中のslotが見つかったため、削除を取りやめる。
			return false;
		}
	}

	// DELETEの予告を試みる。
	expect = chunk_control_status::RESERVED_DELETION;
	if ( !status_.compare_exchange_strong( expect, chunk_control_status::ANNOUNCEMENT_DELETION, std::memory_order_acq_rel ) ) {
		return false;
	}

	for ( size_t i = 0; i < p_slot_array_mgr_->num_of_slots_; i++ ) {
		if ( p_slot_array_mgr_->slot_header_array_[i].mh_.offset_to_tail_padding_.load( std::memory_order_acquire ) != 0 ) {
			// 使用中のslotが見つかったため、削除を取りやめる。
			status_.store( chunk_control_status::RESERVED_DELETION, std::memory_order_release );
			return false;
		}
	}

	// access者の有無を再確認
	if ( num_of_accesser_.load( std::memory_order_acquire ) != 0 ) {
		// access者がいるため、削除を取りやめる。
		status_.store( chunk_control_status::RESERVED_DELETION, std::memory_order_release );
		return false;
	}

	// DELETEの実行権の獲得を試みる。
	expect = chunk_control_status::ANNOUNCEMENT_DELETION;
	if ( !status_.compare_exchange_strong( expect, chunk_control_status::ANNOUNCEMENT_DELETION, std::memory_order_acq_rel ) ) {
		return false;
	}

	std::lock_guard<std::recursive_mutex> lg( dynamic_tls_global_exclusive_control_for_destructions );

	delete p_slot_array_mgr_;   // スロット配列を削除する
	p_slot_array_mgr_ = nullptr;
	owner_tl_id_.store( NON_OWNERED_TL_ID, std::memory_order_release );

	p_statistics_->valid_chunk_num_.fetch_sub( 1, std::memory_order_acq_rel );
	p_statistics_->total_slot_cnt_.fetch_sub( slot_conf_.num_of_pieces_, std::memory_order_acq_rel );
	p_statistics_->free_slot_cnt_.fetch_sub( slot_conf_.num_of_pieces_, std::memory_order_acq_rel );

	status_.store( chunk_control_status::EMPTY, std::memory_order_release );

	return true;
}

slot_chk_result chunk_header_multi_slot::get_chunk(
	void* p_addr   //!< [in] void* address that allocate_mem_slot() returns
)
{
	auto chk_ret = slot_container::get_slot_header_from_assignment_p( p_addr );
	if ( !chk_ret.is_ok_ ) {
		// 多分slot_containerのヘッダが壊れてるか、無関係のアドレスが渡された
		return slot_chk_result { false, nullptr, nullptr, nullptr };
	}
	if ( chk_ret.p_ush_->check_type() ) {
		// allocタイプなので、chunk_header_multi_slotへのポインタ情報はない
		return slot_chk_result { false, nullptr, nullptr, nullptr };
	}
	slot_array_mgr* p_rcv_sam = chk_ret.p_ush_->arrayh_.mh_.get_mgr_pointer();
	if ( p_rcv_sam == nullptr ) {
		// 多分slot_header_of_arrayヘッダが壊れてる。
		return slot_chk_result { false, nullptr, nullptr, nullptr };
	}

	return slot_chk_result { true, p_rcv_sam->p_owner_chunk_header_, p_rcv_sam, &( chk_ret.p_ush_->arrayh_ ) };
}

chunk_statistics chunk_header_multi_slot::get_statistics( void ) const
{
	// access可能状態なので、accesserのカウントアップを行い、access 開始を表明
	scoped_inout_counter_atomic_int cnt_inout( num_of_accesser_ );

	chunk_statistics ans = p_statistics_->get_statistics();

	ans.alloc_conf_ = slot_conf_;

	return ans;
}

/*!
 * @brief	dump for debug
 */
void chunk_header_multi_slot::dump( void )
{
	// access可能状態なので、accesserのカウントアップを行い、access 開始を表明
	scoped_inout_counter_atomic_int cnt_inout( num_of_accesser_ );

	internal::LogOutput( log_type::DUMP,
	                     "object chunk_header_multi_slot_%p as %p {\n"
	                     "\t alloc_conf_.size_of_one_piece_ = %u \n"
	                     "\t alloc_conf_.num_of_pieces_ = %u \n"
	                     "}",
	                     this, this,
	                     slot_conf_.size_of_one_piece_,
	                     slot_conf_.num_of_pieces_
	                     //
	);

	return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::atomic<unsigned int> chunk_list::tl_chunk_param::tl_id_counter_( 1 );

unsigned int chunk_list::tl_chunk_param::get_new_tl_id( void )
{
#ifdef ALCONCURRENT_CONF_PREFER_TO_SHARE_CHUNK
	// tl_idを固定値とするとことで、chunkの区別がつかないようにする。
	return 1;
#else
	unsigned int ans = tl_id_counter_.fetch_add( 1, std::memory_order_acq_rel );
	if ( ans == NON_OWNERED_TL_ID ) {
		ans = tl_id_counter_.fetch_add( 1, std::memory_order_acq_rel );
	}
	return ans;
#endif
}

chunk_list::tl_chunk_param::tl_chunk_param(
	chunk_list* p_owner_chunk_list_arg,   //!< [in] pointer to onwer chunk_list
	size_t      init_num_of_pieces_arg    //!< [in] initiall number of slots for allocation
	)
  : p_owner_chunk_list_( p_owner_chunk_list_arg )
  , tl_id_( get_new_tl_id() )
  , num_of_pieces_( init_num_of_pieces_arg )
  , tls_p_hint_chunk( nullptr )
{
	return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void chunk_list::mark_as_reserved_deletion(
	unsigned int             target_tl_id_arg,    //!< [in] 削除予約対象に変更するtl_id_
	chunk_header_multi_slot* p_non_deletion_arg   //!< [in] 削除予約対象としないchunk_header_multi_slotへのポインタ
)
{
	for ( auto&& e : p_top_chunk_ ) {
		if ( e.owner_tl_id_.load( std::memory_order_acquire ) != target_tl_id_arg ) continue;
		if ( &e == p_non_deletion_arg ) continue;

		e.set_delete_reservation();
	}

	return;
}

size_t chunk_list::get_cur_max_slot_size(
	unsigned int target_tl_id_arg   //!< [in] オーナー権を開放する対象のtl_id_
)
{
	size_t ans_cur_max_size = 0;
	for ( auto&& e : p_top_chunk_ ) {
		if ( e.owner_tl_id_.load( std::memory_order_acquire ) != target_tl_id_arg ) continue;

		const param_chunk_allocation& pca = e.get_param_chunk_allocation();
		if ( pca.num_of_pieces_ > ans_cur_max_size ) {
			ans_cur_max_size = pca.num_of_pieces_;
		}
	}

	return ans_cur_max_size;
}

void chunk_list::release_all_of_ownership(
	unsigned int                   target_tl_id_arg,         //!< [in] オーナー権を開放する対象のtl_id_
	const chunk_header_multi_slot* p_non_release_chunk_arg   //!< [in] オーナー権解放の対象外とするchunk_header_multi_slotへのポインタ。指定しない場合は、nullptrを指定すること。
)
{
	for ( auto&& e : p_top_chunk_ ) {
		if ( e.owner_tl_id_.load( std::memory_order_acquire ) != target_tl_id_arg ) continue;
		if ( &e == p_non_release_chunk_arg ) continue;

		e.owner_tl_id_.store( NON_OWNERED_TL_ID, std::memory_order_release );
	}

	return;
}

void* chunk_list::allocate_mem_slot( size_t req_size, size_t req_align )
{
	void* p_ans = nullptr;

	// hintに登録されたchunkから空きスロット検索を開始する。
	tl_chunk_param& hint_params = tls_hint_.get_tls_instance();

	chunk_header_multi_slot* const p_start_chms = hint_params.tls_p_hint_chunk;
	chunk_header_multi_slot*       p_cur_chms   = nullptr;
	if ( p_start_chms != nullptr ) {
		// 自スレッド用の割り当てが終わっている場合、自スレッド用のchunk_header_multi_slotからの割り当てを試みる
		p_cur_chms = p_start_chms;
		while ( p_cur_chms != nullptr ) {
			if ( p_cur_chms->owner_tl_id_.load( std::memory_order_acquire ) == hint_params.tl_id_ ) {
				// tl_id_が一致する場合は、自スレッドの持ち物となる。自スレッドの持ち物のchunk_header_multi_slotからメモリを割り当てる。
				p_ans = p_cur_chms->allocate_mem_slot( req_size, req_align );
				if ( p_ans != nullptr ) {
					// 空きスロットが見つかれば終了
					hint_params.tls_p_hint_chunk = p_cur_chms;
					return p_ans;
				}
			}

			// リンクリストをたどって、次に検索するchunkを取り出す。
			// 検索を開始したchunkに到着したら、空きスロットが見つからなかったことを示すので、検索を終了する。
			chunk_header_multi_slot* p_next_chms = p_cur_chms->p_next_chunk_.load( std::memory_order_acquire );
			if ( p_next_chms == nullptr ) {
				p_next_chms = p_top_chunk_.load( std::memory_order_acquire );
			}
			if ( p_next_chms == p_start_chms ) {
				p_cur_chms = nullptr;
				break;
			}
			p_cur_chms = p_next_chms;
		}

		// 自スレッドの持ち物で、削除予約されたchunkからの再使用を試みる。
		// ただし、すでに削除されてしまっているかもしれない。
		// なお、自スレッドの持ち物だったとしても、処理を行っている最中に、削除ー＞別スレッドに再割り当てー＞削除予約状態となっている可能性もある。
		// たとえ別スレッドの物になっていても、新たにメモリを割り当てるよりは性能観点でましな可能性が高いため、そのまま割り当てを試みる。
		for ( auto&& e : p_top_chunk_ ) {
			p_ans = e.try_allocate_mem_slot_from_reserved_deletion( hint_params.tl_id_, req_size, req_align );
			if ( p_ans != nullptr ) {
				// スロット確保成功
				hint_params.tls_p_hint_chunk = &e;   // 別スレッドのものかもしれないが、hintを更新する。次の呼び出しで、是正される。
				return p_ans;
			}
		}
	}

	// オーナーなしchunk_header_multi_slotからの割り当てを試みる
	for ( auto&& e : p_top_chunk_ ) {
		p_ans = e.try_get_ownership_allocate_mem_slot( hint_params.tl_id_, req_size, req_align );
		if ( p_ans != nullptr ) {
			// オーナー権とスロット確保成功
			hint_params.tls_p_hint_chunk = &e;   // 別スレッドのものかもしれないが、hintを更新する。次の呼び出しで、是正される。
			return p_ans;
		}
	}

	// 以降は、新しいメモリ領域をmallocで確保することになる。
	// 確保するスロットサイズを決める。既存のスロット数では足りなかったので、スロット数を2倍化する。
	// unsigned int cur_slot_num = hint_params.num_of_pieces_;
	size_t cur_slot_num = get_cur_max_slot_size( hint_params.tl_id_ );
	size_t new_slot_num = cur_slot_num * 2;
	if ( cur_slot_num == 0 ) {
		new_slot_num = chunk_param_.num_of_pieces_;
	}
	if ( ( (size_t)cur_slot_num * (size_t)chunk_param_.size_of_one_piece_ ) >= MAX_ALLOC_SIZE_LIMIT ) {
		// オーバーフローしてしまったら、2倍化を取りやめる。
		new_slot_num = cur_slot_num;
	}
	param_chunk_allocation cur_alloc_conf( chunk_param_.size_of_one_piece_, new_slot_num );

	// 空きchunk_header_multi_slotに対して、再利用を試みる。
	// リンクリストをたどって、次に検索するchunkを取り出す。
	// 検索を開始したchunkに到着したら、空きスロットが見つからなかったことを示すので、検索を終了する。
	for ( auto&& e : p_top_chunk_ ) {
		if ( e.status_.load( std::memory_order_acquire ) == chunk_control_status::EMPTY ) {
			// 空きchunk_header_multi_slotに対して、メモリ割り当てを試みる。ここでロックが発生する可能性がある。
			if ( e.alloc_new_chunk( cur_alloc_conf, hint_params.tl_id_ ) ) {
				// 再割り当て成功
				// 既存のオーナー権を持つチャンクを解放
				release_all_of_ownership( hint_params.tl_id_, &e );
				// メモリスロットを確保する。
				p_ans = e.allocate_mem_slot( req_size, req_align );
				if ( p_ans != nullptr ) {
					hint_params.tls_p_hint_chunk = &e;

					// chunkの登録が終わったので、スロット数の2倍化された値を登録する。
					hint_params.num_of_pieces_ = new_slot_num;

					// 2倍化chunkが登録されたので、それまでのchunkは削除候補に変更にする。
					mark_as_reserved_deletion( hint_params.tl_id_, &e );

					return p_ans;
				}
			} else {
				// 再割り当て失敗
				if ( e.status_.load( std::memory_order_acquire ) == chunk_control_status::NORMAL ) {
					// 自身による再割り当てに失敗はしたが、既に別のスレッドで再割り当て完了済みだった。
					// たとえ別スレッドの物になっていても、新たにメモリを割り当てるよりは性能観点でましな可能性が高いため、そのまま割り当てを試みる。
					p_ans = e.allocate_mem_slot( req_size, req_align );
					if ( p_ans != nullptr ) {
						hint_params.tls_p_hint_chunk = &e;
						return p_ans;
					}
				}
			}
		}
	}

	// 既存のchunkの再利用に失敗したので、新しいchunkを確保する。
	chunk_header_multi_slot* p_new_chms = new ( *p_allocator_ ) chunk_header_multi_slot( cur_alloc_conf, hint_params.tl_id_, &statistics_ );
	if ( p_new_chms == nullptr ) {
		return nullptr;
	}
	p_ans = p_new_chms->allocate_mem_slot( req_size, req_align );
	if ( p_ans == nullptr ) {
		delete p_new_chms;
		return nullptr;
	}

	// 既存のオーナー権を持つチャンクを解放。
	release_all_of_ownership( hint_params.tl_id_, nullptr );   // まだ新しいchunkをリストに登録していないので、nullptrを指定する。

	// 新たに確保したchunkを、chunkリストの先頭に追加する。
	p_top_chunk_.push( p_new_chms );

	hint_params.tls_p_hint_chunk = p_new_chms;

	// スロット数の2倍化されたchunkの登録が終わったので、スロット数の2倍化された値を登録する。
	hint_params.num_of_pieces_ = new_slot_num;

	// 2倍化chunkが登録されたので、それまでのchunkは削除候補に変更にする。
	mark_as_reserved_deletion( hint_params.tl_id_, p_new_chms );

	return p_ans;
}

bool chunk_list::recycle_mem_slot(
	void* p_recycle_slot   //!< [in] pointer to the memory slot to recycle.
)
{
	for ( auto&& e : p_top_chunk_ ) {
		bool ret = e.recycle_mem_slot( p_recycle_slot );
		if ( ret ) return true;
	}

#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	statistics_.dealloc_req_err_cnt_++;
#endif
	return false;
}

void chunk_list::prune( void )
{
	for ( auto&& e : p_top_chunk_ ) {
		e.exec_deletion();
	}

	return;
}

/*!
 * @brief	get statistics
 */
chunk_statistics chunk_list::get_statistics( void ) const
{
	chunk_statistics ans = statistics_.get_statistics();

	ans.alloc_conf_ = chunk_param_;

	return ans;
}

void* general_mem_allocator_impl_allocate(
	unsigned int          pr_ch_size_arg,         //!< array size of chunk and param array
	internal::chunk_list* p_param_ch_array_arg,   //!< unique pointer to chunk and param array
	size_t                n_arg,                  //!< [in] memory size to allocate
	size_t                req_align               //!< [in] requested align size
)
{
	void* p_ans = nullptr;
	if ( pr_ch_size_arg > 0 ) {
		// binary search
		unsigned int si = 0;
		unsigned int ei = pr_ch_size_arg - 1;
		unsigned int mi = si + ( ei - si ) / 2;
		while ( si != ei ) {
			if ( ( n_arg + req_align ) <= ( p_param_ch_array_arg[mi].chunk_param_.size_of_one_piece_ + default_slot_alignsize ) ) {
				ei = mi;
			} else {
				si = mi + 1;
			}
			mi = si + ( ei - si ) / 2;
		}
		if ( ( n_arg + req_align ) <= ( p_param_ch_array_arg[si].chunk_param_.size_of_one_piece_ + default_slot_alignsize ) ) {
			p_ans = p_param_ch_array_arg[si].allocate_mem_slot( n_arg, req_align );
			if ( p_ans != nullptr ) {
				return p_ans;
			}
		}
	}

	// 対応するサイズのメモリスロットが見つからなかったので、slot_header_of_alloc経由でメモリ領域を確保する。
	size_t          buff_size      = slot_header_of_alloc::calc_slot_header_and_container_size( n_arg, req_align );
	allocate_result alloc_mem_info = basic_mem_allocator::allocate( buff_size, req_align );
	if ( alloc_mem_info.p_allocated_addr_ == nullptr ) {
		internal::LogOutput( log_type::ERR, "fail allocate memory by basic_mem_allocator::allocate(%zu, %zu)", buff_size, req_align );
		return nullptr;
	}
	slot_header_of_alloc* p_slot_header_of_alloc = new ( alloc_mem_info.p_allocated_addr_ ) slot_header_of_alloc( alloc_mem_info.allocated_size_ );
	p_ans                                        = p_slot_header_of_alloc->allocate( n_arg, req_align );

	return p_ans;
}
void general_mem_allocator_impl_deallocate(
	unsigned int          pr_ch_size_arg,         //!< array size of chunk and param array
	internal::chunk_list* p_param_ch_array_arg,   //!< unique pointer to chunk and param array
	void*                 p_mem                   //!< [in] pointer to allocated memory by allocate()
)
{
	if ( p_mem == nullptr ) return;

	internal::slot_chk_result chk_ret = internal::chunk_header_multi_slot::get_chunk( p_mem );

	if ( chk_ret.correct_ ) {
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
		if ( chk_ret.p_chms_ == nullptr ) {
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_EXCEPTION
			std::string errlog = "return value of get_chunk() is unexpected";
			throw std::logic_error( errlog );
#else
			internal::LogOutput( log_type::ERR, "return value of get_chunk() is unexpected" );
			return;
#endif
		}
#endif
		chk_ret.p_chms_->unchk_recycle_mem_slot( chk_ret.p_sam_, chk_ret.p_sha_ );
	} else {
		// slot_header_of_allocで確保されているかどうかをチェックする
		auto check_result = slot_container::get_slot_header_from_assignment_p( p_mem );
		if ( check_result.is_ok_ ) {
			if ( check_result.p_ush_->check_type() ) {
				check_result.p_ush_->alloch_.deallocate();
				basic_mem_allocator::deallocate( check_result.p_ush_, check_result.p_ush_->alloch_.sh_.alloc_size_ );
				return;
			}
		}
		internal::LogOutput( log_type::WARN, "Header is corrupted. full search correct chunk and try free" );
		for ( unsigned int i = 0; i < pr_ch_size_arg; i++ ) {
			bool ret = p_param_ch_array_arg[i].recycle_mem_slot( p_mem );
			if ( ret ) {
				internal::LogOutput( log_type::WARN, "Header is corrupted, but luckily success to find and free" );
				return;
			}
		}

		// header is corrupted and unknown memory slot deallocation is requested. try to call free()
		internal::LogOutput( log_type::WARN, "header is corrupted and unknown memory slot deallocation is requested. try to free by calling free()" );
		std::free( p_mem );
	}

	return;
}
void general_mem_allocator_impl_prune(
	unsigned int          pr_ch_size_arg,         //!< array size of chunk and param array
	internal::chunk_list* p_param_ch_array_arg,   //!< unique pointer to chunk and param array
	std::atomic_bool&     exclusive_ctr_arg       //<! reference to exclusive control for prune
)
{
	bool expected = false;
	bool ret      = exclusive_ctr_arg.compare_exchange_strong( expected, true );

	if ( !ret ) return;   // other thread/s is executing now

	for ( unsigned int ii = 0; ii < pr_ch_size_arg; ii++ ) {
		p_param_ch_array_arg[ii].prune();
	}

	exclusive_ctr_arg.store( false, std::memory_order_release );
	return;
}

}   // namespace internal

////////////////////////////////////////////////////////////////////////////////////////////////////////
general_mem_allocator::general_mem_allocator(
	const param_chunk_allocation* p_param_array,   //!< [in] pointer to parameter array
	unsigned int                  num              //!< [in] array size
	)
  : allocator_impl_()
{
#ifdef ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
#else
	allocator_impl_.setup_by_param( p_param_array, num );
#endif
	return;
}

std::string chunk_statistics::print( void ) const
{
	char buf[CONF_LOGGER_INTERNAL_BUFF_SIZE];
	snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
	          "chunk conf{.size=%d, .num=%d}, chunk_num: %d, valid chunk_num: %d"
	          ", total_slot=%d, free_slot=%d, consum cnt=%d, max consum cnt=%d"
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	          ", alloc cnt=%d, alloc err=%d, dealloc cnt=%d, dealloc err=%d, alloc_colli=%d, dealloc_colli=%d"
#endif
	          ,
	          (int)alloc_conf_.size_of_one_piece_,
	          (int)alloc_conf_.num_of_pieces_,
	          (int)chunk_num_,
	          (int)valid_chunk_num_,
	          (int)total_slot_cnt_,
	          (int)free_slot_cnt_,
	          (int)consum_cnt_,
	          (int)max_consum_cnt_
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	          ,
	          (int)alloc_req_cnt_,
	          (int)error_alloc_req_cnt_,
	          (int)dealloc_req_cnt_,
	          (int)error_dealloc_req_cnt_,
	          (int)alloc_collision_cnt_,
	          (int)dealloc_collision_cnt_
#endif
	);

	return std::string( buf );
}

std::string general_mem_allocator_statistics::print( void ) const
{
	std::string ans;
	for ( auto&& e : ch_st_ ) {
		ans += e.print();
		ans += "\n";
	};
	ans += al_st_.print();
	return ans;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
std::tuple<bool,
           alpha::concurrent::bt_info,
           alpha::concurrent::bt_info>
get_backtrace_info(
	void* p_mem   //!< [in] pointer to allocated memory by allocate()
)
{
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	internal::bool_unified_slot_header_p chk_ret = internal::slot_container::get_slot_header_from_assignment_p( p_mem );
	if ( !chk_ret.is_ok_ ) {
		return std::tuple<bool,
		                  alpha::concurrent::bt_info,
		                  alpha::concurrent::bt_info>(
			false,
			bt_info(),
			bt_info() );
	}
#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
	if ( !chk_ret.p_ush_->mh_.check_marker() ) {
		return std::tuple<bool,
		                  alpha::concurrent::bt_info,
		                  alpha::concurrent::bt_info>(
			false,
			bt_info(),
			bt_info() );
	}
#endif
	return std::tuple<bool,
	                  alpha::concurrent::bt_info,
	                  alpha::concurrent::bt_info>(
		true,
		chk_ret.p_ush_->mh_.alloc_bt_info_,
		chk_ret.p_ush_->mh_.free_bt_info_ );

#else
	return std::tuple<bool,
	                  alpha::concurrent::bt_info,
	                  alpha::concurrent::bt_info>(
		false,
		bt_info(),
		bt_info() );
#endif
}

void output_backtrace_info(
	const log_type lt,     //!< [in] log type
	void*          p_mem   //!< [in] pointer to allocated memory by allocate()
)
{
	static std::atomic_int counter( 0 );
	int                    id_count = counter.fetch_add( 1, std::memory_order_acq_rel );

	internal::bool_unified_slot_header_p chk_ret = internal::slot_container::get_slot_header_from_assignment_p( p_mem );
	internal::LogOutput(
		lt,
		"[%d] header check result of %p: correct_=%s, p_ush_=%p",
		id_count, p_mem, chk_ret.is_ok_ ? "true" : "false", chk_ret.p_ush_ );

#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	internal::LogOutput( lt, "[%d-a] alloc_bt_info_ of %p", id_count, p_mem );
	chk_ret.p_ush_->mh_.alloc_bt_info_.dump_to_log( lt, 'a', id_count );
	internal::LogOutput( lt, "[%d-f] free_bt_info_ of %p", id_count, p_mem );
	chk_ret.p_ush_->mh_.free_bt_info_.dump_to_log( lt, 'f', id_count );
#else
	internal::LogOutput( lt, "[%d] no backtrace information, because the library is not compiled with ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE", id_count );
#endif

	return;
}

}   // namespace concurrent
}   // namespace alpha
