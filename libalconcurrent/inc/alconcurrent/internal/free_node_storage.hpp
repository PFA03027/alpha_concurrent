/*!
 * @file	free_node_storage.hpp
 * @brief	free node strage class for lock-free data structure
 * @author	Teruaki Ata
 * @date	Created on 2020/12/31
 * @details
 *
 * Copyright (C) 2020 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef ALCONCURRENT_INC_INTERNAL_FREE_NODE_STORAGE_HPP_
#define ALCONCURRENT_INC_INTERNAL_FREE_NODE_STORAGE_HPP_

#include <atomic>
#include <memory>
#include <mutex>

#include "../hazard_ptr.hpp"
#include "../lf_mem_alloc.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

/*!
 * @brief	lock freeで使用するノードの基本クラス
 *
 * このヘッダファイルで提供する free_node_storage を使用する場合、
 * 利用者側が使用するクラスでのノードクラスをこのクラスから派生させること。
 */
struct node_of_list {
	enum class next_slot_idx : int {
		FREE_ND_LIST_SLOT = 0,   //!<	next slot index for free node list
		TL_LIST_SLOT      = 1    //!<	next slot index for thread local list
	};

	node_of_list( void )
	{
		for ( auto& e : next_ ) {
			e.store( nullptr, std::memory_order_release );
		}
	}

	virtual ~node_of_list()
	{
		return;
	}

	node_of_list* get_next( next_slot_idx cur_slot_idx )
	{
		return next_[(int)cur_slot_idx].load( std::memory_order_acquire );
	}

	void set_next( node_of_list* p_new_next, next_slot_idx cur_slot_idx )
	{
		next_[(int)cur_slot_idx].store( p_new_next, std::memory_order_release );
		return;
	}

	bool next_CAS( node_of_list** pp_expect_ptr, node_of_list* p_desired_ptr, next_slot_idx cur_slot_idx )
	{
		return next_[(int)cur_slot_idx].compare_exchange_weak( *pp_expect_ptr, p_desired_ptr, std::memory_order_acq_rel );
	}

	virtual void release_ownership( void );
	virtual void teardown_by_recycle( void );

	void* operator new( std::size_t n );                               // usual new...(1)
	void* operator new( std::size_t n, std::align_val_t alignment );   // usual new with alignment...(1) C++11/C++14 just ignore. C++17 and after uses this.
	void  operator delete( void* p_mem ) noexcept;                     // usual delete...(2)

	void* operator new[]( std::size_t n );             // usual new...(1)
	void  operator delete[]( void* p_mem ) noexcept;   // usual delete...(2)

	void* operator new( std::size_t n, void* p );          // placement new
	void  operator delete( void* p, void* p2 ) noexcept;   // placement delete...(3)

	static general_mem_allocator_statistics get_statistics( void );

private:
	node_of_list( const node_of_list& )           = delete;
	node_of_list( node_of_list&& )                = delete;
	node_of_list operator=( const node_of_list& ) = delete;
	node_of_list operator=( node_of_list&& )      = delete;

	std::atomic<node_of_list*> next_[2];
};

}   // namespace internal

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_LF_FIFO_HPP_ */
