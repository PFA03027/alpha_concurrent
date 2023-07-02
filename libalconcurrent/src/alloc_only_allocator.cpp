/**
 * @file alloc_only_allocator.cpp
 * @author Teruaki Ata <PFA03027@nifty.com>
 * @brief allocation only allocator
 * @version 0.1
 * @date 2023-07-02
 *
 * @copyright Copyright (c) 2023 Teruaki Ata <PFA03027@nifty.com>
 *
 */

#include <cstdint>
#include <cstdlib>

#include <atomic>
#include <new>

#include "alloc_only_allocator.hpp"

namespace alpha {
namespace concurrent {

////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace internal {

struct alloc_chamber {
	size_t                      allocated_size_;   //!< alloc_chamberのサイズ
	std::atomic<alloc_chamber*> next_;             //!< alloc_chamberのスタックリスト上の次のalloc_chamber
	std::atomic<alloc_chamber*> ref_stack_next_;   //!< alloc_chamberの参照用スタックリスト上の次のalloc_chamber
	size_t                      offset_;           //!< 次のallocateの先頭へのオフセット

	explicit alloc_chamber( size_t allocated_size_arg );

	static constexpr size_t calc_init_offset( void );
};

constexpr size_t alloc_chamber::calc_init_offset( void )
{
	size_t n = sizeof( alloc_chamber ) / default_align_size;
	size_t r = sizeof( alloc_chamber ) % default_align_size;
	return default_align_size * ( n + ( ( r == 0 ) ? 0 : 1 ) );
}

alloc_chamber::alloc_chamber( size_t allocated_size_arg )
  : allocated_size_( allocated_size_arg )
  , next_( nullptr )
  , ref_stack_next_( nullptr )
  , offset_( calc_init_offset() )
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
alloc_chamber_head alloc_chamber_head::singleton_;

void alloc_chamber_head::push( void* p_alloced_mem, size_t allocated_size )
{
	alloc_chamber* p_new_chamber = new ( p_alloced_mem ) alloc_chamber( allocated_size );

	alloc_chamber* p_cur_head = head_.load( std::memory_order_acquire );
	do {
		p_new_chamber->next_.store( p_cur_head, std::memory_order_release );
	} while ( head_.compare_exchange_strong( p_cur_head, p_new_chamber ) );

	return;
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
