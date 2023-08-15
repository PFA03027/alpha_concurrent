/*!
 * @file	lf_mem_alloc.hpp
 * @brief	semi lock-free memory allocator
 * @author	Teruaki Ata
 * @date	Created on 2021/05/12
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef INC_ALCONCURRENT_LF_MEM_ALLOC_HPP_
#define INC_ALCONCURRENT_LF_MEM_ALLOC_HPP_

#include <array>
#include <atomic>
#include <cstdlib>
#include <initializer_list>
#include <list>
#include <memory>

#include "conf_logger.hpp"

#include "alloc_only_allocator.hpp"
#include "lf_mem_alloc_internal.hpp"
#include "lf_mem_alloc_type.hpp"

namespace alpha {
namespace concurrent {

namespace internal {
class chunk_list;
void* general_mem_allocator_impl_allocate(
	unsigned int          pr_ch_size_arg,         //!< array size of chunk and param array
	internal::chunk_list* p_param_ch_array_arg,   //!< unique pointer to chunk and param array
	std::size_t           n_arg                   //!< [in] memory size to allocate
);
void general_mem_allocator_impl_deallocate(
	unsigned int          pr_ch_size_arg,         //!< array size of chunk and param array
	internal::chunk_list* p_param_ch_array_arg,   //!< unique pointer to chunk and param array
	void*                 p_mem                   //!< [in] pointer to allocated memory by allocate()
);
void general_mem_allocator_impl_prune(
	unsigned int          pr_ch_size_arg,         //!< array size of chunk and param array
	internal::chunk_list* p_param_ch_array_arg,   //!< unique pointer to chunk and param array
	std::atomic_bool&     exclusive_ctr_arg       //<! reference to exclusive control for prune
);
}   // namespace internal

/*!
 * @brief	semi lock-free memory allocator based on multi chunk size list
 *
 * If requested size is over max size that parameter has, just call malloc()
 */
template <size_t NUM_ENTRY>
class static_general_mem_allocator {
public:
	/*!
	 * @brief	constructor
	 */
	constexpr static_general_mem_allocator( void )
	  : allocating_only_allocator_( true, 4 * 1024 )
	  , pr_ch_size_( 0 )
	  , param_ch_array_impl {}
	  , param_ch_array_( nullptr )
	  , exclusive_ctl_of_prune_( false )
	{
		return;
	}

	/*!
	 * @brief	constructor
	 */
	template <typename... Args>
	constexpr static_general_mem_allocator( Args... args )
	  : allocating_only_allocator_( true, 4 * 1024 )
	  , pr_ch_size_( sizeof...( args ) )
	  , param_ch_array_impl { { std::forward<Args>( args ), &allocating_only_allocator_ }... }
	  , param_ch_array_( param_ch_array_impl )
	  , exclusive_ctl_of_prune_( false )
	{
#ifdef ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
#else
		static_assert( ( ... && ( std::is_same<typename std::remove_const<Args>::type, param_chunk_allocation>::value ) ), "constructor parameter is not same to param_chunk_allocation" );
		static_assert( NUM_ENTRY >= sizeof...( args ), "number of constructer paramter is over than NUM_ENTRY" );
#endif
		return;
	}

	/*!
	 * @brief	destructor
	 */
	~static_general_mem_allocator()
	{
		if ( ( param_ch_array_ != param_ch_array_impl ) && ( param_ch_array_ != nullptr ) ) {
			for ( unsigned int i = 0; i < pr_ch_size_; i++ ) {
				param_ch_array_[i].~chunk_list();
			}
			// allocating_only_allocator_.detect_unexpected_deallocate( reinterpret_cast<void*>( param_ch_array_ ) );
		}
	}

	/*!
	 * @brief	allocate memory
	 */
	void* allocate(
		std::size_t n   //!< [in] memory size to allocate
	)
	{
		return general_mem_allocator_impl_allocate( pr_ch_size_, param_ch_array_, n );
	}

	/*!
	 * @brief	deallocate memory
	 */
	void deallocate(
		void* p_mem   //!< [in] pointer to allocated memory by allocate()
	)
	{
		general_mem_allocator_impl_deallocate( pr_ch_size_, param_ch_array_, p_mem );
	}

	/*!
	 * @brief	free the deletable buffers
	 */
	void prune( void )
	{
		general_mem_allocator_impl_prune( pr_ch_size_, param_ch_array_, exclusive_ctl_of_prune_ );
	}

	/**
	 * @brief Setup by argument paramter
	 *
	 * If already setup, this call is ignored
	 */
	void setup_by_param(
		const param_chunk_allocation* p_param_array,   //!< [in] pointer to parameter array
		unsigned int                  num              //!< [in] array size
	)
	{
		if ( param_ch_array_ != nullptr ) {
			internal::LogOutput( log_type::WARN, "already setup. ignore this request." );
			return;
		}

		void* p_tmp     = allocating_only_allocator_.allocate( sizeof( internal::chunk_list[num] ), sizeof( uintptr_t ) );
		param_ch_array_ = reinterpret_cast<internal::chunk_list*>( p_tmp );
		for ( unsigned int i = 0; i < num; i++ ) {
			new ( &( param_ch_array_[i] ) ) internal::chunk_list( p_param_array[i], &allocating_only_allocator_ );
		}

		pr_ch_size_ = num;
	}

	/*!
	 * @brief	get statistics
	 *
	 * @note
	 * This I/F does not lock allocat/deallocate itself, but this I/F execution is not lock free.
	 */
	std::list<chunk_statistics> get_statistics( void ) const
	{
		std::list<chunk_statistics> ans;

		for ( unsigned int i = 0; i < pr_ch_size_; i++ ) {
			chunk_statistics tmp_st = param_ch_array_[i].get_statistics();
			ans.push_back( tmp_st );
		}

		return ans;
	}

private:
	internal::alloc_only_chamber allocating_only_allocator_;       //!< chunk_header_multi_slotのメモリ割り当て用アロケータ
	unsigned int                 pr_ch_size_;                      //!< array size of chunk and param array
	internal::chunk_list         param_ch_array_impl[NUM_ENTRY];   //!< chunk and param array
	internal::chunk_list*        param_ch_array_;                  //!< pointer to chunk and param array
	std::atomic_bool             exclusive_ctl_of_prune_;          //!< exclusive control for prune()
};

/*!
 * @brief	semi lock-free memory allocator based on multi chunk size list
 *
 * If requested size is over max size that parameter has, just call malloc()
 */
class general_mem_allocator {
public:
	/*!
	 * @brief	constructor
	 */
	constexpr general_mem_allocator( void ) = default;

	/*!
	 * @brief	constructor
	 */
	general_mem_allocator(
		const param_chunk_allocation* p_param_array,   //!< [in] pointer to parameter array
		unsigned int                  num              //!< [in] array size
	);

	/*!
	 * @brief	destructor
	 */
	~general_mem_allocator() = default;

	/*!
	 * @brief	allocate memory
	 */
	void* allocate(
		std::size_t n   //!< [in] memory size to allocate
	)
	{
		return allocator_impl_.allocate( n );
	}

	/*!
	 * @brief	deallocate memory
	 */
	void deallocate(
		void* p_mem   //!< [in] pointer to allocated memory by allocate()
	)
	{
		allocator_impl_.deallocate( p_mem );
	}

	/*!
	 * @brief	free the deletable buffers
	 */
	void prune( void )
	{
		allocator_impl_.prune();
	}

	/*!
	 * @brief	set parameter
	 *
	 * If already set paramter by constructor or this I/F, this call is ignored.
	 *
	 * @warn
	 * This I/F is NOT thread safe. @n
	 * This should be called before launching any threads. Therefore mostly debug or sample code purpose only.
	 */
	void set_param(
		const param_chunk_allocation* p_param_array,   //!< [in] pointer to parameter array
		unsigned int                  num              //!< [in] array size
	)
	{
		allocator_impl_.setup_by_param( p_param_array, num );
	}

	/*!
	 * @brief	get statistics
	 *
	 * @note
	 * This I/F does not lock allocat/deallocate itself, but this I/F execution is not lock free.
	 */
	std::list<chunk_statistics> get_statistics( void ) const
	{
		return allocator_impl_.get_statistics();
	}

private:
	static_general_mem_allocator<0> allocator_impl_;
};

/*!
 * @brief	allocate memory
 *
 * This I/F allocates a memory from a global general_mem_allocator instance. @n
 * The allocated memory must free by gmem_deallocate().
 *
 * @note
 * This uses default_param_array and num_of_default_param_array as initial allocation parameter
 */
void* gmem_allocate(
	std::size_t n   //!< [in] memory size to allocate
);

/*!
 * @brief	deallocate memory
 *
 * This I/F free a memory area that is allocated by gmem_allocate().
 *
 * @note
 * If p_mem is not a memory that is not allocated by gmem_allocate(), this I/F will try to free by calling free().
 */
void gmem_deallocate(
	void* p_mem   //!< [in] pointer to free.
);

/*!
 * @brief	free the deletable buffers
 */
void gmem_prune( void );

/*!
 * @brief	get statistics from a global general_mem_allocator instance
 *
 * @note
 * This I/F does not lock allocat/deallocate itself, but this I/F execution is not lock free.
 */
std::list<chunk_statistics> gmem_get_statistics( void );

/*!
 * @brief get backtrace information
 *
 * this is for debug purpose.
 *
 * @return backtrace information
 * @return 1st element: check result. if success, slot_chk_result::correct_ is true
 * @return 2nd element: backtrace when this memory is allocated.
 * @return 3rd element: backtrace when this memory is free.
 */
std::tuple<bool,
           alpha::concurrent::bt_info,
           alpha::concurrent::bt_info>
get_backtrace_info(
	void* p_mem   //!< [in] pointer to allocated memory by allocate()
);

/*!
 * @brief output backtrace information to log output
 */
void output_backtrace_info(
	const log_type lt,     //!< [in] log type
	void*          p_mem   //!< [in] pointer to allocated memory by allocate()
);

bool test_platform_std_atomic_lockfree_condition( void );

}   // namespace concurrent
}   // namespace alpha

#endif /* INC_ALCONCURRENT_LF_MEM_ALLOC_HPP_ */
