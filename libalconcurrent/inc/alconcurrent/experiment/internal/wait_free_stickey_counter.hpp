/**
 * @file wait_free_stickey_counter.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief Wait-free sticky counter
 * @version 0.1
 * @date 2025-02-11
 *
 * @copyright Copyright (c) 2025, Teruaki Ata (PFA03027@nifty.com)
 *
 * This wait-free algorithm and implementation introduced in below CppCon 2024 by Daniel Anderson
 *
 * https://www.youtube.com/watch?v=kPh8pod0-gk
 *
 * If the platform is x86, this algorithm may be wait-free truely. Because x86 has the instruction lock xadd.
 * If the platform is ARM64, this algorithm is lock-free. Because ARM64 maybe be used CAS loop by linked-store.
 * Caution: above check is depended on compiler type and/or compiler version. please check by yourself.
 */

#ifndef WAIT_FREE_STICKY_COUNTER_HPP_
#define WAIT_FREE_STICKY_COUNTER_HPP_

#include <atomic>
#include <cstdint>
#include <limits>

namespace alpha {
namespace concurrent {
namespace internal {

/**
 * @brief sticky counter
 *
 * This wait-free algorithm and implementation introduced in below CppCon 2024 by Daniel Anderson
 *
 * https://www.youtube.com/watch?v=kPh8pod0-gk
 *
 * If the platform is x86, this algorithm may be wait-free truely. Because x86 has the instruction lock xadd.
 * If the platform is ARM64, this algorithm is lock-free. Because ARM64 maybe be used CAS loop by linked-store.
 * Caution: above check is depended on compiler type and/or compiler version. please check by yourself.
 */
struct sticky_counter {
	using rc_type                     = uint64_t;
	static constexpr rc_type is_zero_ = ~( std::numeric_limits<rc_type>::max() >> 1 );                  //<! 最上位ビットのみが立った値
	static constexpr rc_type helped_  = ~( ( std::numeric_limits<rc_type>::max() >> 2 ) | is_zero_ );   //<! 最上位から2番目のみのビットが立った値

	/**
	 * @brief increment counter if it is not zero
	 *
	 * @return true counter is not zero, then success to increment
	 * @return false counter is zero, therefore fail to increment
	 */
	bool increment_if_not_zero( void )
	{
		return ( counter_.fetch_add( 1, std::memory_order_acq_rel ) & is_zero_ ) == 0;
	}

	/**
	 * @brief decrement counter
	 *
	 * @pre counter is greater than or equal to 1
	 *
	 * @return true counter reatched to zero after decrement of caller thread
	 * @return false counter is not zero, even if decrement of caller thread
	 *
	 * @warning if caller side violate the pre-condtion, this sticky counter is broken
	 */
	bool decrement_then_is_zero( void )
	{
		if ( counter_.fetch_sub( 1, std::memory_order_acq_rel ) == 1 ) {
			rc_type e = 0;
			if ( counter_.compare_exchange_strong( e, is_zero_, std::memory_order_acq_rel ) ) {
				return true;
			}
			if ( ( e & helped_ ) != 0 ) {
				if ( ( counter_.exchange( is_zero_, std::memory_order_release ) & helped_ ) != 0 ) {
					// this thread get helped flag. this thread takes credit that decrement operation reached to zero.
					return true;
				} else {
					// other thread get helped flag. other thread took credit that decrement operation reached to zero.
				}
			} else {
				// does not reach zero by below possibilies;
				// * other threads increment counter_ b/w fetch_sub and compare_exchange_strong. So, counter become 1 or greater than 1
				// * other thread set zero flag. so, other thread took credit that decrement operation reached to zero.
			}
		}
		return false;
	}

	/**
	 * @brief read counter value
	 *
	 * @return rc_type counter value
	 */
	rc_type read( void ) const noexcept
	{
		rc_type val = counter_.load( std::memory_order_acquire );
		if ( val == 0 ) {
			if ( counter_.compare_exchange_strong( val, is_zero_ | helped_, std::memory_order_acq_rel ) ) {
				return 0;
			}
		}
		return ( val & is_zero_ ) ? 0 : val;
	}

private:
	mutable std::atomic<rc_type> counter_ { 1 };   //!< reference counter. mutable attribute is for read() member function

	static_assert( std::atomic<rc_type>::is_always_lock_free, "rc_type should support atomic operation" );
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
