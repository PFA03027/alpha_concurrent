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
#include <string>

#include "alconcurrent/conf_logger.hpp"

namespace alpha {
namespace concurrent {

////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace internal {

////////////////////////////////////////////////////////////////////////////////////////////////////////
// internal I/F
constexpr size_t default_align_size = 32;

class alloc_chamber;

struct alloc_chamber_statistics {
	size_t chamber_count_;
	size_t alloc_size_;
	size_t consum_size_;
	size_t free_size_;

	constexpr alloc_chamber_statistics( void )
	  : chamber_count_( 0 )
	  , alloc_size_( 0 )
	  , consum_size_( 0 )
	  , free_size_( 0 )
	{
	}

	alloc_chamber_statistics& operator+=( const alloc_chamber_statistics& op );

	std::string print( void ) const;
};

class alloc_only_chamber {
public:
	constexpr alloc_only_chamber( bool need_release_munmap_arg, size_t pre_alloc_size_arg )
	  : head_( nullptr )
	  , one_try_hint_( nullptr )
	  , need_release_munmap_( need_release_munmap_arg )
	  , pre_alloc_size_( pre_alloc_size_arg )
	{
	}

	~alloc_only_chamber();

	void* allocate( size_t req_size, size_t req_align );

	void detect_unexpected_deallocate( void* );

	alloc_chamber_statistics get_statistics( void ) const;
	void                     dump_to_log( log_type lt, char c, int id );

private:
	void* try_allocate( size_t req_size, size_t req_align );
	void  push_alloc_mem( void* p_alloced_mem, size_t allocated_size );
	void  munmap_alloc_chamber( alloc_chamber* p_ac );

	std::atomic<alloc_chamber*> head_;                  //!< alloc_chamberのスタックリスト上のheadのalloc_chamber
	std::atomic<alloc_chamber*> one_try_hint_;          //!< alloc_chamberのスタックリスト上、一度だけチェックを行う先を示すポインタ。
	bool                        need_release_munmap_;   //!< true: when destructing, munmap memory
	size_t                      pre_alloc_size_;        //!< mmapで割り当てる基本サイズ
};
static_assert( std::is_standard_layout<alloc_only_chamber>::value, "alloc_only_chamber should be standard-layout type" );

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif   // ALLOC_ONLY_ALLOCATOR_HPP_
