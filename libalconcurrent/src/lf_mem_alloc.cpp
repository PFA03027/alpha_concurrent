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
#include <exception>
#include <stdexcept>
#include <vector>

#include "alconcurrent/lf_mem_alloc.hpp"
#include "alconcurrent/lf_mem_alloc_idx_mgr.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

idx_mgr_element::idx_mgr_element( void )
  : idx_( -1 )
  , p_invalid_idx_next_element_( nullptr )
  , p_valid_idx_next_element_( nullptr )
  , p_waiting_next_element_( nullptr )
{
	return;
}

/*!
 * @breif	dump for debug
 */
void idx_mgr_element::dump( void ) const
{
	char buf[2048];
	snprintf( buf, 2047,
	          "object idx_mgr_element_%p as %p {\n"
	          "\t idx_ = %d\n"
	          "\t p_invalid_idx_next_element_ = %p\n"
	          "\t p_valid_idx_next_element_ = %p\n"
	          "\t p_waiting_next_element_ = %p\n"
	          "}\n",
	          this, this,
	          idx_,
	          p_invalid_idx_next_element_.load(),
	          p_valid_idx_next_element_.load(),
	          p_waiting_next_element_ );
	LogOutput( log_type::DUMP, buf );

	if ( p_invalid_idx_next_element_.load() != nullptr ) {
		snprintf( buf, 2047,
		          "%p --> %p : invalid\n",
		          this, p_invalid_idx_next_element_.load() );
		LogOutput( log_type::DUMP, buf );
	}

	if ( p_valid_idx_next_element_.load() != nullptr ) {
		snprintf( buf, 2047,
		          "%p --> %p : valid\n",
		          this, p_valid_idx_next_element_.load() );
		LogOutput( log_type::DUMP, buf );
	}

	if ( p_waiting_next_element_ != nullptr ) {
		snprintf( buf, 2047,
		          "%p --> %p : waiting\n",
		          this, p_waiting_next_element_ );
		LogOutput( log_type::DUMP, buf );
	}

	return;
}

waiting_element_list::waiting_element_list( const int idx_buff_size_arg )
  : head_( nullptr )
  , tail_( nullptr )
  , idx_buff_size_( idx_buff_size_arg )
  , idx_top_idx_( 0 )
{
	p_idx_buff_ = new int[idx_buff_size_];
	return;
}

waiting_element_list::~waiting_element_list()
{
	delete[] p_idx_buff_;
}

void waiting_element_list::reset_and_set_size( const int idx_buff_size_arg )
{
	delete[] p_idx_buff_;
	idx_buff_size_ = idx_buff_size_arg;
	p_idx_buff_    = new int[idx_buff_size_];
	head_          = nullptr;
	tail_          = nullptr;
	idx_top_idx_   = 0;
	return;
}

idx_mgr_element* waiting_element_list::pop( void )
{
	idx_mgr_element* p_ans = nullptr;

	if ( head_ == nullptr ) return nullptr;

	p_ans = head_;
	head_ = head_->p_waiting_next_element_;
	if ( head_ == nullptr ) {
		tail_ = nullptr;
	}

	return p_ans;
}

void waiting_element_list::push(
	idx_mgr_element* p_el_arg   //
)
{
	p_el_arg->p_waiting_next_element_ = nullptr;

	if ( tail_ != nullptr ) {
		tail_->p_waiting_next_element_ = p_el_arg;
		tail_                          = p_el_arg;
	} else {
		head_ = p_el_arg;
		tail_ = p_el_arg;
	}

	return;
}

int waiting_element_list::pop_from_tls( void )
{
	int ans = -1;

	if ( idx_top_idx_ <= 0 ) return ans;

	idx_top_idx_--;

	ans                       = p_idx_buff_[idx_top_idx_];
	p_idx_buff_[idx_top_idx_] = -1;

	return ans;
}

void waiting_element_list::push_to_tls( const int valid_idx )
{
	if ( idx_top_idx_ >= idx_buff_size_ ) {
		throw std::exception();
		return;
	}

	p_idx_buff_[idx_top_idx_] = valid_idx;
	idx_top_idx_++;

	return;
}

void waiting_element_list::dump( void ) const
{
	char buf[2048];

	snprintf( buf, 2047,
	          "object waiting_element_list_%p as %p {\n"
	          "\t head_ = %p\n"
	          "\t tail_ = %p\n"
	          "\t idx_buff_size_ = %d\n"
	          "\t idx_top_idx_ = %d\n"
	          "\t p_idx_buff_ = %p\n"
	          "}\n",
	          this, this,
	          head_,
	          tail_,
	          idx_buff_size_,
	          idx_top_idx_,
	          p_idx_buff_ );
	LogOutput( log_type::DUMP, buf );

	if ( p_idx_buff_ != nullptr ) {
		snprintf( buf, 2047,
		          "object p_idx_buff_%p as %p {\n",
		          this, this );
		LogOutput( log_type::DUMP, buf );

		for ( int i = 0; i < idx_buff_size_; i++ ) {
			snprintf( buf, 2047,
			          "\t %d => %d\n",
			          i, p_idx_buff_[i] );
			LogOutput( log_type::DUMP, buf );
		}

		LogOutput( log_type::DUMP, "}\n" );
	}

	if ( head_ != nullptr ) {
		snprintf( buf, 2047,
		          "%p --> %p\n",
		          this, head_ );
		LogOutput( log_type::DUMP, buf );
	}

	if ( tail_ != nullptr ) {
		snprintf( buf, 2047,
		          "%p --> %p\n",
		          this, tail_ );
		LogOutput( log_type::DUMP, buf );
	}

	return;
}

/*!
 * @breif	コンストラクタ
 */
idx_mgr::idx_mgr(
	const int idx_size_arg   //!< [in] 用意するインデックス番号の数。-1の場合、割り当てを保留する。
	)
  : idx_size_( idx_size_arg )
  , p_idx_mgr_element_array_( nullptr )
  , invalid_element_stack_head_( nullptr )
  , valid_element_stack_head_( nullptr )
{
	if ( idx_size_ < 0 ) return;

	tls_waiting_list_.get_tls_instance( idx_size_ );

	set_idx_size( idx_size_ );
	return;
}

idx_mgr::~idx_mgr()
{
	delete[] p_idx_mgr_element_array_;
}

void idx_mgr::set_idx_size( const int idx_size_arg )
{
	if ( idx_size_arg <= 0 ) {
		idx_size_ = -1;
		delete[] p_idx_mgr_element_array_;
		p_idx_mgr_element_array_ = nullptr;
		return;
	}

	if ( p_idx_mgr_element_array_ == nullptr ) {
		p_idx_mgr_element_array_ = new idx_mgr_element[idx_size_arg];
	} else {
		if ( idx_size_arg > idx_size_ ) {
			delete[] p_idx_mgr_element_array_;
			p_idx_mgr_element_array_ = new idx_mgr_element[idx_size_arg];
		}
	}
	idx_size_ = idx_size_arg;

	idx_mgr_element* tmp_head = nullptr;
	for ( int i = 0; i < idx_size_; i++ ) {
		p_idx_mgr_element_array_[i].idx_ = i;
		p_idx_mgr_element_array_[i].p_valid_idx_next_element_.store( tmp_head );
		tmp_head = &( p_idx_mgr_element_array_[i] );
		p_idx_mgr_element_array_[i].p_invalid_idx_next_element_.store( nullptr );
		p_idx_mgr_element_array_[i].p_waiting_next_element_ = nullptr;
	}
	valid_element_stack_head_.store( tmp_head );

	invalid_element_stack_head_.store( nullptr );

	return;
}

/*!
 * @breif	利用可能なインデックス番号を取り出す。
 *
 * @return	利用可能なインデックス番号
 * @retval	-1	利用可能なインデックス番号がない
 *
 * @note
 * このI/Fはスレッドセーフで、ロックフリーです。
 */
idx_mgr_element* idx_mgr::stack_pop_element(
	std::atomic<idx_mgr_element*>& head_,                               //!< [in] reference of atomic pointer to stack head
	std::atomic<idx_mgr_element*> idx_mgr_element::*p_next,             //!< [in] member pointer that points to next element
	const hazard_ptr_idx                            hzd_pop_cur_slot,   //!< [in] hazard pointer slot to store pop current pointer
	const hazard_ptr_idx                            hzd_pop_next_slot   //!< [in] hazard pointer slot to store pop next pointer
)
{
	scoped_hazard_ref scoped_ref_first( hzrd_element_, (int)hzd_pop_cur_slot );
	scoped_hazard_ref scoped_ref_next( hzrd_element_, (int)hzd_pop_next_slot );

	while ( true ) {
		idx_mgr_element* p_cur_first = head_.load( std::memory_order_acquire );
		scoped_ref_first.regist_ptr_as_hazard_ptr( p_cur_first );
		if ( p_cur_first != head_.load( std::memory_order_acquire ) ) continue;

		if ( p_cur_first == nullptr ) {
			// stackは空。
			return nullptr;
		}

		idx_mgr_element* p_cur_next = ( p_cur_first->*p_next ).load();
		scoped_ref_next.regist_ptr_as_hazard_ptr( p_cur_next );
		if ( p_cur_next != ( p_cur_first->*p_next ).load() ) continue;

		// ここで、プリエンプションして、head_がA->B->A'となった時、p_cur_nextが期待値とは異なるが、
		// ハザードポインタにA相当を確保しているので、A'は現れない。よって、このようなABA問題は起きない。
		if ( head_.compare_exchange_weak( p_cur_first, p_cur_next ) ) {
			// ここで、firstの取り出しと所有権確保が完了
			// ただし、ハザードポインタをチェックしていないため、まだ参照している人がいるかもしれない。
			return p_cur_first;
		}
	}
	return nullptr;
}

void idx_mgr::stack_push_element(
	idx_mgr_element*               p_push_element,                      //!< [in] pointer of element to push
	std::atomic<idx_mgr_element*>& head_,                               //!< [in] reference of atomic pointer to stack head
	std::atomic<idx_mgr_element*> idx_mgr_element::*p_next,             //!< [in] member pointer that points to next element
	const hazard_ptr_idx                            hzd_push_cur_slot   //!< [in] hazard pointer slot to store pop current pointer
)
{
	( p_push_element->*p_next ).store( nullptr );

	scoped_hazard_ref scoped_ref_cur( hzrd_element_, (int)hzd_push_cur_slot );

	while ( true ) {
		idx_mgr_element* p_cur_top = head_.load( std::memory_order_acquire );
		scoped_ref_cur.regist_ptr_as_hazard_ptr( p_cur_top );
		if ( p_cur_top != head_.load( std::memory_order_acquire ) ) continue;

		( p_push_element->*p_next ).store( p_cur_top );

		// head_を更新する。
		// ここで、プリエンプションして、tail_がA->B->A'となった時、p_cur_lastが期待値とは異なるが、
		// ハザードポインタにA相当を確保しているので、A'は現れない。よって、このようなABA問題は起きない。
		if ( head_.compare_exchange_weak( p_cur_top, p_push_element ) ) {
			return;   // 追加完了
		}
	}
	return;
}

int idx_mgr::pop( void )
{
	int                   ans       = -1;
	waiting_element_list& wait_list = tls_waiting_list_.get_tls_instance( idx_size_ );

	ans = wait_list.pop_from_tls();
	if ( ans != -1 ) {
		return ans;
	}

	idx_mgr_element* p_valid_pop_element = stack_pop_element(
		valid_element_stack_head_,
		&idx_mgr_element::p_valid_idx_next_element_,
		hazard_ptr_idx::POP_VAL_CUR,
		hazard_ptr_idx::POP_VAL_NEXT );

	if ( p_valid_pop_element == nullptr ) return -1;

	ans                       = p_valid_pop_element->idx_;
	p_valid_pop_element->idx_ = -1;

	wait_list.push( p_valid_pop_element );

	return ans;
}

void idx_mgr::push(
	const int idx_arg   //!< [in] 返却するインデックス番号
)
{
	waiting_element_list& wait_list = tls_waiting_list_.get_tls_instance( idx_size_ );

	idx_mgr_element* p_invalid_element       = wait_list.pop();
	idx_mgr_element* p_invalid_element_first = nullptr;
	int              ch_cnt                  = 2;

	while ( p_invalid_element != nullptr ) {
		if ( !hzrd_element_.check_ptr_in_hazard_list( p_invalid_element ) ) {
			stack_push_element(
				p_invalid_element,
				invalid_element_stack_head_,
				&idx_mgr_element::p_invalid_idx_next_element_,
				hazard_ptr_idx::PUSH_INV_CUR );
		} else {
			if ( p_invalid_element_first == nullptr ) {
				p_invalid_element_first = p_invalid_element;
			}
			wait_list.push( p_invalid_element );
		}
		if ( ch_cnt <= 0 ) {
			break;
		}
		ch_cnt--;
		p_invalid_element = wait_list.pop();
	}

	p_invalid_element = stack_pop_element(
		invalid_element_stack_head_,
		&idx_mgr_element::p_invalid_idx_next_element_,
		hazard_ptr_idx::POP_INV_CUR,
		hazard_ptr_idx::POP_INV_NEXT );

	if ( p_invalid_element == nullptr ) {
		wait_list.push_to_tls( idx_arg );
	} else {

		p_invalid_element->idx_ = idx_arg;

		stack_push_element(
			p_invalid_element,
			valid_element_stack_head_,
			&idx_mgr_element::p_valid_idx_next_element_,
			hazard_ptr_idx::PUSH_VAL_CUR );
	}

	return;
}

void idx_mgr::dump( void )
{
	char buf[2048];

	const waiting_element_list& tmp_wel = tls_waiting_list_.get_tls_instance( idx_size_ );

	snprintf( buf, 2047,
	          "object idx_mgr_%p as %p {\n"
	          "\t idx_size_ = %d\n"
	          "\t invalid_element_stack_head_ = %p\n"
	          "\t valid_element_stack_head_ = %p\n"
	          "\t waiting_element_list = %p\n"
	          "\t hzrd_element_ = (not implemented)\n"
	          "}\n",
	          this, this,
	          idx_size_,
	          invalid_element_stack_head_.load(),
	          valid_element_stack_head_.load(),
	          &tmp_wel );
	LogOutput( log_type::DUMP, buf );

	if ( p_idx_mgr_element_array_ != nullptr ) {
		snprintf( buf, 2047,
		          "%p --> %p \n",
		          this, p_idx_mgr_element_array_ );
		LogOutput( log_type::DUMP, buf );
	}

	if ( invalid_element_stack_head_.load() != nullptr ) {
		snprintf( buf, 2047,
		          "%p --> %p \n",
		          this, invalid_element_stack_head_.load() );
		LogOutput( log_type::DUMP, buf );
	}

	if ( valid_element_stack_head_.load() != nullptr ) {
		snprintf( buf, 2047,
		          "%p --> %p \n",
		          this, valid_element_stack_head_.load() );
		LogOutput( log_type::DUMP, buf );
	}

	snprintf( buf, 2047,
	          "%p --> %p \n",
	          this, &tmp_wel );
	LogOutput( log_type::DUMP, buf );

	if ( p_idx_mgr_element_array_ != nullptr ) {
		for ( int i = 0; i < idx_size_; i++ ) {
			p_idx_mgr_element_array_[i].dump();
		}
	}

	tmp_wel.dump();

	return;
}

}   // namespace internal

constexpr int num_of_free_chk_try = 10;

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
  , free_slot_idx_mgr_( -1 )
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
		for ( std::size_t i = 0; i < alloc_conf_.num_of_pieces_; i++ ) {
			p_free_slot_mark_[i].store( true );
		}
	}

	size_of_chunk_ = tmp_size;

	free_slot_idx_mgr_.set_idx_size( alloc_conf_.num_of_pieces_ );

	status_.store( chunk_control_status::NORMAL );

	return true;
}

void* chunk_header_multi_slot::allocate_mem_slot( void )
{
	if ( status_.load() != chunk_control_status::NORMAL ) return nullptr;

	scoped_inout_counter<std::atomic<int>> cnt_inout( num_of_accesser_ );

	statistics_alloc_req_cnt_++;

	int read_idx = free_slot_idx_mgr_.pop();
	if ( read_idx == -1 ) {
		statistics_alloc_req_err_cnt_++;
		return nullptr;
	}

	p_free_slot_mark_[read_idx].store( false );

	std::uintptr_t p_ans_addr = reinterpret_cast<std::uintptr_t>( p_chunk_ );
	p_ans_addr += read_idx * alloc_conf_.size_of_one_piece_;

	return reinterpret_cast<void*>( p_ans_addr );
}

bool chunk_header_multi_slot::recycle_mem_slot(
	void* p_recycle_slot   //!< [in] pointer to the memory slot to recycle.
)
{
	switch ( status_.load( std::memory_order_acquire ) ) {
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
		//		fflush( NULL );
		statistics_dealloc_req_err_cnt_++;
		return true;
	}

	// OK. correct address
	free_slot_idx_mgr_.push( idx );
	statistics_dealloc_req_cnt_++;

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

	delete[] p_free_slot_mark_;
	p_free_slot_mark_ = nullptr;

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

/*!
 * @breif	dump for debug
 */
void chunk_header_multi_slot::dump( void )
{
	char buf[2048];

	if ( p_chunk_ != nullptr ) {
		snprintf( buf, 2047,
		          "object chunk_%p as %p \n",
		          p_chunk_, p_chunk_ );
		LogOutput( log_type::DUMP, buf );
	}

	if ( p_free_slot_mark_ != nullptr ) {
		snprintf( buf, 2047,
		          "object p_free_slot_mark_%p as %p {\n",
		          p_free_slot_mark_, p_free_slot_mark_ );
		LogOutput( log_type::DUMP, buf );

		for ( std::size_t i = 0; i < alloc_conf_.num_of_pieces_; i++ ) {
			snprintf( buf, 2047,
			          "%zu = %s \n",
			          i, p_free_slot_mark_[i].load() ? "true" : "false" );
			LogOutput( log_type::DUMP, buf );
		}
		LogOutput( log_type::DUMP, "}\n" );
	}

	snprintf( buf, 2047,
	          "object chunk_header_multi_slot_%p as %p {\n"
	          "\t alloc_conf_.size_of_one_piece_ = %zu \n"
	          "\t alloc_conf_.num_of_pieces_ = %zu \n"
	          "\t size_of_chunk_ = %zu \n"
	          "\t p_free_slot_mark_ = %p \n"
	          "\t p_chunk_ = %p \n"
	          "\t free_slot_idx_mgr_ = %p \n"
	          "}\n",
	          this, this,
	          alloc_conf_.size_of_one_piece_,
	          alloc_conf_.num_of_pieces_,
	          size_of_chunk_,
	          p_free_slot_mark_,
	          p_chunk_,
	          &free_slot_idx_mgr_ );
	LogOutput( log_type::DUMP, buf );

	free_slot_idx_mgr_.dump();

	return;
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

	chunk_header_multi_slot* p_cur_chms         = p_top_chunk_.load( std::memory_order_acquire );
	chunk_header_multi_slot* p_1st_rsv_del_chms = nullptr;
	chunk_header_multi_slot* p_1st_empty_chms   = nullptr;
	while ( p_cur_chms != nullptr ) {
		p_ans = p_cur_chms->allocate_mem_slot();
		if ( p_ans != nullptr ) {
			return p_ans;
		}
		if ( p_cur_chms->status_.load( std::memory_order_acquire ) == chunk_control_status::RESERVED_DELETION ) {
			if ( p_1st_rsv_del_chms == nullptr ) {
				p_1st_rsv_del_chms = p_cur_chms;
			}
		}
		if ( p_cur_chms->status_.load( std::memory_order_acquire ) == chunk_control_status::EMPTY ) {
			if ( p_1st_empty_chms == nullptr ) {
				p_1st_empty_chms = p_cur_chms;
			}
		}
		chunk_header_multi_slot* p_next_chms = p_cur_chms->p_next_chunk_.load( std::memory_order_acquire );
		p_cur_chms                           = p_next_chms;
	}

	if ( p_1st_rsv_del_chms != nullptr ) {
		if ( p_1st_rsv_del_chms->unset_delete_reservation() ) {
			p_ans = p_1st_rsv_del_chms->allocate_mem_slot();
			if ( p_ans != nullptr ) {
				return p_ans;
			}
		} else {
			if ( p_1st_rsv_del_chms->status_.load( std::memory_order_acquire ) == chunk_control_status::NORMAL ) {
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
			if ( p_1st_empty_chms->status_.load( std::memory_order_acquire ) == chunk_control_status::NORMAL ) {
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
		p_cur_top = p_top_chunk_.load( std::memory_order_acquire );
		p_new_chms->p_next_chunk_.store( p_cur_top );
	} while ( !std::atomic_compare_exchange_strong( &p_top_chunk_, &p_cur_top, p_new_chms ) );

	return p_ans;
}

bool chunk_list::recycle_mem_slot(
	void* p_recycle_slot   //!< [in] pointer to the memory slot to recycle.
)
{
	chunk_header_multi_slot* p_cur_chms = p_top_chunk_.load( std::memory_order_acquire );
	while ( p_cur_chms != nullptr ) {
		bool ret = p_cur_chms->recycle_mem_slot( p_recycle_slot );
		if ( ret ) return true;
		chunk_header_multi_slot* p_next_chms = p_cur_chms->p_next_chunk_.load( std::memory_order_acquire );
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
