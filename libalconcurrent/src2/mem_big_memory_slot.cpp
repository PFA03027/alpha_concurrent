/**
 * @file mem_big_memory_slot.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-22
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "mem_big_memory_slot.hpp"
#include "mem_common.hpp"
#include "mmap_allocator.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

size_t big_memory_slot_list::limit_bytes_of_unused_retrieved_memory_    = big_memory_slot_list::defualt_limit_bytes_of_unused_retrieved_memory_;
size_t big_memory_slot_list::too_big_memory_slot_buffer_size_threshold_ = big_memory_slot_list::defualt_limit_bytes_of_unused_retrieved_memory_;

big_memory_slot* big_memory_slot::check_validity_to_ownwer_and_get( void ) const noexcept
{
	big_memory_slot* p_slot_owner = link_to_big_memory_slot_.load_addr<big_memory_slot>();
	if ( p_slot_owner == nullptr ) {
		return nullptr;
	}
	if ( p_slot_owner->magic_number_ != big_memory_slot::magic_number_value_ ) {
		return nullptr;
	}
	return p_slot_owner;
}

big_memory_slot* big_memory_slot_list::reuse_allocate( size_t requested_allocatable_size ) noexcept
{
	big_memory_slot* p_tmp_head = nullptr;
	big_memory_slot* p_ans      = nullptr;
	while ( true ) {
		p_ans = unused_retrieved_slots_mgr_.request_reuse();
		if ( p_ans == nullptr ) {
			// 候補スロットがなくなったので、終了する。
			break;
		}

		if ( requested_allocatable_size <= p_ans->max_allocatable_size() ) {
			// 割り当て可能なスロットが見つかったので、終了する
			break;
		}

		p_ans->p_temprary_link_next_ = p_tmp_head;
		p_tmp_head                   = p_ans;
	}

	// 一時的にキープしていたスロットを未使用スロットリストに戻す。
	while ( p_tmp_head != nullptr ) {
		big_memory_slot* p_tmp = p_tmp_head;
		p_tmp_head             = p_tmp_head->p_temprary_link_next_;
		unused_retrieved_slots_mgr_.retrieve( p_tmp );
	}

	if ( p_ans != nullptr ) {
		unused_retrieved_memory_bytes_.fetch_sub( p_ans->buffer_size_, std::memory_order_release );
		bool old_is_used = p_ans->link_to_big_memory_slot_.fetch_set( true );
		if ( old_is_used ) {
			LogOutput( log_type::ERR, "big_memory_slot_list::reuse_allocate() detected unexpected is_used flag" );
		}
	}

	return p_ans;
}

void big_memory_slot_list::deallocate( big_memory_slot* p ) noexcept
{
	if ( p == nullptr ) {
		LogOutput( log_type::WARN, "big_memory_slot_list::deallocate() is called with nullptr" );
		return;
	}
	auto p_slot_owner = p->check_validity_to_ownwer_and_get();
	if ( p_slot_owner == nullptr ) {
		LogOutput( log_type::WARN, "big_memory_slot_list::deallocate() is called with invalid big_memory_slot" );
		return;
	}

	auto slot_info = p->link_to_big_memory_slot_.load_allocation_info<big_memory_slot>();
	if ( slot_info.is_used_ == false ) {
		LogOutput( log_type::WARN, "big_memory_slot_list::deallocate() is called with unused slot. this means double-free." );
		return;
	}
	if ( !p->link_to_big_memory_slot_.compare_and_exchange_used_flag( slot_info.is_used_, false ) ) {
		LogOutput( log_type::WARN, "big_memory_slot_list::deallocate() fail to change slot status as unused slot. this means double-free causes race-condition b/w threads." );
		return;
	}

	if ( slot_info.mt_ == mem_type::BIG_MEM ) {
		if ( ( unused_retrieved_memory_bytes_.load( std::memory_order_acquire ) + p->buffer_size_ ) > limit_bytes_of_unused_retrieved_memory_ ) {
			deallocate_by_munmap( p, p->buffer_size_ );
		} else {
			unused_retrieved_memory_bytes_.fetch_add( p->buffer_size_, std::memory_order_release );
			unused_retrieved_slots_mgr_.retrieve( p );
		}
	} else if ( slot_info.mt_ == mem_type::OVER_BIG_MEM ) {
		deallocate_by_munmap( p, p->buffer_size_ );
	} else {
		LogOutput( log_type::WARN, "big_memory_slot_list::deallocate() is called with unknown mem_type %u", static_cast<unsigned int>( slot_info.mt_ ) );
	}
	return;
}

big_memory_slot* big_memory_slot_list::allocate_newly( size_t requested_allocatable_size ) noexcept
{
	auto buffer_ret = allocate_by_mmap( big_memory_slot::calc_minimum_buffer_size( requested_allocatable_size ), allocated_mem_top::min_alignment_size_ );
	if ( buffer_ret.p_allocated_addr_ == nullptr ) {
		return nullptr;
	}

	return big_memory_slot::emplace_on_mem( buffer_ret.p_allocated_addr_,
	                                        ( buffer_ret.allocated_size_ < too_big_memory_slot_buffer_size_threshold_ ) ? mem_type::BIG_MEM : mem_type::OVER_BIG_MEM,
	                                        buffer_ret.allocated_size_ );
}

void big_memory_slot_list::clear_for_test( void ) noexcept
{
	big_memory_slot* p_ans = nullptr;
	p_ans                  = unused_retrieved_slots_mgr_.request_reuse();
	while ( p_ans != nullptr ) {
		deallocate_by_munmap( p_ans, p_ans->buffer_size_ );
		p_ans = unused_retrieved_slots_mgr_.request_reuse();
	}
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
