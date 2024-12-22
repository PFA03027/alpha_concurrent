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
#include "mem_common.hpp"
#include "mmap_allocator.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

memory_slot_group* slot_link_info::check_validity_to_ownwer_and_get( void ) noexcept
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

slot_link_info* memory_slot_group_list::allocate( void ) noexcept
{
	// 回収済み、再割り当て待ちリストからスロットの取得を試みる
	slot_link_info* p_ans = unused_retrieved_slots_mgr_.request_reuse();
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
		LogOutput( log_type::WARN, "memory_slot_group_list::deallocate() nullptr" );
		return false;
	}
	auto p_slot_owner = p->check_validity_to_ownwer_and_get();
	if ( p_slot_owner == nullptr ) {
		LogOutput( log_type::WARN, "memory_slot_group_list::deallocate() invalid slot_link_info" );
		return false;
	}

	auto slot_info = p->link_to_memory_slot_group_.load_allocation_info<memory_slot_group>();
	if ( slot_info.mt_ != mem_type::SMALL_MEM ) {
		LogOutput( log_type::WARN, "memory_slot_group_list::deallocate() is called with unknown mem_type %u", static_cast<unsigned int>( slot_info.mt_ ) );
		return false;
	}

	if ( slot_info.is_used_ == false ) {
		LogOutput( log_type::WARN, "memory_slot_group_list::deallocate() is called with unused slot. this means double-free." );
		return false;
	}
	if ( !p->link_to_memory_slot_group_.compare_and_exchange_used_flag( slot_info.is_used_, false ) ) {
		LogOutput( log_type::WARN, "memory_slot_group_list::deallocate() fail to change slot status as unused slot. this means double-free causes race-condition b/w threads." );
		return false;
	}

	unused_retrieved_slots_mgr_.retrieve( p );
	return true;
}

void memory_slot_group_list::request_allocate_memory_slot_group( void ) noexcept
{
	size_t cur_allocating_buffer_bytes = next_allocating_buffer_bytes_.load( std::memory_order_acquire );
	auto   buffer_ret                  = allocate_by_mmap( cur_allocating_buffer_bytes, allocated_mem_top::min_alignment_size_ );
	if ( buffer_ret.p_allocated_addr_ == nullptr ) {
		return;
	}
	memory_slot_group* p_new_group = memory_slot_group::emplace_on_mem( buffer_ret.p_allocated_addr_, this, buffer_ret.allocated_size_, allocatable_bytes_ );
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
	memory_slot_group* p_cur = ap_head_memory_slot_group_.load( std::memory_order_acquire );
	while ( p_cur != nullptr ) {
		memory_slot_group* p_next = p_cur->ap_next_group_;
		deallocate_by_munmap( p_cur, p_cur->buffer_size_ );
		p_cur = p_next;
	}
	ap_head_memory_slot_group_.store( nullptr, std::memory_order_release );
	ap_cur_assigning_memory_slot_group_.store( nullptr, std::memory_order_release );
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
