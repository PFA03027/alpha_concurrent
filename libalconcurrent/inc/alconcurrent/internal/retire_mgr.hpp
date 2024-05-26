/**
 * @file retire_mgr.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-04
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCURRENT_INC_INTERNAL_RETIRE_MGR_HPP_
#define ALCONCURRENT_INC_INTERNAL_RETIRE_MGR_HPP_

#include <atomic>
#include <memory>
#include <mutex>

#include "../hazard_ptr.hpp"
#include "../lf_mem_alloc.hpp"
#include "od_node_base.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

///////////////////////////////////////////////////////////////////////////////////////////////
struct retire_node_abst : public od_node_base<retire_node_abst> {
	retire_node_abst( void* p_retire_arg ) noexcept
	  : od_node_base<retire_node_abst>( nullptr )
	  , p_retire_( p_retire_arg )
	{
	}
	virtual ~retire_node_abst() = default;

	void* get_retire_pointer( void ) const noexcept
	{
		return p_retire_;
	}

private:
	void* p_retire_;
};

/**
 * @brief
 *
 * @tparam T
 * @tparam Deleter please refer to std::deleter<T>. this class should not throw execption
 */
template <typename T, typename Deleter>
struct retire_node : public retire_node_abst {
	static_assert( std::is_nothrow_default_constructible<Deleter>::value &&
	                   std::is_nothrow_copy_constructible<Deleter>::value &&
	                   std::is_nothrow_move_constructible<Deleter>::value,
	               "Deleter should be nothrow of constructors" );

	Deleter deleter_;

	constexpr retire_node( T* p_retire_arg, const Deleter& deleter_arg ) noexcept
	  : retire_node_abst { p_retire_arg }
	  , deleter_( deleter_arg )
	{
	}
	constexpr retire_node( T* p_retire_arg, Deleter&& deleter_arg ) noexcept
	  : retire_node_abst { p_retire_arg }
	  , deleter_( std::move( deleter_arg ) )
	{
	}

	~retire_node() override
	{
		deleter_( reinterpret_cast<T*>( get_retire_pointer() ) );
	}
};

/**
 * @brief retire管理用I/Fクラス（Facadeクラス）
 *
 * @todo シングルトン化すべきか？
 */
class retire_mgr {
public:
	template <typename T, typename Deleter = std::default_delete<T>>
	static void retire( T* p_retire_obj, Deleter&& deleter_arg = std::default_delete<T> {} )
	{
		if ( internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( p_retire_obj ) ) {
			retire_node_abst* p_new_retire = new retire_node<T, Deleter>( p_retire_obj, std::forward<Deleter>( deleter_arg ) );
			retire_impl( p_new_retire );
		} else {
			deleter_arg( p_retire_obj );
		}
	}

	template <typename T, typename Deleter = std::default_delete<T>>
	static void retire_always_store( T* p_retire_obj, Deleter&& deleter_arg = std::default_delete<T> {} )
	{
		retire_node_abst* p_new_retire = new retire_node<T, Deleter>( p_retire_obj, std::forward<Deleter>( deleter_arg ) );
		retire_impl( p_new_retire );
	}

	static void stop_prune_thread( void );

private:
	class prune_thread_inst_t;
	class prune_thread_mgr;

	static void retire_impl( retire_node_abst* p_new_retire );

	static void prune_one_work( void );
	static void prune_thread( void );
	static void request_stop_prune_thread( void );

	static std::atomic<bool>   loop_flag_prune_thread_;
	static prune_thread_inst_t g_prune_thread_obj;
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
