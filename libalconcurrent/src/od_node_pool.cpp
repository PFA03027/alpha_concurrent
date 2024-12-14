/**
 * @file od_node_pool.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-06-30
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include "alconcurrent/internal/od_node_pool.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

#ifdef ALCONCURRENT_CONF_ENABLE_COUNTERMEASURE_GCC_BUG_66944
thread_local std::list<std::unique_ptr<countermeasure_gcc_bug_deletable_obj_abst>> tl_list_list;
#endif

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
