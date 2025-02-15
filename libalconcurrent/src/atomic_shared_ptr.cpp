/**
 * @file atomic_share_ptr.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2025-02-15
 *
 * @copyright Copyright (c) 2025, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "alconcurrent/experiment/internal/atomic_shared_ptr.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

// defered reclamation
void control_block_base::retire( control_block_base* p )
{
	delete p;
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
