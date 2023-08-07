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

namespace alpha {
namespace concurrent {

namespace internal {
////////////////////////////////////////////////////////////////////////////////////////////////////////

constexpr size_t default_slot_alignsize = sizeof( std::uintptr_t );

class slot_header_of_array;

/**
 * @brief slot main header
 *
 */
struct slot_mheader {
	std::atomic<std::uintptr_t> offset_to_mgr_;            //!< offset to slot array manager header
#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
	std::atomic<std::uintptr_t> marker_;                   //!< check sum maker value
#endif
	std::atomic<std::uintptr_t> offset_to_tail_padding_;   //!< offset to tail padding

	constexpr slot_mheader( std::uintptr_t offset_to_mgr_arg = 0 )
	  : offset_to_mgr_( offset_to_mgr_arg )
#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
	  , marker_( make_maker_value( offset_to_mgr_arg ) )
#endif
	  , offset_to_tail_padding_( 0 )
	{
	}

	slot_mheader( void* p_mgr_arg )
	  : offset_to_mgr_( make_offset_mgr_to_value( p_mgr_arg, reinterpret_cast<void*>( this ) ) )   // intentionally saturating arithmetic operation
#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
	  , marker_( make_maker_value( make_offset_mgr_to_value( p_mgr_arg, reinterpret_cast<void*>( this ) ) ) )
#endif
	  , offset_to_tail_padding_( 0 )
	{
	}

	template <typename T>
	T* get_mgr_pointer( void ) const
	{
		return reinterpret_cast<T*>( reinterpret_cast<uintptr_t>( this ) + offset_to_mgr_.load( std::memory_order_acquire ) );   // intentionally saturating arithmetic operation
	}

#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
	bool check_marker( void ) const
	{
		// 今は簡単な仕組みで実装する
		std::uintptr_t tmp = offset_to_mgr_.load( std::memory_order_acquire );
		tmp += marker_.load( std::memory_order_acquire ) + 1;   // intentionally saturating arithmetic operation

		return ( tmp == 0 );
	}
#endif

private:
	static constexpr std::uintptr_t make_offset_mgr_to_value( void* p_mgr_arg, void* p_this )
	{
		return reinterpret_cast<uintptr_t>( p_mgr_arg ) + ( static_cast<uintptr_t>( 0 ) - reinterpret_cast<uintptr_t>( p_this ) );   // intentionally saturating arithmetic operation
	}
#ifdef ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
	static constexpr std::uintptr_t make_maker_value( std::uintptr_t offset_v )
	{
		return ( static_cast<uintptr_t>( 0 ) - offset_v - static_cast<uintptr_t>( 1 ) );   // intentionally saturating arithmetic operation
	}
#endif
};

static_assert( std::is_standard_layout<slot_mheader>::value, "slot_mheader should be standard-layout type" );

/**
 * @brief slot sub header for slot array
 *
 */
struct array_slot_sheader {
	std::atomic<slot_header_of_array*> p_next_;   //!< stackリストとして繋がる次のslot_header_of_arrayへのポインタ
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	bt_info alloc_bt_info_;                       //!< backtrace information when is allocated
	bt_info free_bt_info_;                        //!< backtrace information when is free
#endif
	unsigned char padding_for_alignment_[0];      //!< padding buffer for alignment

	constexpr array_slot_sheader( slot_header_of_array* p_next_arg = nullptr )
	  : p_next_( p_next_arg )
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	  , alloc_bt_info_()   //!< backtrace information when is allocated
	  , free_bt_info_()    //!< backtrace information when is free
#endif
	  , padding_for_alignment_ {}
	{
	}
};

static_assert( std::is_standard_layout<array_slot_sheader>::value, "array_slot_sheader should be standard-layout type" );

/**
 * @brief slot sub header for individual allocated slot
 *
 */
struct alloc_slot_sheader {
	size_t alloc_size_;                        //!< allocation size of this slot
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	bt_info alloc_bt_info_;                    //!< backtrace information when is allocated
	bt_info free_bt_info_;                     //!< backtrace information when is free
#endif
	unsigned char padding_for_alignment_[0];   //!< padding buffer for alignment

	constexpr alloc_slot_sheader( void )
	  : alloc_size_( 0 )
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	  , alloc_bt_info_()   //!< backtrace information when is allocated
	  , free_bt_info_()    //!< backtrace information when is free
#endif
	  , padding_for_alignment_ {}
	{
	}

	constexpr alloc_slot_sheader( size_t alloc_size_arg )
	  : alloc_size_( alloc_size_arg )
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	  , alloc_bt_info_()   //!< backtrace information when is allocated
	  , free_bt_info_()    //!< backtrace information when is free
#endif
	  , padding_for_alignment_ {}
	{
	}
};

static_assert( std::is_standard_layout<alloc_slot_sheader>::value, "alloc_slot_sheader should be standard-layout type" );

/**
 * @brief slot header of slot array
 *
 */
struct slot_header_of_array {
	slot_mheader       mh_;   //!< main header
	array_slot_sheader sh_;   //!< sub header

	constexpr slot_header_of_array( std::uintptr_t offset_to_mgr_arg = 0 )
	  : mh_( offset_to_mgr_arg )
	  , sh_()
	{
	}

	slot_header_of_array( void* p_mgr_arg, slot_header_of_array* p_next_arg )
	  : mh_( p_mgr_arg )
	  , sh_( p_next_arg )
	{
	}

	void* allocate( size_t alloc_size, size_t n, size_t req_alignsize );
	// void  deallocate( void );
};

static_assert( std::is_standard_layout<slot_header_of_array>::value, "slot_header_of_array should be standard-layout type" );

/**
 * @brief slot header of individual allocated slot
 *
 */
struct slot_header_of_alloc {
	slot_mheader       mh_;   //!< main header
	alloc_slot_sheader sh_;   //!< sub header

	constexpr slot_header_of_alloc( size_t alloc_size_arg )
	  : mh_( static_cast<std::uintptr_t>( 0 ) )
	  , sh_( alloc_size_arg )
	{
	}

	void* allocate( size_t alloc_size, size_t n, size_t req_alignsize );
	// void  deallocate( void );
};

static_assert( std::is_standard_layout<slot_header_of_alloc>::value, "slot_header_of_alloc should be standard-layout type" );

/**
 * @brief union of slot headers
 *
 */
union unified_slot_header {
	slot_mheader         mh_;       //!< main header of common
	slot_header_of_array arrayh_;   //!< slot header of array slot type
	slot_header_of_alloc alloch_;   //!< slot header of individual allocated slot type

	unified_slot_header( void )
	  : mh_()
	{
	}

	static unified_slot_header* get_slot_header_from_assignment_p( void* p_mem );
};

/**
 * @brief answer value structure of calc_addr_info_of_slot_of()
 *
 */
struct addr_info_of_slot {
	bool           is_success_;                        //!< if true, success to calculation
	uintptr_t*     p_back_offset_;                     //!< pointer to back_offset
	uintptr_t      value_of_back_offset_;              //!< value of back_offset
	void*          p_assignment_area_;                 //!< pointer to assignment area
	uintptr_t      value_of_offset_to_tail_padding_;   //!< value of offset_to_tail_padding_
	unsigned char* p_tail_padding_;                    //!< pointer to offset_to_tail_padding_
	size_t         tail_padding_size_;                 //!< tail padding size
};

/**
 * @brief slotを構築する上で必要となる各種のアドレス情報を算出する
 *
 * @warning アライメント要求値は、2のn乗値を暗黙的に期待している。
 *
 * @note
 * 上記のアライメント要求値の期待制約を取り外す場合、アライメント要求値値とsizeof( std::max_align_t )との最小公倍数で、アライメントを行う実装が必要。
 * 具体的には、sel_alignmentsizeの算出において、アライメント要求値値とsizeof( std::max_align_t )との最小公倍数を算出する必要がある。
 *
 * @tparam SLOT_H_T slot header type. slot_header_of_array or slot_header_of_alloc
 * @param p_alloc_top pointer of allocated memory
 * @param alloc_size size of allocated memory
 * @param n size of assignment required
 * @param req_alignsize alignment size of assignment required
 * @return addr_info_of_slot result of the calculation addresses
 */
template <typename SLOT_H_T>
inline addr_info_of_slot calc_addr_info_of_slot_of( void* p_alloc_top, size_t alloc_size, size_t n, size_t req_alignsize )
{
	size_t    min_base_size      = sizeof( SLOT_H_T ) + sizeof( std::uintptr_t );   // header + back_offset
	uintptr_t min_base_addr      = reinterpret_cast<uintptr_t>( p_alloc_top ) + min_base_size;
	uintptr_t tfit_req_alignsize = static_cast<uintptr_t>( req_alignsize );
	uintptr_t mx                 = min_base_addr / tfit_req_alignsize;
	uintptr_t rx                 = min_base_addr % tfit_req_alignsize;
	uintptr_t ans_addr           = tfit_req_alignsize * mx + ( ( rx == 0 ) ? 0 : tfit_req_alignsize );
	uintptr_t addr_end_of_alloc  = reinterpret_cast<uintptr_t>( p_alloc_top ) + static_cast<uintptr_t>( alloc_size );
	uintptr_t addr_end_of_assign = ans_addr + static_cast<uintptr_t>( n );
#if 0
	printf( "p_alloc_top:		%p\n", p_alloc_top );
	printf( "alloc_size:		%zu, 0x%zx\n", alloc_size, alloc_size );
	printf( "n:			%zu\n", n );
	printf( "req_alignsize:		%zu\n", req_alignsize );
	printf( "min_base_size:		%zu\n", min_base_size );
	printf( "min_base_addr:		%zx\n", min_base_addr );
	printf( "tfit_req_alignsize:	%zu\n", tfit_req_alignsize );
	printf( "mx:			%zu\n", mx );
	printf( "rx:			%zu\n", rx );
	printf( "ans_addr: 		%zx\n", ans_addr );
	printf( "addr_end_of_alloc: 	%zx\n", addr_end_of_alloc );
	printf( "addr_end_of_assign: 	%zx\n", addr_end_of_assign );
#endif
	if ( addr_end_of_assign >= addr_end_of_alloc ) {
		// tail_paddingのサイズ分を考慮するために >= で比較している。
		return addr_info_of_slot { false, nullptr, 0, nullptr, 0, nullptr, 0 };
	}

	size_t ans_tail_padding_size = static_cast<size_t>( ( reinterpret_cast<uintptr_t>( p_alloc_top ) + static_cast<uintptr_t>( alloc_size ) ) - ( ans_addr + static_cast<uintptr_t>( n ) ) );
	if ( ans_tail_padding_size > ( req_alignsize + default_slot_alignsize ) ) {
		std::string errlog = "fail the tail padding size calculation: ";
		errlog += std::to_string( ans_tail_padding_size );
		errlog += std::string( "  expected is smaller or equal to " );
		errlog += std::to_string( req_alignsize + default_slot_alignsize );
		throw std::logic_error( errlog );
	}
	uintptr_t*     p_back_offsetX = reinterpret_cast<uintptr_t*>( ans_addr - static_cast<uintptr_t>( sizeof( uintptr_t ) ) );
	unsigned char* p_tail_padding = reinterpret_cast<unsigned char*>( ans_addr + static_cast<uintptr_t>( n ) );
	return addr_info_of_slot {
		true,
		p_back_offsetX,
		reinterpret_cast<uintptr_t>( p_alloc_top ) + ( static_cast<uintptr_t>( 0 ) - reinterpret_cast<uintptr_t>( p_back_offsetX ) ),
		reinterpret_cast<void*>( ans_addr ),
		reinterpret_cast<uintptr_t>( p_tail_padding ) - reinterpret_cast<uintptr_t>( p_alloc_top ),
		p_tail_padding,
		ans_tail_padding_size };
}

/**
 * @brief 割り当てしてほしいサイズとアライメント要求値をもとに、slotサイズを求める。
 *
 * @warning アライメント要求値は、2のn乗値を暗黙的に期待している。
 *
 * @note
 * 上記のアライメント要求値の期待制約を取り外す場合、アライメント要求値値とsizeof( std::max_align_t )との最小公倍数で、アライメントを行う実装が必要。
 * 具体的には、sel_alignmentsizeの算出において、アライメント要求値値とsizeof( std::max_align_t )との最小公倍数を算出する必要がある。
 *
 * @tparam SLOT_H_T slot header type. slot_header_of_array or slot_header_of_alloc
 * @param n size of assignment required
 * @param req_alignsize alignment size of assignment required
 * @return size_t slot size
 */
template <typename SLOT_H_T>
inline constexpr size_t calc_total_slot_size_of_slot_header_of( size_t n, size_t req_alignsize )
{
	size_t min_base_size = sizeof( SLOT_H_T ) + sizeof( std::uintptr_t );   // header + back_offset + n + ajusting size of alignment + minimum tail_padding
	size_t h_n_align     = min_base_size + n + req_alignsize - 1;
	size_t mx            = h_n_align / default_slot_alignsize;
	size_t base_ans      = default_slot_alignsize * ( mx + 1 );   // add tail padding area
#if 0
	printf( "min_base_size:		%zu\n", min_base_size );
	printf( "h_n_align:		%zu\n", h_n_align );
	printf( "mx:			%zu\n", mx );
	printf( "base_ans:		%zu\n", base_ans );
#endif
	return base_ans;
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
