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

namespace alpha {
namespace concurrent {

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
  , free_slot_stack_( ch_param_arg.num_of_pieces_ )
{
	alloc_new_chunk( ch_param_arg );
	return;
}

chunk_header_multi_slot::~chunk_header_multi_slot()
{
	std::free( p_chunk_ );
}

std::size_t chunk_header_multi_slot::get_size_of_one_slot( void ) const
{
	std::size_t ans;
	std::size_t tmp = alloc_conf_.size_of_one_piece_ / sizeof( std::max_align_t );
	std::size_t adt = alloc_conf_.size_of_one_piece_ % sizeof( std::max_align_t );

	ans = ( tmp + ( ( adt > 0 ) ? 1 : 0 ) ) * sizeof( std::max_align_t );

	return ans;
}

bool chunk_header_multi_slot::alloc_new_chunk(
	const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
)
{
	// get ownership to allocate a chunk
	chunk_control_status expect = chunk_control_status::EMPTY;
	bool                 result = std::atomic_compare_exchange_strong( &status_, &expect, chunk_control_status::RESERVED_ALLOCATION );
	if ( !result ) return false;

	alloc_conf_ = ch_param_arg;

	std::size_t tmp_size;
	tmp_size = get_size_of_one_slot() * alloc_conf_.num_of_pieces_;

	if ( tmp_size == 0 ) {
		status_.store( chunk_control_status::EMPTY );
		return false;
	}

	p_chunk_ = std::malloc( tmp_size );
	if ( p_chunk_ == nullptr ) {
		status_.store( chunk_control_status::EMPTY );
		return false;
	}

	size_of_chunk_ = tmp_size;

	for ( std::size_t i = 0; i < alloc_conf_.num_of_pieces_; i++ ) {
		std::uintptr_t p_slot_addr = reinterpret_cast<std::uintptr_t>( p_chunk_ );

		p_slot_addr += i * get_size_of_one_slot();
		free_slot_stack_.push( reinterpret_cast<void*>( p_slot_addr ) );
	}

	status_.store( chunk_control_status::NORMAL );

	return true;
}

void* chunk_header_multi_slot::allocate_mem_slot( void )
{
	if ( status_.load() != chunk_control_status::NORMAL ) return nullptr;

	scoped_inout_counter<std::atomic<int>> cnt_inout( num_of_accesser_ );

	auto  ret   = free_slot_stack_.pop();
	void* p_ans = nullptr;
	if ( std::get<0>( ret ) ) {
		p_ans = std::get<1>( ret );
	}

	return p_ans;
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

	std::uintptr_t idx = p_chking_addr / get_size_of_one_slot();
	std::uintptr_t adt = p_chking_addr % get_size_of_one_slot();

	if ( idx >= alloc_conf_.num_of_pieces_ ) return false;
	if ( adt != 0 ) return false;

	// OK. correct address
	free_slot_stack_.push( p_recycle_slot );

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

bool chunk_header_multi_slot::alloc_new_chunk( void )
{
	return alloc_new_chunk( alloc_conf_ );
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

}   // namespace concurrent
}   // namespace alpha
