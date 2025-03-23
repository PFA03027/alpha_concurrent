/**
 * @file hazard_ptr_internal.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-01
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCURRENT_INC_INTERNAL_HAZARD_PTR_INTERNAL_HPP_
#define ALCONCURRENT_INC_INTERNAL_HAZARD_PTR_INTERNAL_HPP_

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <thread>

#include "alconcurrent/internal/cpp_std_configure.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

#if defined( __cpp_lib_hardware_interference_size ) && defined( ALCONCURRENT_CONF_ENABLE_FORCE_USE_INTERFERENCE_SIZE )
constexpr size_t atomic_variable_align = std::hardware_destructive_interference_size;   // it is better to be equal to std::hardware_destructive_interference_size
#else
constexpr size_t atomic_variable_align = 64;   // it is better to be equal to std::hardware_destructive_interference_size
#endif

#if __cpp_aligned_new >= 201606
#define ALIGNAS_ATOMIC_VARIABLE_ALIGN alignas( atomic_variable_align )
#else
#define ALIGNAS_ATOMIC_VARIABLE_ALIGN
#endif

#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
extern std::atomic<size_t> call_count_hazard_ptr_get_;
extern std::atomic<size_t> loop_count_in_hazard_ptr_get_;
#endif

constexpr std::memory_order hzrd_slot_memory_order_for_store = std::memory_order_seq_cst;

class hzrd_slot_releaser {
public:
	inline void operator()( std::atomic<const void*>* ptr ) const noexcept
	{
		if ( ptr == nullptr ) return;
		ptr->store( nullptr, std::memory_order_release );
	}
};
using hzrd_slot_ownership_t = std::unique_ptr<std::atomic<const void*>, hzrd_slot_releaser>;

/////////////////////////////////////////////////////////////////
class hazard_ptr_mgr {
public:
	/**
	 * @brief assign a slot of hazard pointer and set the pointer
	 *
	 * @param p pointer to a object. should not be nullptr
	 * @return hzrd_slot_ownership_t pointer to hazard pointer slot
	 * @retval nullptr fail to assign or p is nullptr
	 * @retval non-nullptr success to assign
	 */
	static hzrd_slot_ownership_t AssignHazardPtrSlot( const void* p );

	/**
	 * @brief Check if p is still in hazard pointer list or not
	 *
	 * @param p
	 * @return true p is still listed in hazard pointer list
	 * @return false p is not hazard pointer
	 */
	static bool CheckPtrIsHazardPtr( void* p ) noexcept;

	/**
	 * @brief Check if p is still in hazard pointer list or not
	 *
	 * @param pred
	 */
	static void ScanHazardPtrs( std::function<void( const void* )> pred );

	/**
	 * @brief remove all hazard_ptr_group from internal global variable
	 *
	 * This API is for debug and test purpose.
	 *
	 * @pre this API should be called from main thread. And, all other threads should be exited before call this API.
	 */
	static void DestoryAll( void );

	/**
	 * @brief check empty or not
	 *
	 * @warning this API has race condition. Therefore, this API is test purpose only.
	 *
	 * @return true hazard pointer related resouce is Empty
	 * @return false hazard pointer related resouce is Not Empty
	 *
	 * @todo introduce exclusive control by mutex b/w DestroyAll() and IsDestoryed()
	 */
	static bool IsDestoryed( void );
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
