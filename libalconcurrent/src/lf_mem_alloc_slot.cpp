/**
 * @file lf_mem_alloc_slot.cpp
 * @author Teruaki Ata <PFA03027@nifty.com>
 * @brief Implement of slot structure I/F
 * @version 0.1
 * @date 2023-08-06
 *
 * @copyright Copyright (c) 2023 Teruaki Ata <PFA03027@nifty.com>
 *
 */

#include "alconcurrent/internal/alloc_only_allocator.hpp"

#include "lf_mem_alloc_slot.hpp"
#include "utility.hpp"

namespace alpha {
namespace concurrent {

namespace internal {
////////////////////////////////////////////////////////////////////////////////////////////////////////

void* slot_header_of_array::allocate( slot_container* p_container_top, size_t container_size, size_t n, size_t req_align )
{
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	RECORD_BACKTRACE_GET_BACKTRACE( mh_.alloc_bt_info_ );
	RECORD_BACKTRACE_INVALIDATE_BACKTRACE( mh_.free_bt_info_ );
#endif

	void* p_ans = slot_container::construct_slot_container_in_container_buffer( &mh_, p_container_top, container_size, n, req_align );
	return p_ans;
}

void slot_header_of_array::deallocate( void )
{
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	bool xx_free_flag = false;
	if ( mh_.free_bt_info_.count_ > 0 ) {
		internal::LogOutput( log_type::ERR, "double free is detected" );
		internal::LogOutput( log_type::ERR, "previous deallocation by below call stack" );
		mh_.free_bt_info_.dump_to_log( log_type::ERR, 'f', 1 );
		xx_free_flag = true;
	}
	RECORD_BACKTRACE_GET_BACKTRACE( mh_.free_bt_info_ );
	if ( xx_free_flag ) {
		internal::LogOutput( log_type::ERR, "second deallocation by below call stack" );
		mh_.free_bt_info_.dump_to_log( log_type::ERR, 'f', 2 );
		throw std::runtime_error( "double free is detected." );
	}
#endif
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_OVERRUN_WRITING
	unsigned char* p_tail_padding = reinterpret_cast<unsigned char*>( reinterpret_cast<uintptr_t>( &mh_ ) + mh_.offset_to_tail_padding_ );
	if ( ( *p_tail_padding ) != tail_padding_byte_v ) {
		internal::LogOutput( log_type::ERR, "write overrun is detected. tail padding address %p", p_tail_padding );
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
		internal::LogOutput( log_type::ERR, "this area is allocated by below call stack" );
		mh_.alloc_bt_info_.dump_to_log( log_type::ERR, 'a', 1 );
#endif
		bt_info cur_bt;
		RECORD_BACKTRACE_GET_BACKTRACE( cur_bt );
		internal::LogOutput( log_type::ERR, "now deallocation by below call stack" );
		cur_bt.dump_to_log( log_type::ERR, 'f', 1 );
		throw std::runtime_error( "detect overrun writing error" );
	}
#endif

	mh_.offset_to_tail_padding_.store( 0, std::memory_order_release );   // mark as not used

	return;
}

void* slot_header_of_alloc::allocate( size_t n, size_t req_align )
{
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	RECORD_BACKTRACE_GET_BACKTRACE( mh_.alloc_bt_info_ );
	RECORD_BACKTRACE_INVALIDATE_BACKTRACE( mh_.free_bt_info_ );
#endif

	void* p_ans = slot_container::construct_slot_container_in_container_buffer(
		&mh_,
		reinterpret_cast<slot_container*>( slot_container_buffer_ ),
		sh_.alloc_size_ - sizeof( slot_header_of_alloc ),
		n,
		req_align );
	return p_ans;
}

void slot_header_of_alloc::deallocate( void )
{
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	bool xx_free_flag = false;
	if ( mh_.free_bt_info_.count_ > 0 ) {
		internal::LogOutput( log_type::ERR, "double free is detected" );
		internal::LogOutput( log_type::ERR, "previous deallocation by below call stack" );
		mh_.free_bt_info_.dump_to_log( log_type::ERR, 'f', 1 );
		xx_free_flag = true;
	}
	RECORD_BACKTRACE_GET_BACKTRACE( mh_.free_bt_info_ );
	if ( xx_free_flag ) {
		internal::LogOutput( log_type::ERR, "second deallocation by below call stack" );
		mh_.free_bt_info_.dump_to_log( log_type::ERR, 'f', 2 );
		throw std::runtime_error( "double free is detected." );
	}
#endif
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_OVERRUN_WRITING
	unsigned char* p_tail_padding = reinterpret_cast<unsigned char*>( reinterpret_cast<uintptr_t>( &mh_ ) + mh_.offset_to_tail_padding_ );
	if ( ( *p_tail_padding ) != tail_padding_byte_v ) {
		internal::LogOutput( log_type::ERR, "write overrun is detected. tail padding address %p", p_tail_padding );
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
		internal::LogOutput( log_type::ERR, "this area is allocated by below call stack" );
		mh_.alloc_bt_info_.dump_to_log( log_type::ERR, 'a', 1 );
#endif
		bt_info cur_bt;
		RECORD_BACKTRACE_GET_BACKTRACE( cur_bt );
		internal::LogOutput( log_type::ERR, "now deallocation by below call stack" );
		cur_bt.dump_to_log( log_type::ERR, 'f', 1 );
		throw std::runtime_error( "detect overrun writing error" );
	}
#endif

	mh_.offset_to_tail_padding_.store( 0, std::memory_order_release );   // mark as not used

	return;
}

bool_unified_slot_header_p slot_container::get_slot_header_from_assignment_p( void* p_mem )
{
	slot_container* p_slot_container = reinterpret_cast<slot_container*>( reinterpret_cast<uintptr_t>( p_mem ) - static_cast<uintptr_t>( sizeof( slot_container ) ) );
#if defined( ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR ) || defined( ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION )
	if ( p_mem != reinterpret_cast<void*>( p_slot_container->mem ) ) {
		internal::LogOutput( log_type::ERR, "does not match p_mem and slot_container::mem[0]. This is logical error." );
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION
		std::terminate();
#else
		return { false, nullptr };
#endif
	}
#endif
#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
	if ( !p_slot_container->check_marker() ) {
		internal::LogOutput( log_type::ERR, "slot_container(%p) is corrupted", p_slot_container );
		return { false, nullptr };
	}
#endif

	uintptr_t addr_back_offsetX = reinterpret_cast<uintptr_t>( &( p_slot_container->back_offset_ ) );
	uintptr_t addr_ush          = addr_back_offsetX + p_slot_container->back_offset_.load( std::memory_order_acquire );

	return bool_unified_slot_header_p { true, reinterpret_cast<unified_slot_header*>( addr_ush ) };
}

void* slot_container::construct_slot_container_in_container_buffer( slot_mheader* p_bind_mh_of_slot, slot_container* p_container_top, size_t container_size, size_t n, size_t req_align )
{
#if defined( ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR ) || defined( ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION )
	if ( !is_power_of_2( req_align ) ) {
		internal::LogOutput( log_type::ERR, "req_align should be power of 2. but, req_align is %zu, 0x%zX", req_align, req_align );
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION
		std::terminate();
#endif
	}
#endif
	uintptr_t min_base_addr      = reinterpret_cast<uintptr_t>( p_container_top ) + static_cast<uintptr_t>( sizeof( slot_container ) );
	uintptr_t tfit_req_alignsize = ( req_align > sizeof( uintptr_t ) ) ? static_cast<uintptr_t>( req_align ) : static_cast<uintptr_t>( sizeof( uintptr_t ) );
	uintptr_t mx                 = min_base_addr / tfit_req_alignsize;   // TODO: ビットマスクを使った演算で多分軽量化できるが、まずは真面目に計算する。
#ifdef ALCONCURRENT_CONF_ENABLE_MODULO_OPERATION_BY_BITMASK
	uintptr_t rx = min_base_addr & ( tfit_req_alignsize - 1 );   // 剰余計算をビットマスク演算に変更。この時点で、tfit_req_alignsizeが2のn乗でなければならない。
#else
	uintptr_t rx = min_base_addr % tfit_req_alignsize;
#endif
	uintptr_t ans_addr           = tfit_req_alignsize * mx + ( ( rx == 0 ) ? 0 : tfit_req_alignsize );
	uintptr_t addr_end_of_alloc  = reinterpret_cast<uintptr_t>( p_container_top ) + static_cast<uintptr_t>( container_size );
	uintptr_t addr_end_of_assign = ans_addr + static_cast<uintptr_t>( n );
	if ( addr_end_of_assign >= addr_end_of_alloc ) {
		// tail_paddingのサイズ分を考慮するために >= で比較している。
		return nullptr;
	}
	// size_t ans_tail_padding_size = addr_end_of_alloc - addr_end_of_assign;

	slot_container* p_slot_container = reinterpret_cast<slot_container*>( ans_addr - static_cast<uintptr_t>( sizeof( slot_container ) ) );
#if defined( ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR ) || defined( ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION )
	if ( reinterpret_cast<void*>( ans_addr ) != reinterpret_cast<void*>( p_slot_container->mem ) ) {
		internal::LogOutput( log_type::ERR, "does not match assignment address and slot_container::mem[0]" );
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION
		std::terminate();
#endif
	}
#endif

	// p_bind_mh_of_slot の中身とback_offset_へ値を埋め込む
	const std::atomic<uintptr_t>* p_back_offsetX    = &( p_slot_container->back_offset_ );
	uintptr_t                     back_offset_value = reinterpret_cast<uintptr_t>( p_bind_mh_of_slot ) - reinterpret_cast<uintptr_t>( p_back_offsetX );
	p_slot_container                                = new ( reinterpret_cast<void*>( p_slot_container ) ) slot_container( back_offset_value );

	// tail_paddingへ値を埋め込む
	unsigned char* p_tail_padding = reinterpret_cast<unsigned char*>( ans_addr + static_cast<uintptr_t>( n ) );
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_OVERRUN_WRITING
	*p_tail_padding = tail_padding_byte_v;
#endif

	// p_bind_mh_of_slot の中身を更新する
	p_bind_mh_of_slot->offset_to_tail_padding_.store( reinterpret_cast<uintptr_t>( p_tail_padding ) - reinterpret_cast<uintptr_t>( p_bind_mh_of_slot ), std::memory_order_release );

	return reinterpret_cast<void*>( ans_addr );
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
