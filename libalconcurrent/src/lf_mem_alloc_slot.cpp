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

#include "lf_mem_alloc_slot.hpp"

namespace alpha {
namespace concurrent {

namespace internal {
////////////////////////////////////////////////////////////////////////////////////////////////////////

void* slot_header_of_array::allocate( size_t alloc_size, size_t n, size_t req_alignsize )
{
	addr_info_of_slot ret = calc_addr_info_of_slot_of<slot_header_of_array>( reinterpret_cast<void*>( this ), alloc_size, n, req_alignsize );
	if ( !ret.is_success_ ) {
		return nullptr;
	}

	*( ret.p_back_offset_ ) = ret.value_of_back_offset_;

	mh_.offset_to_tail_padding_ = ret.value_of_offset_to_tail_padding_;
	*( ret.p_tail_padding_ )    = 1;

	return ret.p_assignment_area_;
}

void* slot_header_of_alloc::allocate( size_t alloc_size, size_t n, size_t req_alignsize )
{
	addr_info_of_slot ret = calc_addr_info_of_slot_of<slot_header_of_alloc>( reinterpret_cast<void*>( this ), alloc_size, n, req_alignsize );
	if ( !ret.is_success_ ) {
		return nullptr;
	}

	*( ret.p_back_offset_ ) = ret.value_of_back_offset_;

	mh_.offset_to_tail_padding_ = ret.value_of_offset_to_tail_padding_;
	*( ret.p_tail_padding_ )    = 1;

	return ret.p_assignment_area_;
}

unified_slot_header* unified_slot_header::get_slot_header_from_assignment_p( void* p_mem )
{
	uintptr_t addr_back_offset         = reinterpret_cast<uintptr_t>( p_mem ) - static_cast<uintptr_t>( sizeof( uintptr_t ) );
	uintptr_t value_of_back_offset     = *( reinterpret_cast<uintptr_t*>( addr_back_offset ) );
	uintptr_t addr_unified_slot_header = addr_back_offset + value_of_back_offset;
	return reinterpret_cast<unified_slot_header*>( addr_unified_slot_header );
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
