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
#include "od_node_pool.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

///////////////////////////////////////////////////////////////////////////////////////////////
class retire_node_abst : public od_node_base<retire_node_abst> {
public:
	using raw_next_t            = typename od_node_base<retire_node_abst>::od_node_base_raw_next_t;
	using hazard_handler_next_t = typename od_node_base<retire_node_abst>::od_node_base_hazard_handler_next_t;

	retire_node_abst( void* p_retire_arg ) noexcept
	  : od_node_base<retire_node_abst>( nullptr )
	  , p_retire_( p_retire_arg )
	{
		count_allocate_++;
	}
	virtual ~retire_node_abst()
	{
		count_allocate_--;
	}

	void* get_retire_pointer( void ) const noexcept
	{
		return p_retire_;
	}
	void set_retire_pointer( void* p ) noexcept
	{
		p_retire_ = p;
	}

	virtual void call_deleter( void ) noexcept = 0;

	static void recycle( retire_node_abst* p_r )
	{
		if ( p_r == nullptr ) return;

		recycler_t* p_recycler = p_r->get_recycler();
		if ( p_recycler == nullptr ) {   // to be safe
			LogOutput( log_type::ERR, "pointer to recycler is nullptr. this is critical implementation logic error." );
			delete p_r;
			return;
		}

		p_recycler->do_recycle( p_r );
	}

	static size_t get_allocate_count( void )
	{
		return count_allocate_.load( std::memory_order_acquire );
	}

protected:
	class recycler_t {
	public:
		virtual void do_recycle( retire_node_abst* p_r ) = 0;
	};

	// should not return nullptr
	virtual recycler_t* get_recycler( void ) noexcept = 0;

	static std::atomic<size_t> count_allocate_;

private:
	void* p_retire_;
};

///////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *
 * @tparam T
 * @tparam Deleter please refer to std::deleter<T>. this class instance has ownership of argument pointer when called. this class should not throw execption
 */
template <typename T, typename Deleter>
struct retire_node : public retire_node_abst, public od_node_base<retire_node<T, Deleter>> {
	static_assert( std::is_nothrow_default_constructible<Deleter>::value &&
	                   std::is_nothrow_copy_constructible<Deleter>::value &&
	                   std::is_nothrow_move_constructible<Deleter>::value,
	               "Deleter should have nothrow of constructors" );

	using raw_next_t            = typename od_node_base<retire_node<T, Deleter>>::od_node_base_raw_next_t;
	using hazard_handler_next_t = typename od_node_base<retire_node<T, Deleter>>::od_node_base_hazard_handler_next_t;

	Deleter deleter_;

	constexpr retire_node( T* p_retire_arg, const Deleter& deleter_arg ) noexcept
	  : retire_node_abst { p_retire_arg }
	  , od_node_base<retire_node<T, Deleter>>()
	  , deleter_( deleter_arg )
	{
	}
	constexpr retire_node( T* p_retire_arg, Deleter&& deleter_arg ) noexcept
	  : retire_node_abst { p_retire_arg }
	  , od_node_base<retire_node<T, Deleter>>()
	  , deleter_( std::move( deleter_arg ) )
	{
	}

	~retire_node() override
	{
		deleter_( reinterpret_cast<T*>( get_retire_pointer() ) );
	}

	void call_deleter( void ) noexcept override
	{
		T* p_tmp = reinterpret_cast<T*>( get_retire_pointer() );
		set_retire_pointer( nullptr );
		deleter_( p_tmp );
	}

	template <typename DeleterU = Deleter>
	static retire_node* allocate( T* p_retire_arg, DeleterU&& deleter_arg )
	{
		retire_node* p_ans = retire_node_recycler_.do_allocate();
		if ( p_ans != nullptr ) {
			p_ans->set_retire_pointer( p_retire_arg );
			p_ans->deleter_ = std::forward<DeleterU>( deleter_arg );
		} else {
			p_ans = new retire_node( p_retire_arg, std::forward<DeleterU>( deleter_arg ) );
		}
		return p_ans;
	}

protected:
	class retire_node_recycler_t : public recycler_t {
	public:
		void do_recycle( retire_node_abst* p_r ) override
		{
			if ( p_r == nullptr ) {
				return;
			}

			retire_node* p_my_t_node = dynamic_cast<retire_node*>( p_r );
			if ( p_my_t_node == nullptr ) {
				throw std::logic_error( "p_r is not this class pointer" );
			}

			retire_node_pool::push( p_my_t_node );
		}

		retire_node* do_allocate( void )
		{
			return retire_node_pool::pop();
		}

	private:
		using retire_node_pool = od_node_pool<retire_node, typename retire_node::raw_next_t>;
	};

	recycler_t* get_recycler( void ) noexcept override
	{
		return &retire_node_recycler_;
	}

	static retire_node_recycler_t retire_node_recycler_;
};

template <typename T, typename Deleter>
typename retire_node<T, Deleter>::retire_node_recycler_t retire_node<T, Deleter>::retire_node_recycler_;

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
#ifdef ALCONCURRENT_CONF_ENABLE_ALL_NODE_RECYCLE_BY_PRUNE_THREAD
		retire_always_store( p_retire_obj, std::forward<Deleter>( deleter_arg ) );
#else
		if ( internal::hazard_ptr_mgr::CheckPtrIsHazardPtr( p_retire_obj ) ) {
			retire_node_abst* p_new_retire = retire_node<T, Deleter>::allocate( p_retire_obj, std::forward<Deleter>( deleter_arg ) );
			retire_impl( p_new_retire );
		} else {
			deleter_arg( p_retire_obj );
		}
#endif
	}

	template <typename T, typename Deleter = std::default_delete<T>>
	static void retire_always_store( T* p_retire_obj, Deleter&& deleter_arg = std::default_delete<T> {} )
	{
		retire_node_abst* p_new_retire = retire_node<T, Deleter>::allocate( p_retire_obj, std::forward<Deleter>( deleter_arg ) );
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
