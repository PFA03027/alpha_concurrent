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
#include <stdexcept>

#include "alconcurrent/conf_logger.hpp"

#include "alconcurrent/alloc_only_allocator.hpp"
#include "mmap_allocator.hpp"

namespace alpha {
namespace concurrent {

////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace internal {

struct room_boader {
	const size_t    chopped_size_;                       //!< chopped roomのサイズ。次のroom_boaderへのオフセットも意味する。そのため、次のroom_boaderのアライメント、およびtail_padding領域を含む
	const uintptr_t offset_into_the_allocated_memory_;   //!< 呼び出し元に渡したメモリアドレスへのオフセット
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	bt_info alloc_bt_info_;   //!< backtrace information when is allocated
#endif

	room_boader( size_t chopped_size_arg, size_t req_size, size_t req_align );

	/**
	 * @brief Get the allocated memory addr that is assigned to caller
	 *
	 * @return void* the allocated memory addr
	 */
	void* get_allocated_mem_addr( void );

	void dump_to_log( log_type lt, char c, int id );

	/**
	 * @brief calculate allocated memory top address from base_addr that is room_boarder class with considering alignment
	 *
	 * @param base_addr top address of room_boarder class
	 * @param req_align considering alignment. req_align should be the power of 2
	 * @return uintptr_t allocated memory top address
	 */
	static uintptr_t calc_allocated_addr( uintptr_t base_addr, size_t req_align );
};

inline uintptr_t room_boader::calc_allocated_addr( uintptr_t base_addr, size_t req_align )
{
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
	if ( !is_power_of_2( req_align ) ) {
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_EXCEPTION
		char buff[128];
		snprintf( buff, 128, "req_align should be power of 2. but, req_align is %zu, 0x%zX", req_align, req_align );
		throw std::logic_error( buff );
#else
		internal::LogOutput( log_type::ERR, "req_align should be power of 2. but, req_align is %zu, 0x%zX", req_align, req_align );
#endif
	}
#endif
	uintptr_t addr_ch_end         = static_cast<uintptr_t>( sizeof( room_boader ) ) + base_addr;
	uintptr_t num_of_align_blocks = addr_ch_end / static_cast<uintptr_t>( req_align );   // TODO: ビットマスクを使った演算で多分軽量化できるが、まずは真面目に計算する。
#ifdef ALCONCURRENT_CONF_ENABLE_MODULO_OPERATION_BY_BITMASK
	uintptr_t r_of_align_blocks = addr_ch_end & ( static_cast<uintptr_t>( req_align ) - 1 );   // 剰余計算をビットマスク演算に変更。この時点で、req_alignが2のn乗でなければならない。
#else
	uintptr_t               r_of_align_blocks = addr_ch_end % static_cast<uintptr_t>( req_align );
#endif
	uintptr_t addr_alloc_top = static_cast<uintptr_t>( req_align ) * ( num_of_align_blocks + ( ( r_of_align_blocks == 0 ) ? 0 : 1 ) );
	return addr_alloc_top;
}

room_boader::room_boader( size_t chopped_size_arg, size_t req_size, size_t req_align )
  : chopped_size_( chopped_size_arg )
  , offset_into_the_allocated_memory_( calc_allocated_addr( reinterpret_cast<uintptr_t>( this ), req_align ) - reinterpret_cast<uintptr_t>( this ) )
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
  , alloc_bt_info_()
#endif
{
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
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

#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	alloc_bt_info_.dump_to_log( lt, c, id );
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

alloc_chamber_statistics& alloc_chamber_statistics::operator+=( const alloc_chamber_statistics& op )
{
	chamber_count_++;
	alloc_size_ += op.alloc_size_;
	consum_size_ += op.consum_size_;
	free_size_ += op.free_size_;
	return *this;
}

std::string alloc_chamber_statistics::print( void ) const
{
	char buff[2048];
	snprintf( buff, 2048,
	          "chamber count = %zu, total allocated size = 0x%zx(%.2fM), consumed size = 0x%zx(%.2fM), free size = 0x%zx(%.2fM), used ratio = %2.1f %%",
	          chamber_count_,
	          alloc_size_, (double)alloc_size_ / (double)( 1024 * 1024 ),
	          consum_size_, (double)consum_size_ / (double)( 1024 * 1024 ),
	          free_size_, (double)free_size_ / (double)( 1024 * 1024 ),
	          ( alloc_size_ > 0 ) ? ( (double)consum_size_ / (double)alloc_size_ * 100.0f ) : 0.0f );

	return std::string( buff );
}

struct alloc_chamber {
	const size_t                chamber_size_;   //!< alloc_chamberのサイズ
	std::atomic<alloc_chamber*> next_;           //!< alloc_chamberのスタックリスト上の次のalloc_chamber
	std::atomic<uintptr_t>      offset_;         //!< 次のallocateの先頭へのオフセット
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	bt_info alloc_bt_info_;   //!< backtrace information when is allocated
#endif
	unsigned char roomtop_[0];

	explicit alloc_chamber( size_t chamber_size_arg );

	void* allocate( size_t req_size, size_t req_align );

	alloc_chamber_statistics get_statistics( void ) const;

	void dump_to_log( log_type lt, char c, int id );

private:
	uintptr_t calc_addr_chopped_room_end_by( uintptr_t expected_offset_, size_t req_size, size_t req_align );

#if ( __cpp_constexpr >= 201304 )
	static constexpr uintptr_t calc_init_offset( void );
#else
	static inline uintptr_t calc_init_offset( void );
#endif
};

#if ( __cpp_constexpr >= 201304 )
constexpr
#else
inline
#endif
	uintptr_t
	alloc_chamber::calc_init_offset( void )
{
	size_t n = sizeof( alloc_chamber ) / default_align_size;   // TODO: ビットマスクを使った演算で多分軽量化できるが、まずは真面目に計算する。
	size_t r = sizeof( alloc_chamber ) % default_align_size;   // ただ、ここは、default_slot_alignsizeは定数かつ2^n上である数値なので、コンパイラの最適化がかかっていると思われる。
	return default_align_size * ( n + ( ( r == 0 ) ? 0 : 1 ) );
}

alloc_chamber::alloc_chamber( size_t chamber_size_arg )
  : chamber_size_( chamber_size_arg )
  , next_( nullptr )
  , offset_( calc_init_offset() )
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
  , alloc_bt_info_()
#endif
{
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
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
		addr_chopped_room_end = calc_addr_chopped_room_end_by( cur_offset, req_size, req_align );
		if ( addr_chopped_room_end <= reinterpret_cast<uintptr_t>( this ) ) {
			// 演算がオーバーフローしてしまうようであれば、失敗を表すnullptrで戻る。
			return nullptr;
		}
		if ( addr_chopped_room_end > ( reinterpret_cast<uintptr_t>( this ) + static_cast<uintptr_t>( chamber_size_ ) ) ) {
			// 最終アドレスが、chamber_size_を超えてしまうようであれば、失敗を表すnullptrで戻る。
			return nullptr;
		}
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
		"[%d-%c] addr = %p, allocated_size = 0x%zx, next_ = %p, offset_ = 0x%zx, remaining = 0x%zx",
		id, c,
		this,
		chamber_size_,
		next_.load( std::memory_order_acquire ),
		offset_.load( std::memory_order_acquire ),
		chamber_size_ - offset_.load( std::memory_order_acquire ) );

#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
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

alloc_chamber_statistics alloc_chamber::get_statistics( void ) const
{
	alloc_chamber_statistics ans;
	ans.alloc_size_  = chamber_size_;
	ans.consum_size_ = static_cast<size_t>( offset_.load( std::memory_order_acquire ) );
	ans.free_size_   = ans.alloc_size_ - ans.consum_size_;

	return ans;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
alloc_only_chamber::~alloc_only_chamber()
{
	if ( !need_release_munmap_ ) {
		return;
	}

	alloc_chamber* p_cur_head = head_.load( std::memory_order_acquire );
	while ( p_cur_head != nullptr ) {
		alloc_chamber* p_nxt_head = p_cur_head->next_.load( std::memory_order_acquire );
		munmap_alloc_chamber( p_cur_head );
		p_cur_head = p_nxt_head;
	}
}

void* alloc_only_chamber::try_allocate( size_t req_size, size_t req_align )
{
	alloc_chamber* p_cur_focusing_ch = head_.load( std::memory_order_acquire );
	if ( p_cur_focusing_ch == nullptr ) {
		return nullptr;
	}

	void* p_ans = p_cur_focusing_ch->allocate( req_size, req_align );
	if ( p_ans != nullptr ) {
		return p_ans;
	}

	// メモリを確保できなかった場合、hint情報から一度だけ取得できるか試す。
	alloc_chamber* p_cur_hint_ch = one_try_hint_.load( std::memory_order_acquire );
	p_cur_focusing_ch            = p_cur_hint_ch;
	if ( p_cur_focusing_ch == nullptr ) {
		p_cur_focusing_ch = head_.load( std::memory_order_acquire );
		if ( p_cur_focusing_ch == nullptr ) {
			return nullptr;
		}
		p_cur_focusing_ch = p_cur_focusing_ch->next_.load( std::memory_order_acquire );
		if ( p_cur_focusing_ch == nullptr ) {
			return nullptr;
		}
	}
	p_ans = p_cur_focusing_ch->allocate( req_size, req_align );
	if ( p_ans != nullptr ) {
		return p_ans;
	}

	// 取得を試みたが、取得できなかったので、hintを一つ動かす。
	alloc_chamber* p_nxt_hint_ch = p_cur_focusing_ch->next_.load( std::memory_order_acquire );
	one_try_hint_.compare_exchange_strong( p_cur_hint_ch, p_nxt_hint_ch, std::memory_order_acq_rel );

	return nullptr;
}

void alloc_only_chamber::push_alloc_mem( void* p_alloced_mem, size_t allocated_size )
{
	if ( p_alloced_mem == nullptr ) return;

	alloc_chamber* p_new_chamber = new ( p_alloced_mem ) alloc_chamber( allocated_size );

	alloc_chamber* p_cur_head = head_.load( std::memory_order_acquire );
	do {
		p_new_chamber->next_.store( p_cur_head, std::memory_order_release );
	} while ( !head_.compare_exchange_weak( p_cur_head, p_new_chamber, std::memory_order_acq_rel ) );

	return;
}

void alloc_only_chamber::munmap_alloc_chamber( alloc_chamber* p_ac )
{
	size_t chamber_size_of_p_ac = p_ac->chamber_size_;
	int    ret                  = deallocate_by_munmap( p_ac, chamber_size_of_p_ac );
	if ( ret != 0 ) {
		auto er_errno = errno;
		char buff[128];
#if ( _POSIX_C_SOURCE >= 200112L ) && !_GNU_SOURCE
		strerror_r( er_errno, buff, 128 );
		internal::LogOutput( log_type::ERR, "munmap is fail with %s", buff );
#else
		const char* ret_str = strerror_r( er_errno, buff, 128 );
		internal::LogOutput( log_type::ERR, "munmap is fail with %s", ret_str );
#endif
	}
	return;
}

void* alloc_only_chamber::chked_allocate( size_t req_size, size_t req_align )
{
	void* p_ans = try_allocate( req_size, req_align );

	while ( p_ans == nullptr ) {
		size_t cur_pre_alloc_size = pre_alloc_size_;
		if ( cur_pre_alloc_size < req_size ) {
			cur_pre_alloc_size = req_size * 2 + sizeof( alloc_chamber );
#if 0
			internal::LogOutput( log_type::DEBUG, "requested size is over pre allocation size: req=0x%zx, therefore try to allocate double size: try=0x%zu", req_size, cur_pre_alloc_size );
			bt_info tmp_bt;
			RECORD_BACKTRACE_GET_BACKTRACE( tmp_bt );
			tmp_bt.dump_to_log( log_type::DEBUG, 'a', 1 );
#endif
		}
		allocate_result ret_mmap = allocate_by_mmap( cur_pre_alloc_size, req_align );
		if ( ret_mmap.p_allocated_addr_ == nullptr ) return nullptr;
		if ( ret_mmap.allocated_size_ == 0 ) return nullptr;

		push_alloc_mem( ret_mmap.p_allocated_addr_, ret_mmap.allocated_size_ );
		p_ans = try_allocate( req_size, req_align );
	}

	return p_ans;
}

void alloc_only_chamber::detect_unexpected_deallocate( void* )
{
#ifdef ALCONCURRENT_CONF_DETECT_UNEXPECTED_DEALLOC_CALLING
	throw std::runtime_error( "allocating_only_deallocate is called unexpectedly" );
#endif
	return;
}

alloc_chamber_statistics alloc_only_chamber::get_statistics( void ) const
{
	alloc_chamber_statistics total_statistics;

	auto p_cur_chamber = head_.load( std::memory_order_acquire );
	while ( p_cur_chamber != nullptr ) {
		total_statistics += p_cur_chamber->get_statistics();
		p_cur_chamber = p_cur_chamber->next_.load( std::memory_order_acquire );
	}

	return total_statistics;
}

void alloc_only_chamber::dump_to_log( log_type lt, char c, int id )
{
	alloc_chamber_statistics total_statistics;

	auto p_cur_chamber = head_.load( std::memory_order_acquire );
	while ( p_cur_chamber != nullptr ) {
		p_cur_chamber->dump_to_log( lt, c, id );
		p_cur_chamber = p_cur_chamber->next_.load( std::memory_order_acquire );
	}

	total_statistics = get_statistics();
	internal::LogOutput( lt, "[%d-%c] %s", id, c, total_statistics.print().c_str() );
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
