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

#include "alconcurrent/conf_logger.hpp"

#include "alloc_only_allocator.hpp"
#include "mmap_allocator.hpp"

namespace alpha {
namespace concurrent {

////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace internal {

struct room_boader {
	const size_t    chopped_size_;                       //!< chopped roomのサイズ。次のroom_boaderへのオフセットも意味する。そのため、次のroom_boaderのアライメント、およびtail_padding領域を含む
	const uintptr_t offset_into_the_allocated_memory_;   //!< 呼び出し元に渡したメモリアドレスへのオフセット
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	bt_info alloc_bt_info_;                              //!< backtrace information when is allocated
#endif

	room_boader( size_t chopped_size_arg, size_t req_size, size_t req_align );

	void* get_allocated_mem_addr( void );

	void dump_to_log( log_type lt, char c, int id );

	static uintptr_t calc_allocated_addr( uintptr_t base_addr, size_t req_align );
};

inline uintptr_t room_boader::calc_allocated_addr( uintptr_t base_addr, size_t req_align )
{
	uintptr_t addr_ch_end         = static_cast<uintptr_t>( sizeof( room_boader ) ) + base_addr;
	uintptr_t num_of_align_blocks = addr_ch_end / static_cast<uintptr_t>( req_align );
	uintptr_t r_of_align_blocks   = addr_ch_end % static_cast<uintptr_t>( req_align );
	uintptr_t addr_alloc_top      = static_cast<uintptr_t>( req_align ) * ( num_of_align_blocks + ( ( r_of_align_blocks == 0 ) ? 0 : 1 ) );
	return addr_alloc_top;
}

room_boader::room_boader( size_t chopped_size_arg, size_t req_size, size_t req_align )
  : chopped_size_( chopped_size_arg )
  , offset_into_the_allocated_memory_( calc_allocated_addr( reinterpret_cast<uintptr_t>( this ), req_align ) - reinterpret_cast<uintptr_t>( this ) )
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
  , alloc_bt_info_()
#endif
{
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	RECORD_BACKTRACE_GET_BACKTRACE( alloc_bt_info_ );
#endif

	uintptr_t      addr_tail_padding  = reinterpret_cast<uintptr_t>( this ) + offset_into_the_allocated_memory_ + req_size;
	unsigned char* p_top_tail_padding = reinterpret_cast<unsigned char*>( addr_tail_padding );
	*p_top_tail_padding               = 0xFF;   // TODO: アクセスオーバーライト検出のマーク値は仮実装
}

inline void* room_boader::get_allocated_mem_addr( void )
{
	uintptr_t allocated_mem_addr = reinterpret_cast<uintptr_t>( this ) + offset_into_the_allocated_memory_;
	return reinterpret_cast<void*>( allocated_mem_addr );
}

void room_boader::dump_to_log( log_type lt, char c, int id )
{
	internal::LogOutput(
		lt,
		"[%d-%c]\taddr = %p, chopped_size_ = 0x%zx, offset_into_the_allocated_memory_ = 0x%zx(%p)",
		id, c,
		this,
		chopped_size_,
		offset_into_the_allocated_memory_,
		reinterpret_cast<void*>( reinterpret_cast<uintptr_t>( this ) + offset_into_the_allocated_memory_ ) );

#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	alloc_bt_info_.dump_to_log( lt, c, id );
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
struct alloc_chamber {
	const size_t                chamber_size_;     //!< alloc_chamberのサイズ
	std::atomic<alloc_chamber*> next_;             //!< alloc_chamberのスタックリスト上の次のalloc_chamber
	std::atomic<alloc_chamber*> ref_stack_next_;   //!< alloc_chamberの参照用スタックリスト上の次のalloc_chamber
	std::atomic<uintptr_t>      offset_;           //!< 次のallocateの先頭へのオフセット
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	bt_info alloc_bt_info_;                        //!< backtrace information when is allocated
#endif
	unsigned char roomtop_[0];

	explicit alloc_chamber( size_t chamber_size_arg );

	void* allocate( size_t req_size, size_t req_align );

	void dump_to_log( log_type lt, char c, int id );

private:
	uintptr_t calc_addr_chopped_room_end_by( uintptr_t expected_offset_, size_t req_size, size_t req_align );

	static constexpr uintptr_t calc_init_offset( void );
};

constexpr uintptr_t alloc_chamber::calc_init_offset( void )
{
	size_t n = sizeof( alloc_chamber ) / default_align_size;
	size_t r = sizeof( alloc_chamber ) % default_align_size;
	return default_align_size * ( n + ( ( r == 0 ) ? 0 : 1 ) );
}

alloc_chamber::alloc_chamber( size_t chamber_size_arg )
  : chamber_size_( chamber_size_arg )
  , next_( nullptr )
  , ref_stack_next_( nullptr )
  , offset_( calc_init_offset() )
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
  , alloc_bt_info_()
#endif
{
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	RECORD_BACKTRACE_GET_BACKTRACE( alloc_bt_info_ );
#endif
}

inline uintptr_t alloc_chamber::calc_addr_chopped_room_end_by( uintptr_t expected_offset_, size_t req_size, size_t req_align )
{
	uintptr_t base_addr             = expected_offset_ + reinterpret_cast<uintptr_t>( this );
	uintptr_t addr_alloc_top        = room_boader::calc_allocated_addr( base_addr, req_align );
	uintptr_t addr_alloc_end        = addr_alloc_top + req_size;
	uintptr_t num_of_align_end      = ( addr_alloc_end + default_align_size ) / default_align_size;
	uintptr_t addr_chopped_room_end = num_of_align_end * default_align_size;
	return addr_chopped_room_end;
}

void* alloc_chamber::allocate( size_t req_size, size_t req_align )
{
	uintptr_t cur_offset             = offset_.load( std::memory_order_acquire );
	uintptr_t final_candidate_offset = 0;
	uintptr_t addr_chopped_room_end  = 0;
	do {
		if ( ( static_cast<uintptr_t>( chamber_size_ ) - cur_offset ) < ( req_size + default_align_size ) ) {
			// 残量がなければ即座に失敗を表すnullptrで戻る。
			return nullptr;
		}
		addr_chopped_room_end  = calc_addr_chopped_room_end_by( cur_offset, req_size, req_align );
		final_candidate_offset = addr_chopped_room_end - reinterpret_cast<uintptr_t>( this );
	} while ( !offset_.compare_exchange_strong( cur_offset, final_candidate_offset ) );   // 置き換え失敗している間、ループする。
	// 置き換えに成功したので、cur_offsetに確保できたchopped roomの先頭へのオフセットが格納されている

	uintptr_t    addr_top_my_chopped_room = reinterpret_cast<uintptr_t>( this ) + cur_offset;
	size_t       final_chopped_room_size  = static_cast<size_t>( addr_chopped_room_end - addr_top_my_chopped_room );
	void*        p_top_my_chopped_room    = reinterpret_cast<void*>( addr_top_my_chopped_room );
	room_boader* p_rb                     = new ( p_top_my_chopped_room ) room_boader( final_chopped_room_size, req_size, req_align );
	void*        p_ans                    = p_rb->get_allocated_mem_addr();
	return p_ans;
}

void alloc_chamber::dump_to_log( log_type lt, char c, int id )
{
	internal::LogOutput(
		lt,
		// "[%d-%c] addr = %p, allocated_size = 0x%zx, next_ = %p, ref_stack_next_ = %p, offset_ = 0x%zx",
		"[%d-%c] addr = %p, allocated_size = 0x%zx, next_ = %p, offset_ = 0x%zx",
		id, c,
		this,
		chamber_size_,
		next_.load( std::memory_order_acquire ),
		ref_stack_next_.load( std::memory_order_acquire ),
		offset_.load( std::memory_order_acquire ) );

#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	alloc_bt_info_.dump_to_log( lt, c, id );
#endif

	uintptr_t last_offset      = offset_.load( std::memory_order_acquire );
	uintptr_t cur_offset_to_rb = calc_init_offset();
	while ( cur_offset_to_rb < last_offset ) {
		uintptr_t    addr_cur_rb = reinterpret_cast<uintptr_t>( this ) + cur_offset_to_rb;
		room_boader* p_cur_rb    = reinterpret_cast<room_boader*>( addr_cur_rb );
		p_cur_rb->dump_to_log( lt, c, id );
		cur_offset_to_rb += p_cur_rb->chopped_size_;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
alloc_chamber_head          alloc_chamber_head::singleton_;
thread_local alloc_chamber* alloc_chamber_head::p_forcusing_chamber_ = nullptr;

void alloc_chamber_head::push_alloc_mem( void* p_alloced_mem, size_t allocated_size )
{
	if ( p_alloced_mem == nullptr ) return;

	alloc_chamber* p_new_chamber = new ( p_alloced_mem ) alloc_chamber( allocated_size );

	alloc_chamber* p_cur_head = head_.load( std::memory_order_acquire );
	do {
		p_new_chamber->next_.store( p_cur_head, std::memory_order_release );
	} while ( !head_.compare_exchange_strong( p_cur_head, p_new_chamber ) );

	p_forcusing_chamber_ = p_new_chamber;

	return;
}

void* alloc_chamber_head::allocate( size_t req_size, size_t req_align )
{
	alloc_chamber* p_cur_focusing_ch = p_forcusing_chamber_;
	if ( p_cur_focusing_ch == nullptr ) {
		p_cur_focusing_ch = head_.load( std::memory_order_acquire );
		if ( p_cur_focusing_ch == nullptr ) {
			return nullptr;
		}
		p_forcusing_chamber_ = p_cur_focusing_ch;
	}

	return p_cur_focusing_ch->allocate( req_size, req_align );
}

void alloc_chamber_head::dump_to_log( log_type lt, char c, int id )
{
	auto p_cur_chamber = head_.load( std::memory_order_acquire );
	while ( p_cur_chamber != nullptr ) {
		p_cur_chamber->dump_to_log( lt, c, id );
		p_cur_chamber = p_cur_chamber->next_.load( std::memory_order_acquire );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void* allocating_only( size_t req_size, size_t req_align )
{
	static_assert( conf_pre_mmap_size > sizeof( alloc_chamber ), "conf_pre_mmap_size is too small" );

	void* p_ans = alloc_chamber_head::get_inst().allocate( req_size, req_align );
	if ( p_ans != nullptr ) return p_ans;

	size_t cur_pre_alloc_size = conf_pre_mmap_size;
	if ( cur_pre_alloc_size < req_size ) {
		cur_pre_alloc_size = req_size * 2 + sizeof( alloc_chamber );
	}
	allocate_result ret_mmap = allocate_by_mmap( cur_pre_alloc_size, default_align_size );
	if ( ret_mmap.p_allocated_addr_ == nullptr ) return nullptr;
	if ( ret_mmap.allocated_size_ == 0 ) return nullptr;

	alloc_chamber_head::get_inst().push_alloc_mem( ret_mmap.p_allocated_addr_, ret_mmap.allocated_size_ );

	return alloc_chamber_head::get_inst().allocate( req_size, req_align );
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
