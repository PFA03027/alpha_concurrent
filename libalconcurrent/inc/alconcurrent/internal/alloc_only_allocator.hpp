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

#ifndef ALCONCURRENT_INC_INTERNAL_ALLOC_ONLY_ALLOCATOR_HPP_
#define ALCONCURRENT_INC_INTERNAL_ALLOC_ONLY_ALLOCATOR_HPP_

#include <cstdint>
#include <cstdlib>

#include <atomic>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/internal/cpp_std_configure.hpp"

namespace alpha {
namespace concurrent {

////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace internal {

////////////////////////////////////////////////////////////////////////////////////////////////////////
// internal I/F
constexpr size_t default_align_size = 32;   // default_align_size should be power of 2

class alloc_chamber;

struct alloc_chamber_statistics {
	using print_string_t = fixedbuff_string<1024>;

	size_t chamber_count_;
	size_t alloc_size_;
	size_t consum_size_;
	size_t free_size_;
	size_t num_of_allocated_;
	size_t num_of_using_allocated_;
	size_t num_of_released_allocated_;

	constexpr alloc_chamber_statistics( void )
	  : chamber_count_( 0 )
	  , alloc_size_( 0 )
	  , consum_size_( 0 )
	  , free_size_( 0 )
	  , num_of_allocated_( 0 )
	  , num_of_using_allocated_( 0 )
	  , num_of_released_allocated_( 0 )
	{
	}

	alloc_chamber_statistics& operator+=( const alloc_chamber_statistics& op ) noexcept;

	print_string_t print( void ) const noexcept;
};

class alloc_only_chamber {
public:
	enum class validity_status {
		kInvalid,
		kUsed,
		kReleased
	};

	constexpr alloc_only_chamber( bool need_release_munmap_arg, size_t pre_alloc_size_arg ) noexcept
	  : head_( nullptr )
	  , one_try_hint_( nullptr )
	  , need_release_munmap_( need_release_munmap_arg )
	  , pre_alloc_size_( pre_alloc_size_arg )
	{
	}

	~alloc_only_chamber();

	ALCC_INTERNAL_NODISCARD_ATTR inline void* allocate( size_t req_size, size_t req_align ) noexcept
	{
		if ( !is_power_of_2( req_align ) ) {
			LogOutput( log_type::WARN, "ignore req_align, becuase req_align is not power of 2. req_align is %zu, 0x%zX", req_align, req_align );
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION
			std::terminate();
#else
			static_assert( is_power_of_2( default_align_size ), "default_align_size should be power of 2" );
			req_align = default_align_size;
#endif
		}
		return chked_allocate( req_size, req_align );
	}

	template <size_t REQ_ALIGN>
	ALCC_INTERNAL_NODISCARD_ATTR inline void* allocate( size_t req_size ) noexcept
	{
		static_assert( is_power_of_2( REQ_ALIGN ), "REQ_ALIGN should be power of 2" );
		return chked_allocate( req_size, REQ_ALIGN );
	}

	ALCC_INTERNAL_NODISCARD_ATTR inline void* allocate( size_t req_size ) noexcept
	{
		return allocate<default_align_size>( req_size );
	}

	/**
	 * @brief Marks the area as deallocated
	 *
	 * This API just marks as deallocated. then it will be possible to detect double free that is unexpected.
	 */
	static void deallocate( void* p_mem ) noexcept;

	bool is_belong_to_this( void* p_mem ) const noexcept;

	alloc_chamber_statistics get_statistics( void ) const noexcept;

	void dump_to_log( log_type lt, char c, int id ) const noexcept;

	/**
	 * @brief inspect using memory
	 *
	 * @param flag_with_dump_to_log true: if using memory found, dump its information to log. false: no log dump
	 * @return size_t number of allocated and still using memory area
	 */
	size_t inspect_using_memory( bool flag_with_dump_to_log = false, log_type lt = log_type::DEBUG, char c = 'a', int id = 0 ) const noexcept;

	/**
	 * @brief Check p_mem belong to alloc_only_chamber, and is still used or already released.
	 */
	static validity_status verify_validity( void* p_mem ) noexcept;

private:
	void* chked_allocate( size_t req_size, size_t req_align ) noexcept;
	void* try_allocate( size_t req_size, size_t req_align ) noexcept;
	void  push_alloc_mem( void* p_alloced_mem, size_t allocated_size ) noexcept;
	void  munmap_alloc_chamber( alloc_chamber* p_ac ) noexcept;

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
