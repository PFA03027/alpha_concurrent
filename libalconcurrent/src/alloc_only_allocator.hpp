/**
 * @file alloc_only_allocator.hpp
 * @author Teruaki Ata <PFA03027@nifty.com>
 * @brief I/F header file of allocation only allocator
 * @version 0.1
 * @date 2023-07-02
 *
 * @copyright Copyright (c) 2023 Teruaki Ata <PFA03027@nifty.com>
 *
 */

#ifndef ALLOC_ONLY_ALLOCATOR_HPP_
#define ALLOC_ONLY_ALLOCATOR_HPP_

#include <cstdint>
#include <cstdlib>

#include <atomic>

namespace alpha {
namespace concurrent {

////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace internal {

constexpr size_t default_align_size = 32;

class alloc_chamber;

class alloc_chamber_head {
public:
	static alloc_chamber_head& get_inst( void );

	void push( void* p_alloced_mem, size_t allocated_size );

private:
	alloc_chamber_head( void )
	  : head_( nullptr )
	  , ref_stack_head_( nullptr ) {}

	std::atomic<alloc_chamber*> head_;             //!< alloc_chamberのスタックリスト上のheadのalloc_chamber
	std::atomic<alloc_chamber*> ref_stack_head_;   //!< alloc_chamberの参照用スタックリスト上のheadのalloc_chamber

	static alloc_chamber_head singleton_;
};

inline alloc_chamber_head& alloc_chamber_head::get_inst( void )
{
	return singleton_;
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif   // ALLOC_ONLY_ALLOCATOR_HPP_
