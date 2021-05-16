/*!
 * @file	lf_mem_alloc.cpp
 * @brief	semi lock-free memory allocater
 * @author	Teruaki Ata
 * @date	Created on 2021/05/14
 * @details
 *
 * Copyright (C) 2021 by alpha Teruaki Ata <PFA03027@nifty.com>
 */

#include <algorithm>
#include <cstddef>
#include <vector>

#include "alconcurrent/lf_mem_alloc.hpp"

//#define ALLOC_ALG1
#define ALLOC_ALG2

namespace alpha {
namespace concurrent {

constexpr int	num_of_free_chk_try = 10;

template <typename T>
class scoped_inout_counter {
public:
	scoped_inout_counter( T& atomic_couter_ref_arg )
	  : atomic_couter_ref( atomic_couter_ref_arg )
	{
		atomic_couter_ref++;
	}

	~scoped_inout_counter()
	{
		atomic_couter_ref--;
	}

private:
	T& atomic_couter_ref;
};

chunk_header_multi_slot::chunk_header_multi_slot(
	const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
	)
  : p_next_chunk_( nullptr )
  , status_( chunk_control_status::EMPTY )
  , num_of_accesser_( 0 )
  , alloc_conf_( ch_param_arg )
  , p_free_slot_mark_( nullptr )
  , p_free_idx_array_( nullptr )
  , non_free_node_stack_head_( nullptr )
  , free_node_stack_head_( nullptr )
  , hint_idx_( 0 )
  , p_chunk_( nullptr )
  , statistics_alloc_req_cnt_( 0 )
  , statistics_alloc_req_err_cnt_( 0 )
  , statistics_dealloc_req_cnt_( 0 )
  , statistics_dealloc_req_err_cnt_( 0 )
{
	alloc_conf_.size_of_one_piece_ = get_size_of_one_slot();
	alloc_conf_.num_of_pieces_     = ( ch_param_arg.num_of_pieces_ >= 2 ) ? ch_param_arg.num_of_pieces_ : 2;

	alloc_new_chunk();

	return;
}

chunk_header_multi_slot::~chunk_header_multi_slot()
{
	delete[] p_free_slot_mark_;
	delete[] p_free_idx_array_;
	std::free( p_chunk_ );

	return;
}

std::size_t chunk_header_multi_slot::get_size_of_one_slot( void ) const
{
	std::size_t ans;
	std::size_t tmp = alloc_conf_.size_of_one_piece_ / sizeof( std::max_align_t );
	std::size_t adt = alloc_conf_.size_of_one_piece_ % sizeof( std::max_align_t );

	ans = ( tmp + ( ( adt > 0 ) ? 1 : 0 ) ) * sizeof( std::max_align_t );

	return ans;
}

bool chunk_header_multi_slot::alloc_new_chunk( void )
{
	// get ownership to allocate a chunk
	chunk_control_status expect = chunk_control_status::EMPTY;
	bool                 result = std::atomic_compare_exchange_strong( &status_, &expect, chunk_control_status::RESERVED_ALLOCATION );
	if ( !result ) return false;

	std::size_t tmp_size;
	tmp_size = alloc_conf_.size_of_one_piece_ * alloc_conf_.num_of_pieces_;

	if ( tmp_size == 0 ) {
		status_.store( chunk_control_status::EMPTY );
		return false;
	}

	p_chunk_ = std::malloc( tmp_size );
	if ( p_chunk_ == nullptr ) {
		status_.store( chunk_control_status::EMPTY );
		return false;
	}

	if ( p_free_slot_mark_ == nullptr ) {
		p_free_slot_mark_ = new std::atomic_bool[alloc_conf_.num_of_pieces_];
	}
	if ( p_free_idx_array_ == nullptr ) {
		p_free_idx_array_ = new free_slot_idx_stack_node[alloc_conf_.num_of_pieces_];
	}

	size_of_chunk_ = tmp_size;

	for ( std::size_t i = 0; i < alloc_conf_.num_of_pieces_; i++ ) {
		p_free_slot_mark_[i].store( true );
		p_free_idx_array_[i].idx_ = i;
		p_free_idx_array_[i].p_non_free_next_node_.store( nullptr );
		p_free_idx_array_[i].p_free_next_node_.store( free_node_stack_head_.load() );
		free_node_stack_head_.store( &( p_free_idx_array_[i] ) );
	}

	status_.store( chunk_control_status::NORMAL );

	return true;
}

void* chunk_header_multi_slot::allocate_mem_slot( void )
{
	if ( status_.load() != chunk_control_status::NORMAL ) return nullptr;

	scoped_inout_counter<std::atomic<int>> cnt_inout( num_of_accesser_ );

#ifdef ALLOC_ALG1
	free_slot_idx_stack_node* p_node;

	{
		// pop free slot stack
		free_slot_idx_stack_node* p_cur_head;
		free_slot_idx_stack_node* p_nxt;
		p_cur_head = free_node_stack_head_.load();
		do {
			if ( p_cur_head == nullptr ) return nullptr;

			p_nxt = p_cur_head->p_free_next_node_.load();

		} while ( !std::atomic_compare_exchange_strong( &free_node_stack_head_, &p_cur_head, p_nxt ) );

		p_node = p_cur_head;
	}

	int read_idx = p_node->idx_;
	p_free_slot_mark_[read_idx].store( false );
	p_node->idx_ = -1;
	statistics_alloc_req_cnt_++;

	{
		// push non free slot stack
		free_slot_idx_stack_node* p_cur_head;
		p_cur_head = non_free_node_stack_head_.load();
		do {
			p_node->p_non_free_next_node_.store( p_cur_head );
		} while ( !std::atomic_compare_exchange_strong( &non_free_node_stack_head_, &p_cur_head, p_node ) );
	}
#elif defined( ALLOC_ALG2 )
	statistics_alloc_req_cnt_++;
	int read_idx = -1;
	for ( int i = 0; i < num_of_free_chk_try; i++ ) {
		int  tmp_idx  = hint_idx_.load();
		int  tmp_idx2 = tmp_idx;
		bool expect   = true;
		bool ret      = std::atomic_compare_exchange_strong( &( p_free_slot_mark_[tmp_idx] ), &expect, false );
		int  expect2  = tmp_idx2 + 1;
		if ( expect2 >= static_cast<int>( alloc_conf_.num_of_pieces_ ) ) {
			expect2 = 0;
		}
		std::atomic_compare_exchange_strong( &( hint_idx_ ), &tmp_idx2, expect2 );
		if ( ret ) {
			read_idx = tmp_idx;
			break;
		}
	}
	if ( read_idx == -1 ) {
		statistics_alloc_req_err_cnt_++;
		return nullptr;
	}
#endif

	std::uintptr_t p_ans_addr = reinterpret_cast<std::uintptr_t>( p_chunk_ );
	p_ans_addr += read_idx * alloc_conf_.size_of_one_piece_;

	return reinterpret_cast<void*>( p_ans_addr );
}

bool chunk_header_multi_slot::recycle_mem_slot(
	void* p_recycle_slot   //!< [in] pointer to the memory slot to recycle.
)
{
	switch ( status_.load() ) {
		default:
		case chunk_control_status::EMPTY:
		case chunk_control_status::RESERVED_ALLOCATION:
		case chunk_control_status::DELETION:
			return false;
			break;

		case chunk_control_status::NORMAL:
		case chunk_control_status::RESERVED_DELETION:
			break;
	}

	scoped_inout_counter<std::atomic<int>> cnt_inout( num_of_accesser_ );

	// check correct address or not.
	if ( p_recycle_slot < p_chunk_ ) return false;

	std::uintptr_t p_chking_addr = reinterpret_cast<std::uintptr_t>( p_recycle_slot );
	p_chking_addr -= reinterpret_cast<std::uintptr_t>( p_chunk_ );

	std::uintptr_t idx = p_chking_addr / alloc_conf_.size_of_one_piece_;
	std::uintptr_t adt = p_chking_addr % alloc_conf_.size_of_one_piece_;

	if ( idx >= alloc_conf_.num_of_pieces_ ) return false;
	if ( adt != 0 ) return false;

	bool expect_not_free = false;
	bool result          = std::atomic_compare_exchange_strong( &p_free_slot_mark_[idx], &expect_not_free, true );

	if ( !result ) {
		// double free has occured.
		LogOutput( log_type::ERR, "double free has occured." );
		statistics_dealloc_req_err_cnt_++;
		return true;
	}

	// OK. correct address
#ifdef ALLOC_ALG1
	free_slot_idx_stack_node* p_node;

	{
		// pop non free slot stack
		free_slot_idx_stack_node* p_cur_head;
		free_slot_idx_stack_node* p_nxt;
		p_cur_head = non_free_node_stack_head_.load();
		do {
			if ( p_cur_head == nullptr ) {
				LogOutput( log_type::ERR, "internal error... over free ..." );
				statistics_dealloc_req_err_cnt_++;
				return true;
			}

			p_nxt = p_cur_head->p_non_free_next_node_.load();

		} while ( !std::atomic_compare_exchange_strong( &non_free_node_stack_head_, &p_cur_head, p_nxt ) );

		p_node = p_cur_head;
	}

	p_node->idx_ = idx;
	statistics_dealloc_req_cnt_++;

	{
		// push free slot stack
		free_slot_idx_stack_node* p_cur_head;
		p_cur_head = free_node_stack_head_.load();
		do {
			p_node->p_free_next_node_.store( p_cur_head );
		} while ( !std::atomic_compare_exchange_strong( &free_node_stack_head_, &p_cur_head, p_node ) );
	}
#elif defined( ALLOC_ALG2 )
	statistics_dealloc_req_cnt_++;
#endif

	return true;
}

const param_chunk_allocation& chunk_header_multi_slot::get_param( void ) const
{
	return alloc_conf_;
}

bool chunk_header_multi_slot::set_delete_reservation( void )
{
	chunk_control_status expect = chunk_control_status::NORMAL;
	bool                 result = std::atomic_compare_exchange_strong( &status_, &expect, chunk_control_status::RESERVED_DELETION );
	return result;
}

bool chunk_header_multi_slot::unset_delete_reservation( void )
{
	chunk_control_status expect = chunk_control_status::RESERVED_DELETION;
	bool                 result = std::atomic_compare_exchange_strong( &status_, &expect, chunk_control_status::NORMAL );
	return result;
}

bool chunk_header_multi_slot::exec_deletion( void )
{
	if ( num_of_accesser_.load() != 0 ) return false;

	chunk_control_status expect = chunk_control_status::RESERVED_DELETION;
	bool                 result = std::atomic_compare_exchange_strong( &status_, &expect, chunk_control_status::DELETION );

	if ( !result ) return false;

	std::free( p_chunk_ );
	p_chunk_ = nullptr;

	status_.store( chunk_control_status::EMPTY );

	return true;
}

chunk_statistics chunk_header_multi_slot::get_statistics( void ) const
{
	chunk_statistics ans;

	ans.alloc_conf_     = alloc_conf_;
	ans.chunk_num_      = 1;
	ans.total_slot_cnt_ = alloc_conf_.num_of_pieces_;
	ans.free_slot_cnt_  = 0;
	for ( std::size_t i = 0; i < alloc_conf_.num_of_pieces_; i++ ) {
		if ( p_free_slot_mark_[i].load() ) ans.free_slot_cnt_++;
	}
	ans.alloc_req_cnt_         = statistics_alloc_req_cnt_.load();
	ans.error_alloc_req_cnt_   = statistics_alloc_req_err_cnt_.load();
	ans.dealloc_req_cnt_       = statistics_dealloc_req_cnt_.load();
	ans.error_dealloc_req_cnt_ = statistics_dealloc_req_err_cnt_.load();

	return ans;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
chunk_list::chunk_list(
	const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
	)
  : alloc_conf_( ch_param_arg )
  , p_top_chunk_( nullptr )
{
	return;
}

chunk_list::~chunk_list()
{
	chunk_header_multi_slot* p_chms = p_top_chunk_.load();
	while ( p_chms != nullptr ) {
		chunk_header_multi_slot* p_next_chms = p_chms->p_next_chunk_.load();
		delete p_chms;
		p_chms = p_next_chms;
	}

	return;
}

void* chunk_list::allocate_mem_slot( void )
{
	void* p_ans = nullptr;

	chunk_header_multi_slot* p_cur_chms         = p_top_chunk_.load();
	chunk_header_multi_slot* p_1st_rsv_del_chms = nullptr;
	chunk_header_multi_slot* p_1st_empty_chms   = nullptr;
	while ( p_cur_chms != nullptr ) {
		p_ans = p_cur_chms->allocate_mem_slot();
		if ( p_ans != nullptr ) {
			return p_ans;
		}
		if ( p_cur_chms->status_.load() == chunk_control_status::RESERVED_DELETION ) {
			if ( p_1st_rsv_del_chms == nullptr ) {
				p_1st_rsv_del_chms = p_cur_chms;
			}
		}
		if ( p_cur_chms->status_.load() == chunk_control_status::EMPTY ) {
			if ( p_1st_empty_chms == nullptr ) {
				p_1st_empty_chms = p_cur_chms;
			}
		}
		chunk_header_multi_slot* p_next_chms = p_cur_chms->p_next_chunk_.load();
		p_cur_chms                           = p_next_chms;
	}

	if ( p_1st_rsv_del_chms != nullptr ) {
		if ( p_1st_rsv_del_chms->unset_delete_reservation() ) {
			p_ans = p_1st_rsv_del_chms->allocate_mem_slot();
			if ( p_ans != nullptr ) {
				return p_ans;
			}
		} else {
			if ( p_1st_rsv_del_chms->status_.load() == chunk_control_status::NORMAL ) {
				p_ans = p_1st_rsv_del_chms->allocate_mem_slot();
				if ( p_ans != nullptr ) {
					return p_ans;
				}
			}
		}
	}

	if ( p_1st_empty_chms != nullptr ) {
		if ( p_1st_empty_chms->alloc_new_chunk() ) {
			p_ans = p_1st_empty_chms->allocate_mem_slot();
			if ( p_ans != nullptr ) {
				return p_ans;
			}
		} else {
			if ( p_1st_empty_chms->status_.load() == chunk_control_status::NORMAL ) {
				p_ans = p_1st_empty_chms->allocate_mem_slot();
				if ( p_ans != nullptr ) {
					return p_ans;
				}
			}
		}
	}

	chunk_header_multi_slot* p_new_chms = new chunk_header_multi_slot( alloc_conf_ );
	p_ans                               = p_new_chms->allocate_mem_slot();
	if ( p_ans == nullptr ) return nullptr;   // TODO: should throw exception ?

	chunk_header_multi_slot* p_cur_top = nullptr;
	do {
		p_cur_top = p_top_chunk_.load();
		p_new_chms->p_next_chunk_.store( p_cur_top );
	} while ( !std::atomic_compare_exchange_strong( &p_top_chunk_, &p_cur_top, p_new_chms ) );

	return p_ans;
}

bool chunk_list::recycle_mem_slot(
	void* p_recycle_slot   //!< [in] pointer to the memory slot to recycle.
)
{
	chunk_header_multi_slot* p_cur_chms = p_top_chunk_.load();
	while ( p_cur_chms != nullptr ) {
		bool ret = p_cur_chms->recycle_mem_slot( p_recycle_slot );
		if ( ret ) return true;
		chunk_header_multi_slot* p_next_chms = p_cur_chms->p_next_chunk_.load();
		p_cur_chms                           = p_next_chms;
	}

	return false;
}

const param_chunk_allocation& chunk_list::get_param( void ) const
{
	return alloc_conf_;
}

/*!
 * @breif	get statistics
 */
chunk_statistics chunk_list::get_statistics( void ) const
{
	chunk_statistics ans;

	ans.alloc_conf_            = alloc_conf_;
	ans.chunk_num_             = 0;
	ans.total_slot_cnt_        = 0;
	ans.free_slot_cnt_         = 0;
	ans.alloc_req_cnt_         = 0;
	ans.error_alloc_req_cnt_   = 0;
	ans.dealloc_req_cnt_       = 0;
	ans.error_dealloc_req_cnt_ = 0;

	chunk_header_multi_slot* p_cur_chms = p_top_chunk_.load();
	while ( p_cur_chms != nullptr ) {
		chunk_statistics one_chms_statistics = p_cur_chms->get_statistics();
		ans.alloc_conf_                      = alloc_conf_;
		ans.chunk_num_ += one_chms_statistics.chunk_num_;
		ans.total_slot_cnt_ += one_chms_statistics.total_slot_cnt_;
		ans.free_slot_cnt_ += one_chms_statistics.free_slot_cnt_;
		ans.alloc_req_cnt_ += one_chms_statistics.alloc_req_cnt_;
		ans.error_alloc_req_cnt_ += one_chms_statistics.error_alloc_req_cnt_;
		ans.dealloc_req_cnt_ += one_chms_statistics.dealloc_req_cnt_;
		ans.error_dealloc_req_cnt_ += one_chms_statistics.error_dealloc_req_cnt_;

		chunk_header_multi_slot* p_nxt_chms = p_cur_chms->p_next_chunk_.load();
		p_cur_chms                          = p_nxt_chms;
	}

	return ans;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
general_mem_allocator::general_mem_allocator(
	const param_chunk_allocation* p_param_array,   //!< [in] pointer to parameter array
	int                           num              //!< [in] array size
	)
  : pr_ch_size_( num )
{
	std::vector<int> idx_vec( pr_ch_size_ );

	for ( int i = 0; i < pr_ch_size_; i++ ) {
		idx_vec[i] = i;
	}

	std::sort( idx_vec.begin(), idx_vec.end(),
	           [p_param_array]( int a, int b ) {
				   return p_param_array[a].size_of_one_piece_ < p_param_array[b].size_of_one_piece_;
			   } );

	up_param_ch_array_ = std::unique_ptr<param_chunk_comb[]>( new param_chunk_comb[pr_ch_size_] );
	for ( int i = 0; i < pr_ch_size_; i++ ) {
		up_param_ch_array_[i].param_        = p_param_array[idx_vec[i]];
		up_param_ch_array_[i].up_chunk_lst_ = std::unique_ptr<chunk_list>( new chunk_list( up_param_ch_array_[i].param_ ) );
	}

	return;
}

void* general_mem_allocator::allocate(
	std::size_t n   //!< [in] required memory size
)
{
	void* p_ans = nullptr;
	for ( int i = 0; i < pr_ch_size_; i++ ) {
		if ( up_param_ch_array_[i].param_.size_of_one_piece_ >= n ) {
			p_ans = up_param_ch_array_[i].up_chunk_lst_->allocate_mem_slot();
			if ( p_ans != nullptr ) {
				return p_ans;
			}
		}
	}

	p_ans = std::malloc( n );
	return p_ans;
}

void general_mem_allocator::deallocate(
	void* p_mem   //!< [in] pointer to allocated memory by allocate()
)
{
	for ( int i = 0; i < pr_ch_size_; i++ ) {
		bool ret = up_param_ch_array_[i].up_chunk_lst_->recycle_mem_slot( p_mem );
		if ( ret ) return;
	}

	std::free( p_mem );

	return;
}

/*!
 * @breif	get statistics
 */
std::list<chunk_statistics> general_mem_allocator::get_statistics( void ) const
{
	std::list<chunk_statistics> ans;

	for ( int i = 0; i < pr_ch_size_; i++ ) {
		chunk_statistics tmp_st = up_param_ch_array_[i].up_chunk_lst_->get_statistics();
		ans.push_back( tmp_st );
	}

	return ans;
}

}   // namespace concurrent
}   // namespace alpha
