/**
 * @file mem_small_memory_slot.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-22
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "mem_small_memory_slot.hpp"
#include "alloc_only_allocator.hpp"
#include "mem_allocated_mem_top.hpp"
#include "mmap_allocator.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

////////////////////////////////////////////////////////////////////////////////////////////////////////
// configuration value
constexpr size_t          conf_pre_mmap_size = 1024 * 1024;
static alloc_only_chamber gmem_alloc_only_inst( false, conf_pre_mmap_size );   // グローバルインスタンスは、プロセス終了までメモリ領域を維持するために、デストラクタが呼ばれてもmmapした領域を解放しない。

////////////////////////////////////////////////////////////////////////////////////////////////////////
memory_slot_group* slot_link_info::check_validity_to_owner_and_get( void ) noexcept
{
	memory_slot_group* p_slot_owner = link_to_memory_slot_group_.load_addr<memory_slot_group>();
	if ( p_slot_owner == nullptr ) {
		return nullptr;
	}
	if ( p_slot_owner->magic_number_ != memory_slot_group::magic_number_value_ ) {
		return nullptr;
	}
	return p_slot_owner;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
memory_slot_group_statistics memory_slot_group::get_statistics( void ) const noexcept
{
	size_t total_slots  = 0;
	size_t in_use_slots = 0;
	size_t free_slots   = 0;

	for ( size_t i = 0; i < num_slots_; i++ ) {
		const slot_link_info* p_i_sli   = get_slot_pointer( i );
		const slot_link_info* p_end_sli = reinterpret_cast<const slot_link_info*>( ap_unassigned_slot_.load() );
		if ( p_end_sli <= p_i_sli ) break;

		total_slots++;
		unziped_allocation_info<void> alloc_info = p_i_sli->link_to_memory_slot_group_.load_allocation_info<void>();
		if ( alloc_info.is_used_ ) {
			in_use_slots++;
		} else {
			free_slots++;
		}
	}

	return { total_slots, in_use_slots, free_slots };
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
slot_link_info* memory_slot_group_list::allocate_impl( void ) noexcept
{
	// 回収済み、再割り当て待ちリストからスロットの取得を試みる
	slot_link_info* p_ans = retrieved_small_slots_array_mgr::request_reuse( retrieved_array_idx_ );
	if ( p_ans != nullptr ) {
		bool old_is_used = p_ans->link_to_memory_slot_group_.fetch_set( true );
		if ( old_is_used ) {
			LogOutput( log_type::ERR, "big_memory_slot_list::reuse_allocate() detected unexpected is_used flag" );
		}
		return p_ans;   // 取得できたので、そのまま返す
	}

	// 未使用スロットリストからスロットの取得を試みる
	memory_slot_group* p_cur_memory_slot_group_target = ap_cur_assigning_memory_slot_group_.load( std::memory_order_acquire );
	if ( p_cur_memory_slot_group_target == nullptr ) {
		return nullptr;   // 割り当て可能なmemory_slot_groupがないので、nullptrを返す
	}

	// 割り当て可能なmemory_slot_groupがある場合、スロットの取得を試みる
	while ( true ) {
		// 未使用スロットの有無を確認
		if ( p_cur_memory_slot_group_target->is_assigned_all_slots() ) {

			// 次のmemory_slot_groupが検索可能かを確認する
			memory_slot_group* p_new_memory_slot_group_target = p_cur_memory_slot_group_target->ap_next_group_.load( std::memory_order_acquire );
			if ( p_new_memory_slot_group_target == nullptr ) {
				// 次のmemory_slot_groupがない場合、先頭のmemory_slot_groupに戻って検索を続ける
				p_new_memory_slot_group_target = ap_head_memory_slot_group_.load( std::memory_order_acquire );
			}

			// 次のmemory_slot_groupがある場合、次のmemory_slot_groupを検索対象に設定する
			if ( !ap_cur_assigning_memory_slot_group_.compare_exchange_strong( p_cur_memory_slot_group_target, p_new_memory_slot_group_target, std::memory_order_acq_rel ) ) {
				// 他のスレッドが先に次のmemory_slot_groupを検索対象に設定した場合、再度検索を最初からやり直す。
				continue;
			}

			// 次のmemory_slot_groupを検索対象に設定できたので、次のmemory_slot_groupの状態を確認する。
			if ( p_new_memory_slot_group_target->is_assigned_all_slots() ) {
				// 次のmemory_slot_groupが全てのスロットを割り当て済みの場合、以降のmemory_slot_groupも全て割り当て済み。
				if ( p_new_memory_slot_group_target == ap_head_memory_slot_group_.load( std::memory_order_acquire ) ) {
					// 先頭のmemory_slot_groupが全てのスロットを割り当て済みの場合、空きスロットはないので、検索を終了する
					break;
				}
				// 他のスレッドによる更新がなされているかもしれないので、検索を最初からやり直す。
				p_cur_memory_slot_group_target = ap_head_memory_slot_group_.load( std::memory_order_acquire );
				continue;
			}
		}
		// ここに到達した場合、割り当て可能なmemory_slot_groupが見つかった。
		// よって、スロットの取得を試みる
		p_ans = p_cur_memory_slot_group_target->assign_new_slot();
		if ( p_ans != nullptr ) {
			// bool old_is_used = p_ans->link_to_memory_slot_group_.fetch_set( true );
			// if ( old_is_used ) {
			// 	LogOutput( log_type::ERR, "big_memory_slot_list::reuse_allocate() detected unexpected is_used flag" );
			// }
			return p_ans;
		}
	}

	return nullptr;
}

bool memory_slot_group_list::deallocate( slot_link_info* p ) noexcept
{
	if ( p == nullptr ) {
		LogOutput( log_type::DEBUG, "memory_slot_group_list::deallocate() with nullptr" );
		return false;
	}
	auto p_slot_owner = p->check_validity_to_owner_and_get();
	if ( p_slot_owner == nullptr ) {
		LogOutput( log_type::WARN, "memory_slot_group_list::deallocate() invalid slot_link_info" );
		bt_info::record_backtrace().dump_to_log( log_type::WARN, 'i', 2 );
		return false;
	}

	auto slot_info = p->link_to_memory_slot_group_.load_allocation_info<memory_slot_group>();
	if ( slot_info.mt_ != mem_type::SMALL_MEM ) {
		LogOutput( log_type::WARN, "memory_slot_group_list::deallocate() is called with unknown mem_type %u", static_cast<unsigned int>( slot_info.mt_ ) );
		bt_info::record_backtrace().dump_to_log( log_type::WARN, 'u', 2 );
		return false;
	}

	if ( slot_info.is_used_ == false ) {
		LogOutput( log_type::WARN, "memory_slot_group_list::deallocate() is called with unused slot. this means double-free." );
		bt_info::record_backtrace().dump_to_log( log_type::WARN, 'd', 7 );
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
		btinfo_alloc_free& cur_btinfo = p_slot_owner->get_btinfo( p_slot_owner->get_slot_idx( p ) );
		LogOutput( log_type::WARN, "Allocated by below;" );
		cur_btinfo.alloc_trace_.dump_to_log( log_type::WARN, 'd', 8 );
		LogOutput( log_type::WARN, "Free by below;" );
		cur_btinfo.free_trace_.dump_to_log( log_type::WARN, 'd', 9 );
#endif
		return false;
	}
	if ( !p->link_to_memory_slot_group_.compare_and_exchange_used_flag( slot_info.is_used_, false ) ) {
		LogOutput( log_type::WARN, "memory_slot_group_list::deallocate() fail to change slot status as unused slot. this means double-free causes race-condition b/w threads." );
		bt_info::record_backtrace().dump_to_log( log_type::WARN, 'd', 10 );
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
		btinfo_alloc_free& cur_btinfo = p_slot_owner->get_btinfo( p_slot_owner->get_slot_idx( p ) );
		LogOutput( log_type::WARN, "Allocated by below;" );
		cur_btinfo.alloc_trace_.dump_to_log( log_type::WARN, 'd', 11 );
		LogOutput( log_type::WARN, "Free by below;" );
		cur_btinfo.free_trace_.dump_to_log( log_type::WARN, 'd', 12 );
#endif
		return false;
	}

#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	btinfo_alloc_free& cur_btinfo = p_slot_owner->get_btinfo( p_slot_owner->get_slot_idx( p ) );
	cur_btinfo.free_trace_        = bt_info::record_backtrace();
#endif
	retrieved_small_slots_array_mgr::retrieve( retrieved_array_idx_, p );
	return true;
}

void memory_slot_group_list::request_allocate_memory_slot_group( void ) noexcept
{
	size_t cur_allocating_buffer_bytes = next_allocating_buffer_bytes_.load( std::memory_order_acquire );
	auto   p_buffer_ret                = gmem_alloc_only_inst.allocate( cur_allocating_buffer_bytes, allocated_mem_top::min_alignment_size_ );
	if ( p_buffer_ret == nullptr ) {
		return;
	}
	memory_slot_group* p_new_group = memory_slot_group::emplace_on_mem( p_buffer_ret, this, cur_allocating_buffer_bytes, allocatable_bytes_ );
	memory_slot_group* p_cur_head  = ap_head_memory_slot_group_.load( std::memory_order_acquire );
	do {
		p_new_group->ap_next_group_ = p_cur_head;
	} while ( !ap_head_memory_slot_group_.compare_exchange_strong( p_cur_head, p_new_group, std::memory_order_acq_rel ) );

	size_t new_alocating_buffer_bytes = clac_next_expected_buffer_size( cur_allocating_buffer_bytes, limit_bytes_for_one_memory_slot_group_ );
	next_allocating_buffer_bytes_.compare_exchange_strong( cur_allocating_buffer_bytes, new_alocating_buffer_bytes, std::memory_order_acq_rel );

	memory_slot_group* p_cur_memory_slot_group_target = ap_cur_assigning_memory_slot_group_.load( std::memory_order_acquire );
	if ( p_cur_memory_slot_group_target == nullptr ) {
		ap_cur_assigning_memory_slot_group_.compare_exchange_strong( p_cur_memory_slot_group_target, p_new_group, std::memory_order_acq_rel );
	}
}

void memory_slot_group_list::clear_for_test( void ) noexcept
{
	retrieved_small_slots_array_mgr::reset_for_test();
	memory_slot_group* p_cur = ap_head_memory_slot_group_.load( std::memory_order_acquire );
	while ( p_cur != nullptr ) {
		memory_slot_group* p_next = p_cur->ap_next_group_;
		gmem_alloc_only_inst.deallocate( p_cur );
		p_cur = p_next;
	}
	ap_head_memory_slot_group_.store( nullptr, std::memory_order_release );
	ap_cur_assigning_memory_slot_group_.store( nullptr, std::memory_order_release );
}

void memory_slot_group_list::dump_status( log_type lt, char c, int id ) noexcept
{
	LogOutput( lt,
	           "[%c-%d] idx=%zu, allocatable_bytes_=%zu, limit_bytes_for_one_memory_slot_group_=%zu, next_allocating_buffer_bytes_=%zu, ap_head_memory_slot_group_=%p",
	           c, id, retrieved_array_idx_,
	           allocatable_bytes_,
	           limit_bytes_for_one_memory_slot_group_,
	           next_allocating_buffer_bytes_.load(),
	           ap_head_memory_slot_group_.load() );

	size_t             memory_slot_group_count = 0;
	size_t             total_slots             = 0;
	size_t             in_use_slots            = 0;
	size_t             free_slots              = 0;
	memory_slot_group* p_cur_msg               = ap_head_memory_slot_group_.load();
	while ( p_cur_msg != nullptr ) {
		memory_slot_group_count++;
		memory_slot_group_statistics ret = p_cur_msg->get_statistics();
		total_slots += ret.total_slots_;
		in_use_slots += ret.in_use_slots_;
		free_slots += ret.free_slots_;

		p_cur_msg = p_cur_msg->ap_next_group_.load();
	}

	LogOutput( lt,
	           "[%c-%d] idx=%zu, memory_slot_group_count=%zu, total_slots=%zu, in_use_slots=%zu, free_slots=%zu",
	           c, id, retrieved_array_idx_,
	           memory_slot_group_count,
	           total_slots,
	           in_use_slots,
	           free_slots );
}

void memory_slot_group_list::dump_log( log_type lt, char c, int id ) noexcept
{
	gmem_alloc_only_inst.dump_to_log( lt, c, id );
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
