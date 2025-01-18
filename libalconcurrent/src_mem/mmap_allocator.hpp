/**
 * @file mmap_allocator.hpp
 * @author Teruaki Ata <PFA03027@nifty.com>
 * @brief memory allocator by mmap()
 * @version 0.1
 * @date 2023-07-02
 *
 * @copyright Copyright (c) 2023 by Teruaki Ata <PFA03027@nifty.com>
 *
 */

#ifndef ALCONCCURRENT_SRC_MMAP_ALLOCATOR_HPP_
#define ALCONCCURRENT_SRC_MMAP_ALLOCATOR_HPP_

#include <cstdlib>
#include <limits>

namespace alpha {
namespace concurrent {

////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace internal {

/**
 * @brief is power of 2 ?
 *
 * @param v
 * @return true
 * @return false
 */
template <typename T>
constexpr bool is_power_of_2( T v )
{
	static_assert( std::is_integral<T>::value, "T should be integral type" );
#if ( __cpp_constexpr >= 201304 )
	// 2のn乗かどうかを判定する。
	if ( v < 1 ) return false;
	if ( v == 1 ) return true;   // 2のゼロ乗と考えてtrueを返す。

	// step1: 最も下位に1が立っているビットのみ残した値を抽出する
	T v2 = -v & v;
	// step2: 2のn乗の数値は、ビットが1つだけ立っている。よって、2のn乗の数値は最も下位のビットが1つだけ。よって、v2はvと同じになる。
	bool ans = ( v == v2 );
	return ans;
#else
	return ( ( v == ( -v & v ) ) && ( v >= 2 ) ) || ( v == 1 );
#endif
}

/**
 * @brief allocate result by allocate_by_mmap()
 *
 */
struct allocate_result {
	void*  p_allocated_addr_;   //!< allocated memory address. if nullptr, fail to allocate
	size_t allocated_size_;     //!< allocated memory size. if fail to allocate, value is 0. (CATION: 0 does NOT means to fail allocation)
};

/**
 * @brief allocate memory by mmap()
 *
 * @param req_alloc_size requet memory size to allocate
 * @param align_size alignment size of the allocated memory address. If this value is little or equal to 4096(page size), it should be powers of 2. If this value is greater than 4096, it should be multiple of 4096.
 * @return allocate_result
 */
allocate_result allocate_by_mmap( size_t req_alloc_size, size_t align_size ) noexcept;

/**
 * @brief deallocate memory by munmap()
 *
 * @param p_allocated_addr address that is return value of allocate_by_mmap()
 * @param allocated_size allocated memory size by allocate_by_mmap()
 * @retval 0 success
 * @retval -1 fail. please also check errno
 */
int deallocate_by_munmap( void* p_allocated_addr, size_t allocated_size ) noexcept;

struct alloc_mmap_status {
	size_t active_size_;
	size_t max_size_;
};

alloc_mmap_status get_alloc_mmap_status( void ) noexcept;

void print_of_mmap_allocator( void );

// configuration value
// constexpr size_t conf_max_mmap_alloc_size = 1024UL * 1024UL * 1024UL;   // 1G
constexpr size_t conf_max_mmap_alloc_size = std::numeric_limits<size_t>::max() / 2UL;

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif   // MMAP_ALLOCATOR_HPP_
