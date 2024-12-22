/**
 * @file mem_common.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-12-22
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCCURRENT_SRC_MEM_COMMON_HPP_
#define ALCONCCURRENT_SRC_MEM_COMMON_HPP_

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
 * @brief memory management type identifier
 *
 */
enum class mem_type : uint8_t {
	NON_USED     = 0,   //!< not assigned yet
	SMALL_MEM    = 1,   //!< memory allocation type is small memory type
	BIG_MEM      = 2,   //!< memory allocation type is big memory type
	OVER_BIG_MEM = 3,   //!< memory allocation type is over big memory type
};

/**
 * @brief utility structure of unziped data of addr_w_mem_flag_ in allocated_mem_top
 *
 */
template <typename T>
struct unziped_allocation_info {
	T*       p_mgr_;
	mem_type mt_;
	bool     is_used_;
};

/**
 * @brief allocated memory information
 *
 */
struct allocated_mem_top {
	std::atomic<uintptr_t> addr_w_mem_flag_;   //!< 0..1: memory allocation type, 2: using/released flag, 3..63: address information
	unsigned char          data_[0];           //!< top for allocated memory

	static constexpr size_t min_alignment_size_ = sizeof( std::atomic<uintptr_t> );

	template <typename U>
	static constexpr allocated_mem_top* emplace_on_mem( unsigned char* p_mem, U* p_mgr_arg, mem_type mt_arg, bool is_used_arg ) noexcept
	{
		return new ( p_mem ) allocated_mem_top( p_mgr_arg, mt_arg, is_used_arg );
	}

	static constexpr allocated_mem_top* emplace_on_mem( unsigned char* p_mem, const allocated_mem_top& src ) noexcept
	{
		return new ( p_mem ) allocated_mem_top( src );
	}

	template <typename U>
	constexpr allocated_mem_top( U* p_mgr_arg, mem_type mt_arg, bool is_used_arg ) noexcept
	  : addr_w_mem_flag_( zip_allocation_info( p_mgr_arg, mt_arg, is_used_arg ) )
	  , data_ {}
	{
	}

	allocated_mem_top( const allocated_mem_top& src ) noexcept
	  : addr_w_mem_flag_( src.addr_w_mem_flag_.load( std::memory_order_acquire ) )
	  , data_ {}
	{
	}

	static allocated_mem_top* get_structure_addr( void* p ) noexcept
	{
		uintptr_t addr = reinterpret_cast<uintptr_t>( p );
		addr -= static_cast<uintptr_t>( sizeof( std::atomic<uintptr_t> ) );
		return reinterpret_cast<allocated_mem_top*>( addr );
	}

	template <typename U>
	unziped_allocation_info<U> load_allocation_info( std::memory_order mo = std::memory_order_acquire ) const noexcept
	{
		uintptr_t                  addr_w_info = addr_w_mem_flag_.load( mo );
		unziped_allocation_info<U> ans;
		ans.p_mgr_   = reinterpret_cast<U*>( addr_w_info & clear_all_flags );
		ans.mt_      = static_cast<mem_type>( addr_w_info & mem_type_bits_ );
		ans.is_used_ = ( ( addr_w_info & used_info_bits_ ) != 0 ) ? true : false;
		return ans;
	}

	template <typename U>
	void store_addr( U* p ) noexcept
	{
		uintptr_t addr_p = reinterpret_cast<uintptr_t>( p );
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
		if ( ( addr_p & all_flags_ ) != 0 ) {
			std::terminate();
		}
#endif

		uintptr_t cur_flags_info = addr_w_mem_flag_.load( std::memory_order_acquire ) & all_flags_;

		uintptr_t desired_addr_w_info = addr_p | cur_flags_info;
		addr_w_mem_flag_.store( desired_addr_w_info, std::memory_order_release );
	}

	bool fetch_set( bool is_used ) noexcept
	{
		uintptr_t addr_w_info         = addr_w_mem_flag_.load( std::memory_order_acquire );
		bool      ans                 = false;
		uintptr_t desired_addr_w_info = 0;
		do {
			ans = ( addr_w_info & used_info_bits_ ) != 0;
			if ( is_used ) {
				desired_addr_w_info = addr_w_info | used_info_bits_;
			} else {
				desired_addr_w_info = addr_w_info & ( ~( used_info_bits_ ) );
			}
		} while ( !addr_w_mem_flag_.compare_exchange_strong( addr_w_info, desired_addr_w_info, std::memory_order_acq_rel ) );
		return ans;
	}

	template <typename U>
	U* load_addr( void ) const noexcept
	{
		uintptr_t addr_w_info = addr_w_mem_flag_.load( std::memory_order_acquire );
		return reinterpret_cast<U*>( addr_w_info & clear_all_flags );
	}

	mem_type load_mem_type( void ) const noexcept
	{
		uintptr_t addr_w_info = addr_w_mem_flag_.load( std::memory_order_acquire );
		return static_cast<mem_type>( addr_w_info & mem_type_bits_ );
	}

	bool compare_and_exchange_used_flag( bool& expected, bool desired ) noexcept
	{
		uintptr_t expected_addr_w_info = addr_w_mem_flag_.load( std::memory_order_acquire );
		uintptr_t desired_addr_w_info  = expected_addr_w_info;
		expected_addr_w_info           = ( expected ) ? ( expected_addr_w_info | used_info_bits_ ) : ( expected_addr_w_info & ( ~( used_info_bits_ ) ) );
		desired_addr_w_info            = ( desired ) ? ( desired_addr_w_info | used_info_bits_ ) : ( desired_addr_w_info & ( ~( used_info_bits_ ) ) );
		bool ans                       = addr_w_mem_flag_.compare_exchange_strong( expected_addr_w_info, desired_addr_w_info, std::memory_order_acq_rel );
		if ( !ans ) {
			expected = ( expected_addr_w_info & used_info_bits_ ) != 0;
		}
		return ans;
	}

	bool compare_and_exchange_addr( uintptr_t& expected, uintptr_t desired ) noexcept
	{
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
		if ( ( expected & all_flags_ ) != 0 ) {
			std::terminate();
		}
		if ( ( desired & all_flags_ ) != 0 ) {
			std::terminate();
		}
#endif

		uintptr_t cur_flags_info = addr_w_mem_flag_.load( std::memory_order_acquire ) & all_flags_;

		uintptr_t expected_addr_w_info = expected | cur_flags_info;
		uintptr_t desired_addr_w_info  = desired | cur_flags_info;
		bool      ans                  = addr_w_mem_flag_.compare_exchange_strong( expected_addr_w_info, desired_addr_w_info, std::memory_order_acq_rel );
		if ( !ans ) {
			expected = expected_addr_w_info & clear_all_flags;
		}
		return ans;
	}

private:
	static constexpr uintptr_t all_flags_      = static_cast<uintptr_t>( 7L );
	static constexpr uintptr_t clear_all_flags = ~( all_flags_ );
	static constexpr uintptr_t mem_type_bits_  = static_cast<uintptr_t>( 3L );
	static constexpr uintptr_t used_info_bits_ = static_cast<uintptr_t>( 4L );

	template <typename U>
	constexpr uintptr_t zip_allocation_info( U* p_mgr_arg, mem_type mt_arg, bool is_used_arg ) noexcept
	{
		return reinterpret_cast<uintptr_t>( p_mgr_arg ) |
		       static_cast<uintptr_t>( mt_arg ) |
		       ( is_used_arg ? used_info_bits_ : static_cast<uintptr_t>( 0 ) );
	}

	static constexpr void* operator new( std::size_t s, void* p ) noexcept   // placement new
	{
		return p;
	}
	static constexpr void operator delete( void* ptr, void* ) noexcept   // placement delete
	{
		return;
	}
};

static_assert( std::is_trivially_destructible<allocated_mem_top>::value );

/**
 * @brief retrieved slots kept by stack
 *
 * とりあえずの、仮実装。後で、スレッドローカル変数を使って、ロックフリー化する。
 *
 * @tparam SLOT_T type of slot that requires below;
 * SLOT_T* == decltype(p->p_temprary_link_next_)
 * std::atomic<SLOT_T*> == decltype(p->ap_slot_next_)
 * callable: p->check_validity_to_ownwer_and_get()
 */
template <typename SLOT_T>
struct retrieved_slots_mgr_impl {
	hazard_ptr_handler<SLOT_T> hph_head_unused_memory_slot_stack_;   //!< pointer to head unused memory slot stack
	mutable std::mutex         mtx_;
	SLOT_T*                    p_head_unused_memory_slot_stack_in_hazard_;   //!< pointer to head unused memory slot stack
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	std::atomic<size_t> count_in_not_hazard_;
	std::atomic<size_t> count_in_hazard_;
#endif

	constexpr retrieved_slots_mgr_impl( void ) noexcept
	  : hph_head_unused_memory_slot_stack_( nullptr )
	  , mtx_()
	  , p_head_unused_memory_slot_stack_in_hazard_( nullptr )
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	  , count_in_not_hazard_( 0 )
	  , count_in_hazard_( 0 )
#endif
	{
	}

	void    retrieve( SLOT_T* p ) noexcept;   // p should be valid pointer and *p should be valid as SLOT_T
	SLOT_T* request_reuse( void ) noexcept;

private:
	using hazard_pointer = typename hazard_ptr_handler<SLOT_T>::hazard_pointer;

	void    retrieve_impl( SLOT_T* p ) noexcept;
	SLOT_T* request_reuse_impl( void ) noexcept;
};

template <typename SLOT_T>
void retrieved_slots_mgr_impl<SLOT_T>::retrieve_impl( SLOT_T* p ) noexcept
{
	if ( hazard_ptr_mgr::CheckPtrIsHazardPtr( p ) ) {
		// ハザードポインタとして登録されている場合、ハザードポインタ登録中のリストに追加する
		std::lock_guard<std::mutex> lock( mtx_ );
		p->p_temprary_link_next_                   = p_head_unused_memory_slot_stack_in_hazard_;
		p_head_unused_memory_slot_stack_in_hazard_ = p;
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		count_in_hazard_++;
#endif
	} else {
		// ハザードポインタとして登録されていない場合、未使用スロットリストに登録する
		SLOT_T* p_cur_head = hph_head_unused_memory_slot_stack_.load( std::memory_order_acquire );
		do {
			p->ap_slot_next_.store( p_cur_head, std::memory_order_release );
		} while ( !hph_head_unused_memory_slot_stack_.compare_exchange_strong( p_cur_head, p, std::memory_order_acq_rel ) );
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		count_in_not_hazard_++;
#endif
	}
}

template <typename SLOT_T>
void retrieved_slots_mgr_impl<SLOT_T>::retrieve( SLOT_T* p ) noexcept
{
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
	if ( p == nullptr ) {
		std::terminate();
	}
#endif
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	auto               p_slot_owner = p->check_validity_to_ownwer_and_get();
	btinfo_alloc_free& cur_btinfo   = p_slot_owner->get_btinfo( p_slot_owner->get_slot_idx( p ) );
	cur_btinfo.free_trace_          = bt_info::record_backtrace();
#endif

	retrieve_impl( p );
}

template <typename SLOT_T>
SLOT_T* retrieved_slots_mgr_impl<SLOT_T>::request_reuse_impl( void ) noexcept
{
	// 未使用スロットリストから取得する
	hazard_pointer                   hp_cur_head = hph_head_unused_memory_slot_stack_.get_to_verify_exchange();
	typename hazard_pointer::pointer p_new_head;
	while ( true ) {
		if ( !hph_head_unused_memory_slot_stack_.verify_exchange( hp_cur_head ) ) {
			continue;
		}
		if ( hp_cur_head == nullptr ) {
			break;
		}

		p_new_head = hp_cur_head->ap_slot_next_.load( std::memory_order_acquire );
		if ( hph_head_unused_memory_slot_stack_.compare_exchange_strong_to_verify_exchange2( hp_cur_head, p_new_head ) ) {
			// hp_cur_headの所有権を獲得
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			count_in_not_hazard_--;
#endif
			return hp_cur_head.get();
		}
	}

	// ここに来た時点で、未使用スロットリストが空のため、slotが取得できなかったことを示す。
	// この場合、ハザードポインタ登録中リストからの取得を試みる。
	// TODO: とりあえず、ロックの確保待ちを行う方式で暫定実装している。
	std::lock_guard<std::mutex> lock( mtx_ );
	SLOT_T*                     p_cur      = nullptr;
	SLOT_T*                     p_tmp_head = nullptr;
	while ( p_head_unused_memory_slot_stack_in_hazard_ != nullptr ) {
		p_cur                                      = p_head_unused_memory_slot_stack_in_hazard_;
		p_head_unused_memory_slot_stack_in_hazard_ = p_cur->p_temprary_link_next_;

		if ( !hazard_ptr_mgr::CheckPtrIsHazardPtr( p_cur ) ) {
			// p_curが利用可能なスロットとして見つかった
			break;
		}

		// p_curはまだハザードポインタとして登録されているので、一時キープ用のリストに追加する
		p_cur->p_temprary_link_next_ = p_tmp_head;
		p_tmp_head                   = p_cur;
		p_cur                        = nullptr;
	}

	// 一時的にキープしていたスロットを未使用スロットリストに戻す。
	while ( p_tmp_head != nullptr ) {
		SLOT_T* p_tmp                              = p_tmp_head;
		p_tmp_head                                 = p_tmp_head->p_temprary_link_next_;
		p_tmp->p_temprary_link_next_               = p_head_unused_memory_slot_stack_in_hazard_;
		p_head_unused_memory_slot_stack_in_hazard_ = p_tmp;
	}
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	if ( p_cur != nullptr ) {
		count_in_hazard_--;
	}
#endif
	return p_cur;
}

template <typename SLOT_T>
SLOT_T* retrieved_slots_mgr_impl<SLOT_T>::request_reuse( void ) noexcept
{
	SLOT_T* p_ans = request_reuse_impl();
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	if ( p_ans != nullptr ) {
		auto p_slot_owner = p_ans->check_validity_to_ownwer_and_get();
		if ( p_slot_owner == nullptr ) {
			LogOutput( log_type::WARN, "retrieved_slots_mgr_impl<SLOT_T>::request_reuse() invalid SLOT_T" );
			return nullptr;
		}
		btinfo_alloc_free& cur_btinfo = p_slot_owner->get_btinfo( p_slot_owner->get_slot_idx( p_ans ) );
		cur_btinfo.alloc_trace_       = bt_info::record_backtrace();
		cur_btinfo.free_trace_.invalidate();
	}
#endif

	return p_ans;
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif
