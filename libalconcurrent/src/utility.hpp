/**
 * @file utility.hpp
 * @author Teruaki Ata <PFA03027@nifty.com>
 * @brief I/F header file of allocation only allocator
 * @version 0.1
 * @date 2023-07-16
 *
 * @copyright Copyright (c) 2023 Teruaki Ata <PFA03027@nifty.com>
 *
 */

#ifndef ALCONCCURRENT_SRC__UTILITY_HPP_
#define ALCONCCURRENT_SRC__UTILITY_HPP_

#include <atomic>

namespace alpha {
namespace concurrent {
namespace internal {

////////////////////////////////////////////////////////////////////////////////

class scoped_inout_counter_atomic_int {
public:
	scoped_inout_counter_atomic_int( std::atomic<int>& atomic_couter_ref_arg )
	  : atomic_couter_ref( atomic_couter_ref_arg )
	{
		atomic_couter_ref.fetch_add( 1, std::memory_order_acq_rel );
	}

	~scoped_inout_counter_atomic_int()
	{
		atomic_couter_ref.fetch_sub( 1, std::memory_order_acq_rel );
	}

private:
	std::atomic<int>& atomic_couter_ref;
};

////////////////////////////////////////////////////////////////////////////////

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
