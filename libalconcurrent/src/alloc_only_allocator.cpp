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

#include <cerrno>
#include <cstdint>
#include <cstdlib>

#include <atomic>
#include <new>
#include <stdexcept>

#include "alconcurrent/conf_logger.hpp"

#include "alconcurrent/internal/alloc_only_allocator.hpp"
#include "mmap_allocator.hpp"

namespace alpha {
namespace concurrent {

////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace internal {

struct alloc_chamber;

struct alloc_in_room {
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	bt_info alloc_bt_info_;     //!< backtrace information when is allocated
	bt_info dealloc_bt_info_;   //!< backtrace information when is deallocated
#endif
	std::atomic<bool>    is_freeed_;            //!< freeされた場合、真を示す。
	const alloc_chamber* p_to_alloc_chamber_;   //!< このchopped roomが属するalloc_chamberへのポインタ
	unsigned char        mem[0];                //!< allocateしたメモリ領域

	explicit alloc_in_room( const alloc_chamber* p_ac )
	  :
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	  alloc_bt_info_()
	  , dealloc_bt_info_()
	  ,
#endif
	  is_freeed_( false )
	  , p_to_alloc_chamber_( p_ac )
	{
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
		RECORD_BACKTRACE_GET_BACKTRACE( alloc_bt_info_ );
#endif
	}

	void dump_to_log( log_type lt, char c, int id )
	{
		internal::LogOutput(
			lt,
			"[%d-%c]\taddr = %p, is_freeed_ = %s, p_to_alloc_chamber_ = %p",
			id, c,
			this,
			is_freeed_.load( std::memory_order_acquire ) ? "true" : "false",
			p_to_alloc_chamber_ );
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
		internal::LogOutput( lt, "[%d-%c]\taddr = %p, alloc_bt_info_", id, c, this );
		alloc_bt_info_.dump_to_log( lt, c, id );
		internal::LogOutput( lt, "[%d-%c]\taddr = %p, dealloc_bt_info_", id, c, this );
		dealloc_bt_info_.dump_to_log( lt, c, id );
#endif
	}
};

struct room_boader {
	const size_t         chopped_size_;      //!< chopped roomのサイズ。次のroom_boaderへのオフセットも意味する。そのため、次のroom_boaderのアライメント、およびtail_padding領域を含む
	alloc_in_room* const p_alloc_in_room_;   //!< alloc_in_roomの先頭メモリアドレス

	room_boader( const alloc_chamber* p_parent, size_t chopped_size_arg, size_t req_size, size_t req_align );

	/**
	 * @brief Get the allocated memory addr that is assigned to caller
	 *
	 * @return void* the allocated memory addr
	 */
	void* get_allocated_mem_pointer( void );

	void dump_to_log( log_type lt, char c, int id );

	/**
	 * @brief calculate allocated memory top address from base_addr that is room_boarder class with considering alignment
	 *
	 * @param base_addr top address of room_boarder class
	 * @param req_size required memory size to allocate
	 * @param req_align considering alignment. req_align should be the power of 2
	 * @return uintptr_t allocated memory top address
	 */
	static uintptr_t calc_addr_of_end_of_tail_padding_based_on_room_boader( uintptr_t base_addr, size_t req_size, size_t req_align );

	static bool try_marks_as_deallocated( void* p_mem );

private:
	/**
	 * @brief calculate allocated memory top address from base_addr that is room_boarder class with considering alignment
	 *
	 * @param base_addr top address of room_boarder class
	 * @param req_align considering alignment. req_align should be the power of 2
	 * @return uintptr_t allocated memory top address
	 */
	static uintptr_t calc_addr_of_allocated_memory_based_on_room_boader( uintptr_t base_addr, size_t req_align );

	/**
	 * @brief calculate alloc_in_room top address from base_addr that is room_boarder class with considering alignment
	 *
	 * @param base_addr top address of room_boarder class
	 * @param req_align considering alignment. req_align should be the power of 2
	 * @return uintptr_t allocated memory top address
	 */
	static alloc_in_room* calc_pointer_of_alloc_in_room_based_on_room_boarder( uintptr_t base_addr, size_t req_align );

	/**
	 * @brief calculate alloc_in_room top address from base_addr that is room_boarder class with considering alignment
	 *
	 * @param base_addr top address of room_boarder class
	 * @param req_align considering alignment. req_align should be the power of 2
	 * @return uintptr_t allocated memory top address
	 */
	static unsigned char* calc_pointer_of_tail_padding_based_on_room_boarder( uintptr_t base_addr, size_t req_size, size_t req_align );

	/**
	 * @brief calculate alloc_in_room top address from the allocated memory address
	 *
	 * @param p_mem pointer to the allocated memory that get_allocated_mem_pointer() is returned normaly.
	 * @return uintptr_t allocated memory top address
	 */
	static alloc_in_room* calc_addr_of_alloc_in_room_from_allocated_memory( void* p_mem );
};

inline uintptr_t room_boader::calc_addr_of_allocated_memory_based_on_room_boader( uintptr_t base_addr, size_t req_align )
{
#if defined( ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR ) || defined( ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_EXCEPTION )
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
	uintptr_t addr_ch_end         = base_addr + static_cast<uintptr_t>( sizeof( room_boader ) ) + static_cast<uintptr_t>( sizeof( alloc_in_room ) );
	uintptr_t num_of_align_blocks = addr_ch_end / static_cast<uintptr_t>( req_align );   // TODO: ビットマスクを使った演算で多分軽量化できるが、まずは真面目に計算する。
#ifdef ALCONCURRENT_CONF_ENABLE_MODULO_OPERATION_BY_BITMASK
	uintptr_t r_of_align_blocks = addr_ch_end & ( static_cast<uintptr_t>( req_align ) - 1 );   // 剰余計算をビットマスク演算に変更。この時点で、req_alignが2のn乗でなければならない。
#else
	uintptr_t r_of_align_blocks = addr_ch_end % static_cast<uintptr_t>( req_align );
#endif
	uintptr_t addr_alloc_top = static_cast<uintptr_t>( req_align ) * ( num_of_align_blocks + ( ( r_of_align_blocks == 0 ) ? 0 : 1 ) );
	return addr_alloc_top;
}

inline alloc_in_room* room_boader::calc_addr_of_alloc_in_room_from_allocated_memory( void* p_mem )
{
	uintptr_t      addr_allocated_mem = reinterpret_cast<uintptr_t>( p_mem );
	uintptr_t      addr_ans           = addr_allocated_mem - sizeof( alloc_in_room );
	alloc_in_room* p_ans              = reinterpret_cast<alloc_in_room*>( addr_ans );

#if defined( ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR ) || defined( ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_EXCEPTION )
	if ( addr_allocated_mem != reinterpret_cast<uintptr_t>( p_ans->mem ) ) {
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_EXCEPTION
		char buff[128];
		snprintf( buff, 128, "calculated address is different to actual address 0x%zu, 0x%zu", addr_allocated_mem, reinterpret_cast<uintptr_t>( p_ans->mem ) );
		throw std::logic_error( buff );
#else
		internal::LogOutput( log_type::ERR, "calculated address is different to actual address 0x%zu, 0x%zu", addr_allocated_mem, reinterpret_cast<uintptr_t>( p_ans->mem ) );
#endif
	}
#endif

	return p_ans;
}

inline alloc_in_room* room_boader::calc_pointer_of_alloc_in_room_based_on_room_boarder( uintptr_t base_addr, size_t req_align )
{
	uintptr_t addr_allocated_mem = calc_addr_of_allocated_memory_based_on_room_boader( base_addr, req_align );
	return calc_addr_of_alloc_in_room_from_allocated_memory( reinterpret_cast<void*>( addr_allocated_mem ) );
}

inline unsigned char* room_boader::calc_pointer_of_tail_padding_based_on_room_boarder( uintptr_t base_addr, size_t req_size, size_t req_align )
{
	uintptr_t addr_allocated_mem = calc_addr_of_allocated_memory_based_on_room_boader( base_addr, req_align );
	uintptr_t addr_tail_padding  = addr_allocated_mem + req_size;
	return reinterpret_cast<unsigned char*>( addr_tail_padding );
}

inline uintptr_t room_boader::calc_addr_of_end_of_tail_padding_based_on_room_boader( uintptr_t base_addr, size_t req_size, size_t req_align )
{
	uintptr_t addr_alloc_top        = calc_addr_of_allocated_memory_based_on_room_boader( base_addr, req_align );
	uintptr_t addr_alloc_end        = addr_alloc_top + req_size;
	uintptr_t num_of_align_end      = ( addr_alloc_end + default_align_size ) / default_align_size;
	uintptr_t addr_chopped_room_end = num_of_align_end * default_align_size;
	return addr_chopped_room_end;
}

room_boader::room_boader( const alloc_chamber* p_parent, size_t chopped_size_arg, size_t req_size, size_t req_align )
  : chopped_size_( chopped_size_arg )
  , p_alloc_in_room_( new( calc_pointer_of_alloc_in_room_based_on_room_boarder( reinterpret_cast<uintptr_t>( this ), req_align ) ) alloc_in_room( p_parent ) )
{
	unsigned char* p_top_tail_padding = calc_pointer_of_tail_padding_based_on_room_boarder( reinterpret_cast<uintptr_t>( this ), req_size, req_align );
	*p_top_tail_padding               = 0xFF;   // TODO: オーバーラン書き込み検出のマーク値は仮実装

#if defined( ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR ) || defined( ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_EXCEPTION )
	uintptr_t addr_end_of_room_boader   = reinterpret_cast<uintptr_t>( this ) + sizeof( room_boader );
	uintptr_t addr_top_of_alloc_in_room = reinterpret_cast<uintptr_t>( p_alloc_in_room_ );
	if ( addr_end_of_room_boader > addr_top_of_alloc_in_room ) {
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_EXCEPTION
		char buff[128];
		snprintf( buff, 128, "room_boader and alloc_in_room is overlapped, addr_end_of_room_boader = 0x%zu, addr_top_of_alloc_in_room = 0x%zu", addr_end_of_room_boader, addr_top_of_alloc_in_room );
		throw std::logic_error( buff );
#else
		internal::LogOutput( log_type::ERR, "room_boader and alloc_in_room is overlapped, addr_end_of_room_boader = 0x%zu, addr_top_of_alloc_in_room = 0x%zu", addr_end_of_room_boader, addr_top_of_alloc_in_room );
#endif
	}
#endif
}

inline void* room_boader::get_allocated_mem_pointer( void )
{
	return reinterpret_cast<void*>( p_alloc_in_room_->mem );
}

void room_boader::dump_to_log( log_type lt, char c, int id )
{
	internal::LogOutput(
		lt,
		"[%d-%c]\taddr = %p, chopped_size_ = 0x%zx, p_alloc_in_room_ = %p",
		id, c,
		this,
		chopped_size_,
		reinterpret_cast<void*>( p_alloc_in_room_ ) );

	p_alloc_in_room_->dump_to_log( lt, c, id );
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
	const uintptr_t             magic_number_;   //!< alloc_chamber構造体であることを示すマジックナンバー
	const size_t                chamber_size_;   //!< alloc_chamberのサイズ
	std::atomic<alloc_chamber*> next_;           //!< alloc_chamberのスタックリスト上の次のalloc_chamber
	std::atomic<uintptr_t>      offset_;         //!< 次のallocateの先頭へのオフセット
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	bt_info alloc_bt_info_;   //!< backtrace information when is allocated
#endif
	unsigned char roomtop_[0];

	explicit alloc_chamber( size_t chamber_size_arg );

	void* allocate( size_t req_size, size_t req_align );

	room_boader* search_associated_room_boader( void* p_mem ) const;

	alloc_chamber_statistics get_statistics( void ) const;

	void dump_to_log( log_type lt, char c, int id );

	static inline bool is_alloc_chamber( const alloc_chamber* p_test )
	{
		return ( p_test->magic_number_ == kMagicNumber );
	}

	static inline bool try_deallocate( void* p_mem )
	{
		return room_boader::try_marks_as_deallocated( p_mem );
	}

private:
	uintptr_t calc_addr_chopped_room_end_by( uintptr_t expected_offset_, size_t req_size, size_t req_align );

#if ( __cpp_constexpr >= 201304 )
	static constexpr uintptr_t calc_init_offset( void );
#else
	static inline uintptr_t calc_init_offset( void );
#endif
	static constexpr uintptr_t kMagicNumber = 0x416c6c6343686d62;   //!< 'AllcChmb'
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
  : magic_number_( kMagicNumber )
  , chamber_size_( chamber_size_arg )
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
	uintptr_t base_addr = expected_offset_ + reinterpret_cast<uintptr_t>( this );
	return room_boader::calc_addr_of_end_of_tail_padding_based_on_room_boader( base_addr, req_size, req_align );
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
	room_boader* p_rb                     = new ( p_top_my_chopped_room ) room_boader( this, final_chopped_room_size, req_size, req_align );
	void*        p_ans                    = p_rb->get_allocated_mem_pointer();
	return p_ans;
}

room_boader* alloc_chamber::search_associated_room_boader( void* p_mem ) const
{
	const uintptr_t addr_top_free_room = offset_.load( std::memory_order_acquire ) + reinterpret_cast<uintptr_t>( this );
	const uintptr_t addr_p_mem         = reinterpret_cast<uintptr_t>( p_mem );
	uintptr_t       addr_cur_rb        = reinterpret_cast<uintptr_t>( this ) + calc_init_offset();

	while ( addr_cur_rb < addr_top_free_room ) {
		room_boader* p_cur_rb    = reinterpret_cast<room_boader*>( addr_cur_rb );
		uintptr_t    addr_nxt_rb = addr_cur_rb + p_cur_rb->chopped_size_;
		if ( ( addr_cur_rb < addr_p_mem ) && ( addr_p_mem < addr_nxt_rb ) ) {
			return p_cur_rb;   // p_memが所属するchopped roomを特定できた。
		}
		addr_cur_rb = addr_nxt_rb;
	}

	// p_memが所属するchopped roomを特定できなかった。
	return nullptr;
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

bool room_boader::try_marks_as_deallocated( void* p_mem )
{
	alloc_in_room* p_air = room_boader::calc_addr_of_alloc_in_room_from_allocated_memory( p_mem );
	if ( !alloc_chamber::is_alloc_chamber( p_air->p_to_alloc_chamber_ ) ) {
		internal::LogOutput( log_type::ERR, "required address(%p) is not the allocated memory by alloc_chamber", p_mem );
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
		bt_info cur_bt_info;   //!< backtrace information when is allocated
		RECORD_BACKTRACE_GET_BACKTRACE( cur_bt_info );
#endif
		return false;
	}

	bool expected_is_free = false;
	if ( !p_air->is_freeed_.compare_exchange_strong( expected_is_free, true, std::memory_order_release ) ) {
		internal::LogOutput( log_type::ERR, "required address(%p) is freed already. This is double free issue.", p_mem );
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
		bt_info cur_bt_info;   //!< backtrace information when is allocated
		RECORD_BACKTRACE_GET_BACKTRACE( cur_bt_info );

		p_air->alloc_bt_info_.dump_to_log( log_type::ERR, 'a', 1 );
		p_air->alloc_bt_info_.dump_to_log( log_type::ERR, 'd', 1 );
		cur_bt_info.dump_to_log( log_type::ERR, 'c', 1 );
#endif
		return false;
	}

	// ここに来た段階で、is_freed_は、trueになっている。
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	RECORD_BACKTRACE_GET_BACKTRACE( p_air->dealloc_bt_info_ );
#endif
	return true;
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

void alloc_only_chamber::deallocate( void* p_mem )
{
	alloc_chamber::try_deallocate( p_mem );
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
