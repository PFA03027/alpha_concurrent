/**
 * @file mem_gmem_allocator.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-22
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "alconcurrent/lf_mem_alloc.hpp"
#include "mem_big_memory_slot.hpp"
#include "mem_retrieved_slot_array_mgr.hpp"
#include "mem_small_memory_slot.hpp"

namespace alpha {
namespace concurrent {

internal::memory_slot_group_list g_memory_slot_group_list_array[] = {
	{ 8, 4096, 1048576, 0 },
	{ 16, 4096, 1048576, 1 },
	{ 24, 4096, 1048576, 2 },
	{ 32, 4096, 1048576, 3 },
	{ 40, 8192, 1048576, 4 },
	{ 48, 8192, 1048576, 5 },
	{ 56, 8192, 1048576, 6 },
	{ 64, 12288, 1048576, 7 },
	{ 72, 12288, 1048576, 8 },
	{ 80, 12288, 1048576, 9 },
	{ 88, 12288, 1048576, 10 },
	{ 96, 16384, 1048576, 11 },
	{ 104, 16384, 1048576, 12 },
	{ 112, 16384, 1048576, 13 },
	{ 120, 16384, 1048576, 14 },
	{ 128, 20480, 1048576, 15 },
	{ 136, 20480, 1048576, 16 },
	{ 144, 20480, 1048576, 17 },
	{ 152, 20480, 1048576, 18 },
	{ 160, 24576, 1048576, 19 },
	{ 168, 24576, 1048576, 20 },
	{ 176, 24576, 1048576, 21 },
	{ 184, 24576, 1048576, 22 },
	{ 192, 28672, 1048576, 23 },
	{ 200, 28672, 1048576, 24 },
	{ 208, 28672, 1048576, 25 },
	{ 216, 28672, 1048576, 26 },
	{ 224, 32768, 1048576, 27 },
	{ 232, 32768, 1048576, 28 },
	{ 240, 32768, 1048576, 29 },
	{ 248, 32768, 1048576, 30 },
	{ 256, 36864, 1048576, 31 },
	{ 264, 36864, 1048576, 32 },
	{ 272, 36864, 1048576, 33 },
	{ 280, 36864, 1048576, 34 },
	{ 288, 40960, 1048576, 35 },
	{ 296, 40960, 1048576, 36 },
	{ 304, 40960, 1048576, 37 },
	{ 312, 40960, 1048576, 38 },
	{ 320, 45056, 1048576, 39 },
	{ 328, 45056, 1048576, 40 },
	{ 336, 45056, 1048576, 41 },
	{ 344, 45056, 1048576, 42 },
	{ 352, 49152, 1048576, 43 },
	{ 360, 49152, 1048576, 44 },
	{ 368, 49152, 1048576, 45 },
	{ 376, 49152, 1048576, 46 },
	{ 384, 53248, 1048576, 47 },
	{ 392, 53248, 1048576, 48 },
	{ 400, 53248, 1048576, 49 },
	{ 408, 53248, 1048576, 50 },
	{ 416, 57344, 1048576, 51 },
	{ 424, 57344, 1048576, 52 },
	{ 432, 57344, 1048576, 53 },
	{ 440, 57344, 1048576, 54 },
	{ 448, 61440, 1048576, 55 },
	{ 456, 61440, 1048576, 56 },
	{ 464, 61440, 1048576, 57 },
	{ 472, 61440, 1048576, 58 },
	{ 480, 65536, 1048576, 59 },
	{ 488, 65536, 1048576, 60 },
	{ 496, 65536, 1048576, 61 },
	{ 504, 65536, 1048576, 62 },
	{ 512, 65536, 1048576, 63 },
	{ 576, 65536, 2097152, 64 },
	{ 640, 65536, 2097152, 65 },
	{ 704, 65536, 2097152, 66 },
	{ 768, 65536, 2097152, 67 },
	{ 832, 65536, 2097152, 68 },
	{ 896, 65536, 2097152, 69 },
	{ 960, 65536, 2097152, 70 },
	{ 1024, 65536, 4194304, 71 },
	{ 1088, 73728, 4194304, 72 },
	{ 1152, 77824, 4194304, 73 },
	{ 1216, 81920, 4194304, 74 },
	{ 1280, 86016, 4194304, 75 },
	{ 1344, 90112, 4194304, 76 },
	{ 1408, 94208, 4194304, 77 },
	{ 1472, 98304, 4194304, 78 },
	{ 1536, 102400, 4194304, 79 },
	{ 1600, 106496, 4194304, 80 },
	{ 1664, 110592, 4194304, 81 },
	{ 1728, 114688, 4194304, 82 },
	{ 1792, 118784, 4194304, 83 },
	{ 1856, 122880, 4194304, 84 },
	{ 1920, 126976, 4194304, 85 },
	{ 1984, 131072, 4194304, 86 },
	{ 2048, 135168, 4194304, 87 },
	{ 2176, 143360, 4194304, 88 },
	{ 2304, 151552, 4194304, 89 },
	{ 2432, 159744, 4194304, 90 },
	{ 2560, 167936, 4194304, 91 },
	{ 2688, 176128, 4194304, 92 },
	{ 2816, 184320, 4194304, 93 },
	{ 2944, 192512, 4194304, 94 },
	{ 3072, 200704, 4194304, 95 },
	{ 3200, 200704, 4194304, 96 },
	{ 3328, 200704, 4194304, 97 },
	{ 3456, 200704, 4194304, 98 },
	{ 3584, 200704, 4194304, 99 },
	{ 3712, 200704, 4194304, 100 },
	{ 3840, 200704, 4194304, 101 },
	{ 3968, 200704, 4194304, 102 },
	{ 4096, 200704, 4194304, 103 },
	{ 4352, 200704, 4194304, 104 },
	{ 4608, 200704, 4194304, 105 },
	{ 4864, 200704, 4194304, 106 },
	{ 5120, 200704, 4194304, 107 },
	{ 5376, 200704, 4194304, 108 },
	{ 5632, 200704, 4194304, 109 },
	{ 5888, 200704, 4194304, 110 },
	{ 6144, 200704, 4194304, 111 },
	{ 6656, 200704, 4194304, 112 },
	{ 7168, 200704, 4194304, 113 },
	{ 7680, 200704, 4194304, 114 },
	{ 8192, 200704, 4194304, 115 },
	{ 9216, 200704, 4194304, 116 },
	{ 10240, 200704, 4194304, 117 },
	{ 12288, 200704, 4194304, 118 },
	{ 14336, 233472, 4194304, 119 },
	{ 16384, 233472, 4194304, 120 },
	{ 20480, 233472, 4194304, 121 },
	{ 24576, 233472, 4194304, 122 },
	{ 32768, 266240, 4194304, 123 },
	{ 49152, 397312, 4194304, 124 },
	{ 65536, 528384, 4194304, 125 },
	{ 98304, 790528, 4194304, 126 },
	{ 131072, 528384, 4194304, 127 },

	//
};

internal::big_memory_slot_list g_big_memory_slot_list;

/*!
 * @brief	allocate memory
 *
 * This I/F allocates a memory from a global general_mem_allocator instance. @n
 * The allocated memory must free by gmem_deallocate().
 *
 * @note
 * This uses default_param_array and num_of_default_param_array as initial allocation parameter
 *
 * @exception
 * If req_align is not power of 2, throw std::logic_error.
 */
void* gmem_allocate_impl(
	size_t n,          //!< [in] memory size to allocate
	size_t req_align   //!< [in] requested align size. req_align should be the power of 2
	) noexcept
{
	size_t needed_bytes = n;
	if ( req_align > internal::allocated_mem_top::min_alignment_size_ ) {
		needed_bytes += req_align - 1;
		if ( needed_bytes < n ) {
			internal::LogOutput( log_type::ERR, "overflow. requested bytes = %zu, requested align = %zu", n, req_align );
			return nullptr;
		}
	}

	const size_t array_count = sizeof( g_memory_slot_group_list_array ) / sizeof( g_memory_slot_group_list_array[0] );
	for ( size_t i = 0; i < array_count; ++i ) {
		if ( needed_bytes <= g_memory_slot_group_list_array[i].allocatable_bytes_ ) {
			internal::slot_link_info* p_slot = g_memory_slot_group_list_array[i].allocate();
			if ( p_slot == nullptr ) {
				g_memory_slot_group_list_array[i].request_allocate_memory_slot_group();
				p_slot = g_memory_slot_group_list_array[i].allocate();
			}
			if ( p_slot != nullptr ) {
				internal::allocated_mem_top* p_ans = p_slot->get_aligned_allocated_mem_top(
					req_align,
					n,
					internal::memory_slot_group::calc_one_slot_size( g_memory_slot_group_list_array[i].allocatable_bytes_ ) );
				return reinterpret_cast<void*>( p_ans->data_ );
			}
		}
	}

	internal::big_memory_slot* p_big_slot = g_big_memory_slot_list.reuse_allocate( needed_bytes );
	if ( p_big_slot == nullptr ) {
		p_big_slot = g_big_memory_slot_list.allocate_newly( needed_bytes );
	}
	if ( p_big_slot != nullptr ) {
		internal::allocated_mem_top* p_ans = p_big_slot->get_aligned_allocated_mem_top( req_align, n );
		return reinterpret_cast<void*>( p_ans->data_ );
	}

	return nullptr;
}

ALCC_INTERNAL_NODISCARD_ATTR void* gmem_allocate(
	size_t n   //!< [in] memory size to allocate
	) noexcept
{
	return gmem_allocate_impl( n, sizeof( uintptr_t ) );
}

ALCC_INTERNAL_NODISCARD_ATTR void* gmem_allocate(
	size_t n,                                //!< [in] memory size to allocate
	size_t req_align = sizeof( uintptr_t )   //!< [in] requested align size. req_align should be the power of 2
)
{
	if ( !internal::is_power_of_2( req_align ) ) {
		internal::LogOutput( log_type::ERR, "req_align is not power of 2." );
		throw std::logic_error( "req_align is not power of 2." );
	}
	return gmem_allocate_impl( n, req_align );
}

bool gmem_deallocate(
	void* p_mem   //!< [in] pointer to free.
)
{
	if ( p_mem == nullptr ) {
		return false;
	}

	bool                         ans           = false;
	internal::allocated_mem_top* p_top         = internal::allocated_mem_top::get_structure_addr( p_mem );
	auto                         slot_info_tmp = p_top->load_allocation_info<void>();
	if ( slot_info_tmp.mt_ == internal::mem_type::SMALL_MEM ) {
		auto slot_info = p_top->load_allocation_info<internal::memory_slot_group>();
		auto idx       = slot_info.p_mgr_->get_slot_idx( p_mem );
		if ( idx < 0 ) {
			internal::LogOutput( log_type::ERR, "invalid slot index." );
			return false;
		}
		internal::slot_link_info* p_slot = slot_info.p_mgr_->get_slot_pointer( static_cast<size_t>( idx ) );
		if ( &( p_slot->link_to_memory_slot_group_ ) != p_top ) {
			p_top->fetch_set( false );
		}
		internal::memory_slot_group_list* p_mgr = slot_info.p_mgr_->p_list_mgr_;
		ans                                     = p_mgr->deallocate( p_slot );
	} else if ( ( slot_info_tmp.mt_ == internal::mem_type::BIG_MEM ) || ( slot_info_tmp.mt_ == internal::mem_type::OVER_BIG_MEM ) ) {
		auto slot_info = p_top->load_allocation_info<internal::big_memory_slot>();
		if ( &( slot_info.p_mgr_->link_to_big_memory_slot_ ) != p_top ) {
			p_top->fetch_set( false );
		}
		ans = g_big_memory_slot_list.deallocate( slot_info.p_mgr_ );
	} else {
		internal::LogOutput( log_type::ERR, "unknown slot type." );
		return false;
	}

	return ans;
}

}   // namespace concurrent
}   // namespace alpha
