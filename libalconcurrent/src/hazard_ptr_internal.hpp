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

#ifndef HAZARD_PTR_INTERNAL_HPP_
#define HAZARD_PTR_INTERNAL_HPP_

#include <array>
#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <thread>

#include "alconcurrent/internal/alloc_only_allocator.hpp"
#include "alconcurrent/internal/cpp_std_configure.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

class hazard_ptr_group {
public:
	static constexpr size_t kArraySize = 32;
	using hzrd_p_array_t               = std::array<std::atomic<void*>, kArraySize>;
	using reference                    = hzrd_p_array_t::reference;
	using const_reference              = hzrd_p_array_t::const_reference;
	using iterator                     = hzrd_p_array_t::iterator;
	using const_iterator               = hzrd_p_array_t::const_iterator;
	using reverse_iterator             = hzrd_p_array_t::reverse_iterator;
	using const_reverse_iterator       = hzrd_p_array_t::const_reverse_iterator;
	using size_type                    = hzrd_p_array_t::size_type;
	using difference_type              = hzrd_p_array_t::difference_type;
	using pointer                      = hzrd_p_array_t::pointer;
	using const_pointer                = hzrd_p_array_t::const_pointer;
	using value_type                   = hzrd_p_array_t::value_type;

	class ownership_releaser {
	public:
		void operator()( hazard_ptr_group* ptr ) const
		{
			if ( ptr == nullptr ) return;
			ptr->is_used_.store( false, std::memory_order_release );
		}
	};
	using ownership_t = std::unique_ptr<hazard_ptr_group, ownership_releaser>;

	class hzrd_slot_releaser {
	public:
		void operator()( std::atomic<void*>* ptr ) const
		{
			if ( ptr == nullptr ) return;
			ptr->store( nullptr, std::memory_order_release );
		}
	};
	using hzrd_slot_ownership_t = std::unique_ptr<std::atomic<void*>, hzrd_slot_releaser>;

	ALCC_INTERNAL_CPPSTD17_CONSTEXPR hazard_ptr_group( void )
	  : ap_chain_next_( nullptr )
	  , ap_list_next_( nullptr )
	  , is_used_( false )
	  , hzrd_ptr_array_ {}                                // zero-initialization as nullptr
	  , next_assign_hint_it_( hzrd_ptr_array_.begin() )   // C++17 and after, array::begin is constexpr
	{
	}

	~hazard_ptr_group();

	/**
	 * @brief try to assing a pointer, p, as hazard pointer
	 *
	 * @param p
	 * @return std::atomic<void*>* pointer to hazard ptr slot. nullptr: fail to get hazard pointer slot. non nullptr: success to get hazard pointer slot
	 */
	hzrd_slot_ownership_t try_assign( void* p );

	/**
	 * @brief try to get ownership
	 *
	 * @return ownership_t nullptr: fail to get ownership. non nullptr: success to get ownership
	 */
	ownership_t try_ocupy( void );

	ALCC_INTERNAL_CPPSTD17_CONSTEXPR iterator begin( void )
	{
		return hzrd_ptr_array_.begin();
	}
	ALCC_INTERNAL_CPPSTD17_CONSTEXPR const_iterator begin( void ) const
	{
		return hzrd_ptr_array_.begin();
	}
	ALCC_INTERNAL_CPPSTD17_CONSTEXPR iterator end( void )
	{
		return hzrd_ptr_array_.end();
	}
	ALCC_INTERNAL_CPPSTD17_CONSTEXPR const_iterator end( void ) const
	{
		return hzrd_ptr_array_.end();
	}

	std::atomic<hazard_ptr_group*> ap_chain_next_;
	std::atomic<hazard_ptr_group*> ap_list_next_;

private:
	std::atomic<bool> is_used_;
	hzrd_p_array_t    hzrd_ptr_array_;
	iterator          next_assign_hint_it_;
};

class bind_hazard_ptr_list {
public:
	using hzrd_slot_ownership_t = hazard_ptr_group::hzrd_slot_ownership_t;

	bind_hazard_ptr_list( void )                              = default;
	bind_hazard_ptr_list( bind_hazard_ptr_list&& )            = default;
	bind_hazard_ptr_list& operator=( bind_hazard_ptr_list&& ) = default;
	~bind_hazard_ptr_list();

	hzrd_slot_ownership_t assign( void* p );

private:
	hazard_ptr_group::ownership_t ownership_ticket_;
};

/////////////////////////////////////////////////////////////////
class global_scope_hazard_ptr_chain {
public:
	/**
	 * @brief Get ownership from unused hazard_ptr_group list
	 *
	 * @return hazard_ptr_group::ownership_t ownership data. this api does not return nullptr
	 */
	static inline hazard_ptr_group::ownership_t GetOwnership( void )
	{
		return g_scope_hzrd_chain_.get_ownership();
	}

	/**
	 * @brief remove all hazard_ptr_group from internal global variable
	 *
	 * This API is for debug and test purpose.
	 *
	 * @pre all object of bind_hazard_ptr_list are destructed
	 */
	static inline void DestoryAll( void )
	{
		return g_scope_hzrd_chain_.remove_all();
	}

private:
	constexpr global_scope_hazard_ptr_chain( void )
	  : ap_top_hzrd_ptr_chain_( nullptr )
	{
	}

	/**
	 * @brief try to get ownership from unused hazard_ptr_group list
	 *
	 * @return hazard_ptr_group::ownership_t non nullptr: success to get ownership. nullptr: fail to get ownership
	 */
	hazard_ptr_group::ownership_t try_get_ownership( void );

	/**
	 * @brief register new hazard_ptr_group list
	 *
	 * @param p_hpg_arg
	 */
	void register_new_hazard_ptr_group( hazard_ptr_group* p_hpg_arg );

	/**
	 * @brief get ownership from unused hazard_ptr_group list
	 *
	 * @return hazard_ptr_group::ownership_t ownership data. this api does not return nullptr
	 */
	hazard_ptr_group::ownership_t get_ownership( void );

	/**
	 * @brief remove all hazard_ptr_group
	 */
	void remove_all( void );

	std::atomic<hazard_ptr_group*> ap_top_hzrd_ptr_chain_;

	static global_scope_hazard_ptr_chain g_scope_hzrd_chain_;
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
