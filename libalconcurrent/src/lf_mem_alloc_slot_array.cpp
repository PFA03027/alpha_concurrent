/**
 * @file lf_mem_alloc_slot_array.cpp
 * @author Teruaki Ata <PFA03027@nifty.com>
 * @brief implemenation of slot array and slot array manager
 * @version 0.1
 * @date 2023-08-06
 *
 * @copyright Copyright (c) 2023 Teruaki Ata <PFA03027@nifty.com>
 *
 */

#include "lf_mem_alloc_slot_array.hpp"
#include "mmap_allocator.hpp"

namespace alpha {
namespace concurrent {

namespace internal {
////////////////////////////////////////////////////////////////////////////////////////////////////////

// usual new...(1)   このクラスでは使用してはならないnew operator
void* slot_array_mgr::operator new( std::size_t n )
{
	throw std::logic_error( "called prohibit slot_array_mgr::operator new( std::size_t n )" );
}
// usual delete...(2)
void slot_array_mgr::operator delete( void* p_mem ) noexcept
{
	size_t alloc_size  = *( reinterpret_cast<size_t*>( p_mem ) );   // ちょっとトリッキーな方法でmmapで確保したサイズ情報を引き出す。const size_tで宣言されているので、書き換えは発生しないことを利用する。
	int    dealloc_ret = deallocate_by_munmap( p_mem, alloc_size );
	if ( dealloc_ret != 0 ) {
		LogOutput( log_type::ERR, "fail deallocate_by_munmap(%p, %zu)", p_mem, alloc_size );
	}
}

// usual new...(1)   このクラスでは使用してはならないnew operator
void* slot_array_mgr::operator new[]( std::size_t n )
{
	throw std::logic_error( "called prohibit slot_array_mgr::operator new[]( std::size_t n )" );
}
// usual delete...(2)   このクラスでは使用してはならないdelete operator
void slot_array_mgr::operator delete[]( void* p_mem ) noexcept
{
	// throw std::logic_error("called prohibit slot_array_mgr::operator delete[]( std::size_t n )");
	return;
}

// placement new    可変長部分の領域も確保するnew operator
void* slot_array_mgr::operator new( std::size_t n_of_slot_array_mgr, size_t num_of_slots_, size_t expected_alloc_n_per_slot )
{
	size_t total_size = n_of_slot_array_mgr + calc_total_slot_array_bytes( num_of_slots_, expected_alloc_n_per_slot );
	auto   alloc_ret  = allocate_by_mmap( total_size, default_slot_alignsize );
	if ( alloc_ret.p_allocated_addr_ == nullptr ) {
		throw std::bad_alloc();
	}

	*( reinterpret_cast<size_t*>( alloc_ret.p_allocated_addr_ ) ) = alloc_ret.allocated_size_;   // ちょっとトリッキーな方法でmmapで確保したサイズ情報をコンストラクタに渡す
	return alloc_ret.p_allocated_addr_;
}
// placement delete...(3)   このクラスでは使用してはならないdelete operator。このdelete operator自身は何もしない。
void slot_array_mgr::operator delete( void* p, void* p2 ) noexcept
{
	// throw std::logic_error("called prohibit slot_array_mgr::operator delete( void* p, void* p2 )");
	return;
}

slot_array_mgr::slot_array_mgr( chunk_header_multi_slot* p_owner, size_t num_of_slots, size_t n )
  : alloc_size_( *( reinterpret_cast<size_t*>( this ) ) )   // ちょっとトリッキーなー方法で確保されたサイズ情報をopertor new()から受け取る。
  , num_of_slots_( num_of_slots )
  , expected_n_per_slot_( n )
  , slot_size_of_this_( calc_one_slot_bytes( n ) )
  , p_owner_chunk_header_( p_owner )
  , p_free_slot_stack_head_( nullptr )
  , mtx_consignment_stack_()
  , p_consignment_stack_head_( nullptr )
  , slot_container_ {}
{
	if ( num_of_slots_ == 0 ) {
		return;
	}

	slot_header_of_array* p_pre_slot_header_of_array = nullptr;
	slot_header_of_array* p_cur_slot                 = get_pointer_of_slot( num_of_slots_ - 1 );
	for ( size_t i = 0; i < num_of_slots_; i++ ) {
		new ( p_cur_slot ) slot_header_of_array( reinterpret_cast<void*>( this ), p_pre_slot_header_of_array );
		p_cur_slot = unchk_get_pre_pointer_of_slot( p_cur_slot );
	}
	p_free_slot_stack_head_.store( p_pre_slot_header_of_array, std::memory_order_release );
}

size_t slot_array_mgr::get_slot_idx_from_assignment_p( void* p_mem )
{
	if ( p_mem == nullptr ) {
		std::string errlog = "try to get slot idx by nullptr";
		throw std::out_of_range( errlog );
	}

	unified_slot_header* p_slot_header = unified_slot_header::get_slot_header_from_assignment_p( p_mem );
	if ( p_slot_header->mh_.offset_to_mgr_ == 0 ) {
		std::string errlog = "this slot is not slot_header_of_array.";
		throw std::runtime_error( errlog );
	}
#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
	if ( !p_slot_header->mh_.check_marker() ) {
		std::string errlog = std::string( "slot header(" ) + std::to_string( p_slot_header ) + std::string( ") is corrupted" );
		throw std::runtime_error( errlog );
	}
#endif

	slot_array_mgr* p_mgr       = p_slot_header->arrayh_.mh_.get_mgr_pointer<slot_array_mgr>();
	uintptr_t       byte_offset = reinterpret_cast<uintptr_t>( p_slot_header ) - reinterpret_cast<uintptr_t>( p_mgr->slot_container_ );
	uintptr_t       ans_idx     = byte_offset / p_mgr->slot_size_of_this_;

	return static_cast<size_t>( ans_idx );
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
