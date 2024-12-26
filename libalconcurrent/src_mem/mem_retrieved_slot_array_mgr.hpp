/**
 * @file mem_retrieved_slot_mgr.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-26
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCCURRENT_SRC_MEM_RETRIEVED_SLOT_ARRAY_MGR_HPP_
#define ALCONCCURRENT_SRC_MEM_RETRIEVED_SLOT_ARRAY_MGR_HPP_

#include <atomic>
#include <mutex>
#include <type_traits>
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
#include <exception>
#endif

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/hazard_ptr.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

/**
 * @brief keep the list of retrieved slots
 *
 * @tparam SLOT_T type of slot that requires below;
 * SLOT_T* == decltype(p->p_temprary_link_next_)
 */
template <typename SLOT_T>
struct retrieved_slots_stack {
	using slot_pointer = SLOT_T*;

	constexpr retrieved_slots_stack( void ) noexcept
	  : p_head_of_slot_stack_( nullptr )
	  , count_( 0 )
	{
	}

	void push( slot_pointer p ) noexcept
	{
		if ( p == nullptr ) {
			return;
		}
		p->p_temprary_link_next_ = p_head_of_slot_stack_;
		p_head_of_slot_stack_    = p;
		count_++;
	}

	slot_pointer pop( void ) noexcept
	{
		slot_pointer p = p_head_of_slot_stack_;
		if ( p == nullptr ) {
			return nullptr;
		}
		p_head_of_slot_stack_ = p->p_temprary_link_next_;
		count_--;
		return p;
	}

	void merge( retrieved_slots_stack&& src ) noexcept
	{
		slot_pointer p = src.p_head_of_slot_stack_;
		if ( p == nullptr ) {
			return;
		}
		src.p_head_of_slot_stack_ = nullptr;

		count_ += src.count_;
		src.count_ = 0;

		slot_pointer p_last = p;
		while ( p_last->p_temprary_link_next_ != nullptr ) {
			p_last = p_last->p_temprary_link_next_;
		}
		p_last->p_temprary_link_next_ = p_head_of_slot_stack_;
		p_head_of_slot_stack_         = p;
	}

	bool is_empty( void ) const noexcept
	{
		return p_head_of_slot_stack_ == nullptr;
	}

	size_t count( void ) const noexcept
	{
		return count_;
	}

	void reset_for_test( void ) noexcept
	{
		p_head_of_slot_stack_ = nullptr;   // even if leaked, just release to detect memory leak
	}

private:
	slot_pointer p_head_of_slot_stack_;   //!< pointer to head unused memory slot stack
	size_t       count_;
};

/**
 * @brief keep the list of retrieved slots as global
 *
 * @tparam SLOT_T type of slot that requires below;
 * SLOT_T* == decltype(p->p_temprary_link_next_)
 */
template <typename SLOT_T>
struct retrieved_slots_stack_lockable {
	using slot_pointer = SLOT_T*;

	constexpr retrieved_slots_stack_lockable( void ) noexcept
	  : mtx_()
	  , head_unused_memory_slot_stack_()
	{
	}

	slot_pointer try_push( slot_pointer p ) noexcept
	{
		std::unique_lock<std::mutex> lock( mtx_, std::try_to_lock );
		if ( !lock.owns_lock() ) {
			return p;
		}

		head_unused_memory_slot_stack_.push( p );
		return nullptr;
	}

	slot_pointer try_pop( void ) noexcept
	{
		std::unique_lock<std::mutex> lock( mtx_, std::try_to_lock );
		if ( !lock.owns_lock() ) {
			return nullptr;
		}

		return head_unused_memory_slot_stack_.pop();
	}

	void merge( retrieved_slots_stack<SLOT_T>&& src ) noexcept
	{
		std::lock_guard<std::mutex> lock( mtx_ );
		head_unused_memory_slot_stack_.merge( std::move( src ) );
	}

	bool is_empty( void ) const noexcept
	{
		std::lock_guard<std::mutex> lock( mtx_ );
		return head_unused_memory_slot_stack_.is_empty();
	}

	size_t count( void ) const noexcept
	{
		std::lock_guard<std::mutex> lock( mtx_ );
		return head_unused_memory_slot_stack_.count();
	}

	void reset_for_test( void ) noexcept
	{
	}

private:
	mutable std::mutex            mtx_;
	retrieved_slots_stack<SLOT_T> head_unused_memory_slot_stack_;   //!< pointer to head unused memory slot stack
};

/**
 * @brief keep the list of retrieved slots as global
 *
 * @tparam SLOT_T type of slot that requires below;
 * SLOT_T* == decltype(p->p_temprary_link_next_)
 */
template <typename SLOT_T>
struct retrieved_slots_stack_lockfree {
	using slot_pointer = SLOT_T*;

	constexpr retrieved_slots_stack_lockfree( void ) noexcept
	  : hph_head_unused_memory_slot_stack_()
	{
	}

	slot_pointer try_push( slot_pointer p ) noexcept
	{
		if ( p == nullptr ) {
			return nullptr;
		}

		// Experimental: CAS loopせず、素直にあきらめる方式
		slot_pointer p_cur_head = hph_head_unused_memory_slot_stack_.load( std::memory_order_acquire );
		p->ap_slot_next_.store( p_cur_head, std::memory_order_release );
		if ( !hph_head_unused_memory_slot_stack_.compare_exchange_strong( p_cur_head, p, std::memory_order_acq_rel ) ) {
			return p;
		}

		return nullptr;
	}

	slot_pointer try_pop( void ) noexcept
	{
		// Experimental: CAS loopせず、素直にあきらめる方式
		hazard_pointer hp_cur_head = hph_head_unused_memory_slot_stack_.get_to_verify_exchange();
		if ( !hph_head_unused_memory_slot_stack_.verify_exchange( hp_cur_head ) ) {
			return nullptr;
		}
		if ( hp_cur_head == nullptr ) {
			return nullptr;
		}

		typename hazard_pointer::pointer p_new_head = hp_cur_head->ap_slot_next_.load( std::memory_order_acquire );
		if ( hph_head_unused_memory_slot_stack_.compare_exchange_strong_to_verify_exchange2( hp_cur_head, p_new_head ) ) {
			// hp_cur_headの所有権を獲得
			return hp_cur_head.get();
		}
		return nullptr;
	}

	void merge( retrieved_slots_stack<SLOT_T>&& src ) noexcept
	{
		slot_pointer p = src.pop();
		while ( p != nullptr ) {
			push( p );
			p = src.pop();
		}
	}

	void reset_for_test( void ) noexcept
	{
		hph_head_unused_memory_slot_stack_.store( nullptr, std::memory_order_release );
	}

private:
	using hazard_pointer = typename hazard_ptr_handler<SLOT_T>::hazard_pointer;

	void push( slot_pointer p ) noexcept
	{
		SLOT_T* p_cur_head = hph_head_unused_memory_slot_stack_.load( std::memory_order_acquire );
		do {
			p->ap_slot_next_.store( p_cur_head, std::memory_order_release );
		} while ( !hph_head_unused_memory_slot_stack_.compare_exchange_strong( p_cur_head, p, std::memory_order_acq_rel ) );

		return;
	}

	hazard_ptr_handler<SLOT_T> hph_head_unused_memory_slot_stack_;   //!< pointer to head unused memory slot stack
};

/**
 * @brief slot manager I/F for retrieved slots
 *
 * @tparam SLOT_T type of slot that requires below;
 * SLOT_T* == decltype(p->p_temprary_link_next_)
 * std::atomic<SLOT_T*> == decltype(p->ap_slot_next_)
 */
template <typename SLOT_T>
struct retrieved_slots_stack_array_mgr {
	using slot_pointer = SLOT_T*;

	static constexpr size_t max_entry_ = 16;

	static void         retrieve( size_t idx, slot_pointer p ) noexcept;
	static slot_pointer request_reuse( size_t idx ) noexcept;

private:
	static retrieved_slots_stack_lockfree<SLOT_T> global_non_hazard_retrieved_slots_lockfree_stack_[max_entry_];
	static retrieved_slots_stack_lockable<SLOT_T> global_in_hazard_retrieved_slots_lockable_stack_[max_entry_];

	struct tls_data {
		retrieved_slots_stack<SLOT_T> non_hazard_retrieved_slots_stack_[max_entry_];
		retrieved_slots_stack<SLOT_T> in_hazard_retrieved_slots_stack_[max_entry_];

		constexpr tls_data( void ) noexcept
		  : non_hazard_retrieved_slots_stack_ {}
		  , in_hazard_retrieved_slots_stack_ {}
		{
		}

		~tls_data( void )
		{
			for ( size_t i = 0; i < max_entry_; i++ ) {
				global_non_hazard_retrieved_slots_lockfree_stack_[i].merge( std::move( non_hazard_retrieved_slots_stack_[i] ) );
				global_in_hazard_retrieved_slots_lockable_stack_[i].merge( std::move( in_hazard_retrieved_slots_stack_[i] ) );
			}
		}
	};

	static thread_local tls_data tls_data_;
};

template <typename SLOT_T>
retrieved_slots_stack_lockfree<SLOT_T> retrieved_slots_stack_array_mgr<SLOT_T>::global_non_hazard_retrieved_slots_lockfree_stack_[max_entry_];

template <typename SLOT_T>
retrieved_slots_stack_lockable<SLOT_T> retrieved_slots_stack_array_mgr<SLOT_T>::global_in_hazard_retrieved_slots_lockable_stack_[max_entry_];

template <typename SLOT_T>
thread_local typename retrieved_slots_stack_array_mgr<SLOT_T>::tls_data retrieved_slots_stack_array_mgr<SLOT_T>::tls_data_;

template <typename SLOT_T>
void retrieved_slots_stack_array_mgr<SLOT_T>::retrieve( size_t idx, slot_pointer p ) noexcept
{
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
	if ( idx >= max_entry_ ) {
		LogOutput( log_type::ERR, "retrieved_slots_stack_array_mgr::push: idx is out of range" );
		std::terminate();
	}
#endif

	if ( hazard_ptr_mgr::CheckPtrIsHazardPtr( p ) ) {
		// ハザードポインタとして登録されている場合、ハザードポインタ登録中のリストに追加する
		tls_data_.in_hazard_retrieved_slots_stack_[idx].push( p );
	} else {
		// ハザードポインタとして登録されていない場合、未使用スロットリストに登録する
		if ( tls_data_.non_hazard_retrieved_slots_stack_[idx].is_empty() ) {
			tls_data_.non_hazard_retrieved_slots_stack_[idx].push( p );
		} else {
			p = global_non_hazard_retrieved_slots_lockfree_stack_[idx].try_push( p );
			if ( p != nullptr ) {
				tls_data_.non_hazard_retrieved_slots_stack_[idx].push( p );
			}
		}
	}
}

template <typename SLOT_T>
typename retrieved_slots_stack_array_mgr<SLOT_T>::slot_pointer retrieved_slots_stack_array_mgr<SLOT_T>::request_reuse( size_t idx ) noexcept
{
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
	if ( idx >= max_entry_ ) {
		LogOutput( log_type::ERR, "retrieved_slots_stack_array_mgr::pop: idx is out of range" );
		std::terminate();
	}
#endif

	// 一番スピードが速いTLSから取得する
	slot_pointer p = tls_data_.non_hazard_retrieved_slots_stack_[idx].pop();
	if ( p != nullptr ) {
		return p;
	}

	// ロックフリースタックから取得を試みる。
	p = global_non_hazard_retrieved_slots_lockfree_stack_[idx].try_pop();
	if ( p != nullptr ) {
		return p;
	}

	// TLSのハザードポインタ登録中リストから取得を試みる。
	p = tls_data_.in_hazard_retrieved_slots_stack_[idx].pop();
	if ( p != nullptr ) {
		if ( hazard_ptr_mgr::CheckPtrIsHazardPtr( p ) ) {
			// まだハザードポインタとして登録されている場合、ハザードポインタ登録中のリストに差し戻す。
			tls_data_.in_hazard_retrieved_slots_stack_[idx].push( p );
		} else {
			// ハザードポインタとして登録されていない場合
			return p;
		}
	}

	return global_in_hazard_retrieved_slots_lockable_stack_[idx].try_pop();
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif /* ALCONCCURRENT_SRC_MEM_RETRIEVED_SLOT_ARRAY_MGR_HPP_ */
