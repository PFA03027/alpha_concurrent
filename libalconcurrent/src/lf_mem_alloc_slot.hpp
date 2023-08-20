/**
 * @file lf_mem_alloc_slot.hpp
 * @author Teruaki Ata <PFA03027@nifty.com>
 * @brief header file of slot structure and handler definition for lf_mem_alloc
 * @version 0.1
 * @date 2023-07-02
 *
 * @copyright Copyright (c) 2023 Teruaki Ata <PFA03027@nifty.com>
 *
 */

#ifndef LF_MEM_ALLOC_SLOT_HPP_
#define LF_MEM_ALLOC_SLOT_HPP_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
#include "alconcurrent/conf_logger.hpp"
#endif

#include "alconcurrent/lf_mem_alloc_type.hpp"

namespace alpha {
namespace concurrent {

namespace internal {
////////////////////////////////////////////////////////////////////////////////////////////////////////

constexpr unsigned char tail_padding_byte_v = 1U;

#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
inline constexpr uintptr_t make_maker_value( uintptr_t offset_v )
{
	return ( static_cast<uintptr_t>( 0 ) - offset_v - static_cast<uintptr_t>( 1 ) );   // intentionally saturating arithmetic operation
}
inline constexpr bool check_marker_func( uintptr_t offset_v, uintptr_t marker_v )
{
	// 今は簡単な仕組みで実装する
#if ( __cpp_constexpr >= 201304 )
	std::uintptr_t tmp = offset_v;
	tmp += marker_v + 1;   // intentionally saturating arithmetic operation

	bool ans = ( tmp == 0 );
	return ans;
#else
	return ( ( offset_v + ( marker_v + 1 ) ) == 0 );
#endif
}
#endif

class slot_header_of_array;
union unified_slot_header;
class slot_container;
class slot_array_mgr;

struct bool_size_t {
	bool   is_ok_;
	size_t idx_;
};

struct bool_unified_slot_header_p {
	bool                 is_ok_;
	unified_slot_header* p_ush_;
};

/**
 * @brief slot main header
 *
 */
struct slot_mheader {
	const std::atomic<uintptr_t> offset_to_mgr_;   //!< offset to slot array manager header
#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
	const std::atomic<uintptr_t> marker_;   //!< check sum maker value
#endif
	std::atomic<uintptr_t> offset_to_tail_padding_;   //!< offset to tail padding. if this value is zero, it means this slot is not used now.
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	bt_info alloc_bt_info_;   //!< backtrace information when is allocated
	bt_info free_bt_info_;    //!< backtrace information when is free
#endif

	constexpr slot_mheader( std::uintptr_t offset_to_mgr_arg )
	  : offset_to_mgr_( offset_to_mgr_arg )
#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
	  , marker_( make_maker_value( offset_to_mgr_arg ) )
#endif
	  , offset_to_tail_padding_( 0 )
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	  , alloc_bt_info_()   //!< backtrace information when is allocated
	  , free_bt_info_()    //!< backtrace information when is free
#endif
	{
	}

	slot_mheader( void* p_mgr_arg )
	  : offset_to_mgr_( make_offset_mgr_to_value( p_mgr_arg, reinterpret_cast<void*>( this ) ) )   // intentionally saturating arithmetic operation
#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
	  , marker_( make_maker_value( make_offset_mgr_to_value( p_mgr_arg, reinterpret_cast<void*>( this ) ) ) )
#endif
	  , offset_to_tail_padding_( 0 )
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	  , alloc_bt_info_()   //!< backtrace information when is allocated
	  , free_bt_info_()    //!< backtrace information when is free
#endif
	{
	}

	slot_array_mgr* get_mgr_pointer( void ) const
	{
#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
		if ( check_marker() ) {
			return reinterpret_cast<slot_array_mgr*>( reinterpret_cast<uintptr_t>( this ) + offset_to_mgr_.load( std::memory_order_acquire ) );   // intentionally saturating arithmetic operation
		} else {
			return nullptr;
		}
#else
		return reinterpret_cast<slot_array_mgr*>( reinterpret_cast<uintptr_t>( this ) + offset_to_mgr_.load( std::memory_order_acquire ) );   // intentionally saturating arithmetic operation
#endif
	}

#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
	inline bool check_marker( void ) const
	{
		return check_marker_func(
			offset_to_mgr_.load( std::memory_order_acquire ),
			marker_.load( std::memory_order_acquire ) );
	}
#endif

	std::string dump_string( void );

private:
	static constexpr std::uintptr_t make_offset_mgr_to_value( void* p_mgr_arg, void* p_this )
	{
		return reinterpret_cast<uintptr_t>( p_mgr_arg ) + ( static_cast<uintptr_t>( 0 ) - reinterpret_cast<uintptr_t>( p_this ) );   // intentionally saturating arithmetic operation
	}
};

static_assert( std::is_standard_layout<slot_mheader>::value, "slot_mheader should be standard-layout type" );
static_assert( ( sizeof( slot_mheader ) % default_slot_alignsize ) == 0, "slot_mheader must be a constant multiple of default_slot_alignsize" );

/**
 * @brief slot sub header for slot array
 *
 */
struct array_slot_sheader {
	std::atomic<slot_header_of_array*> p_next_;   //!< stackリストとして繋がる次のslot_header_of_arrayへのポインタ

	constexpr array_slot_sheader( slot_header_of_array* p_next_arg = nullptr )
	  : p_next_( p_next_arg )
	{
	}

	std::string dump_string( void );
};

static_assert( std::is_standard_layout<array_slot_sheader>::value, "array_slot_sheader should be standard-layout type" );
static_assert( ( sizeof( array_slot_sheader ) % default_slot_alignsize ) == 0, "array_slot_sheader must be a constant multiple of default_slot_alignsize" );

/**
 * @brief slot sub header for individual allocated slot
 *
 */
struct alloc_slot_sheader {
	size_t alloc_size_;   //!< allocation size of this slot

	constexpr alloc_slot_sheader( void )
	  : alloc_size_( 0 )
	{
	}

	constexpr alloc_slot_sheader( size_t alloc_size_arg )
	  : alloc_size_( alloc_size_arg )
	{
	}

	std::string dump_string( void );
};

static_assert( std::is_standard_layout<alloc_slot_sheader>::value, "alloc_slot_sheader should be standard-layout type" );
static_assert( ( sizeof( alloc_slot_sheader ) % default_slot_alignsize ) == 0, "alloc_slot_sheader must be a constant multiple of default_slot_alignsize" );

/**
 * @brief slot header of slot array
 *
 * this class is used for slot_array_mgr
 */
struct slot_header_of_array {
	slot_mheader       mh_;   //!< main header
	array_slot_sheader sh_;   //!< sub header

#if ( __cpp_constexpr >= 201304 )
	constexpr
#endif
		slot_header_of_array( uintptr_t offset_to_mgr_arg )
	  : mh_( offset_to_mgr_arg )
	  , sh_()
	{
		if ( offset_to_mgr_arg == 0 ) {
			throw std::runtime_error( "offset_to_mgr_arg must not be 0(Zero)" );
		}
	}

	slot_header_of_array( void* p_mgr_arg, slot_header_of_array* p_next_arg )
	  : mh_( p_mgr_arg )
	  , sh_( p_next_arg )
	{
	}

	void* allocate( slot_container* p_container_top, size_t container_size, size_t n, size_t req_alignsize );
	void  deallocate( void );

	slot_header_of_array* get_next( void )
	{
		return sh_.p_next_.load( std::memory_order_acquire );
	}

	void set_next( slot_header_of_array* p_new_next )
	{
		sh_.p_next_.store( p_new_next, std::memory_order_release );
		return;
	}

	bool next_CAS( slot_header_of_array** pp_expect_ptr, slot_header_of_array* p_desired_ptr )
	{
		return sh_.p_next_.compare_exchange_weak( *pp_expect_ptr, p_desired_ptr, std::memory_order_acq_rel );
	}

	void dump( int indent = 0 );
};

static_assert( std::is_standard_layout<slot_header_of_array>::value, "slot_header_of_array should be standard-layout type" );
static_assert( ( sizeof( slot_header_of_array ) % default_slot_alignsize ) == 0, "slot_header_of_array must be a constant multiple of default_slot_alignsize" );

/**
 * @brief slot header of individual allocated slot
 *
 * this class should be constructed by like below;
 * @code {.cpp}
 * size_t buff_size = slot_header_of_alloc::calc_slot_header_and_container_size( allocation required size, alignment required size );
 * void* p_allocated_slot = malloc(buff_size);
 * slot_header_of_alloc* p_slot_header_of_alloc = new (p_allocated_slot) slot_header_of_alloc(buff_size);
 * @endcode
 *
 * then, please get the address for allocation memory by like below;
 * @code {.cpp}
 * void* p_mem = p_slot_header_of_alloc->allocate( allocation required size, alignment required size );
 * // using memory
 * // ...
 * @endcode
 *
 * finish using, then de-allocate memory
 * @code {.cpp}
 * p_slot_header_of_alloc->deallocate();
 * @endcode
 *
 * or
 * @code {.cpp}
 * auto check_result = slot_container::get_slot_header_from_assignment_p(p_mem);
 * if( check_result.is_ok_ ) {
 *     if( check_result.p_ush_->check_type() ) {
 *         check_result.p_ush_->alloch_.deallocate();
 *         free(check_result.p_ush_);
 *     }
 * }
 * @endcode
 *
 */
struct slot_header_of_alloc {
	slot_mheader       mh_;                         //!< main header
	alloc_slot_sheader sh_;                         //!< sub header
	unsigned char      slot_container_buffer_[0];   //!< padding and slot container buffer for alignment

	constexpr slot_header_of_alloc( size_t alloc_size_arg )
	  : mh_( static_cast<std::uintptr_t>( 0 ) )
	  , sh_( alloc_size_arg )
	  , slot_container_buffer_ {}
	{
	}

	void* allocate( size_t n, size_t req_alignsize );
	void  deallocate( void );

	void dump( int indent = 0 );

#if ( __cpp_constexpr >= 201304 )
	static constexpr size_t calc_slot_header_and_container_size( size_t n, size_t req_alignsize );
#else
	static inline size_t calc_slot_header_and_container_size( size_t n, size_t req_alignsize );
#endif
};

static_assert( std::is_standard_layout<slot_header_of_alloc>::value, "slot_header_of_alloc should be standard-layout type" );
static_assert( ( sizeof( slot_header_of_alloc ) % default_slot_alignsize ) == 0, "slot_header_of_alloc must be a constant multiple of default_slot_alignsize" );

/**
 * @brief union of slot headers
 *
 */
union unified_slot_header {
	slot_mheader         mh_;       //!< main header of common
	slot_header_of_array arrayh_;   //!< slot header of array slot type
	slot_header_of_alloc alloch_;   //!< slot header of individual allocated slot type

	unified_slot_header( void )
	  : mh_( static_cast<std::uintptr_t>( 0 ) )
	{
	}

	/**
	 * @brief check type
	 *
	 * @return true type is slot_header_of_alloc
	 * @return false type is slot_header_of_array
	 */
	bool check_type( void ) const
	{
		if ( mh_.offset_to_mgr_.load( std::memory_order_acquire ) == 0 ) {
			return true;
		} else {
			return false;
		}
	}
};

static_assert( std::is_standard_layout<unified_slot_header>::value, "unified_slot_header should be standard-layout type" );

/**
 * @brief container structure of slot
 *
 */
struct slot_container {
	const std::atomic<uintptr_t> back_offset_;   //!< offset to unified_slot_header
#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
	const std::atomic<uintptr_t> back_offset_marker_;   //!< check sum maker value
#endif
	unsigned char mem[0];   //!< assignment memory area
	                        // there is no definition of tail_padding

	constexpr slot_container( uintptr_t back_offset_arg )
	  : back_offset_( back_offset_arg )
#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
	  , back_offset_marker_( make_maker_value( back_offset_arg ) )
#endif
	  , mem {}
	{
	}

#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
	inline bool check_marker( void ) const
	{
		return check_marker_func(
			back_offset_.load( std::memory_order_acquire ),
			back_offset_marker_.load( std::memory_order_acquire ) );
	}
#endif

	static bool_unified_slot_header_p get_slot_header_from_assignment_p( void* p_mem );
#if ( __cpp_constexpr >= 201304 )
	static constexpr size_t calc_slot_container_size( size_t n, size_t req_alignsize );
#else
	static inline size_t calc_slot_container_size( size_t n, size_t req_alignsize );
#endif
	static void* construct_slot_container_in_container_buffer( slot_mheader* p_bind_mh_of_slot, slot_container* p_container_top, size_t container_size, size_t n, size_t req_align );
};

static_assert( std::is_standard_layout<slot_container>::value, "slot_container should be standard-layout type" );
static_assert( ( sizeof( slot_container ) % default_slot_alignsize ) == 0, "slot_container must be a constant multiple of default_slot_alignsize" );

#if ( __cpp_constexpr >= 201304 )
constexpr
#else
inline
#endif
	size_t
	slot_container::calc_slot_container_size( size_t n, size_t req_alignsize )
{
	// slot container header + tail_padding + n bytes
	size_t tfit_req_alignsize = ( req_alignsize > default_slot_alignsize ) ? req_alignsize : default_slot_alignsize;
	size_t base_ans           = sizeof( slot_container ) + tfit_req_alignsize + n;
	size_t mx                 = base_ans / default_slot_alignsize;   // TODO: ビットマスクを使った演算で多分軽量化できるが、まずは真面目に計算する。
	size_t rx                 = base_ans % default_slot_alignsize;   // ただ、ここは、default_slot_alignsizeは定数かつ2^n上である数値なので、コンパイラの最適化がかかっていると思われる。
	size_t ans                = mx * default_slot_alignsize + ( ( rx == 0 ) ? 0 : default_slot_alignsize );
	return ans;
}

#if ( __cpp_constexpr >= 201304 )
constexpr
#else
inline
#endif
	size_t
	slot_header_of_alloc::calc_slot_header_and_container_size( size_t n, size_t req_alignsize )
{
	return sizeof( slot_header_of_alloc ) + slot_container::calc_slot_container_size( n, req_alignsize );
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
