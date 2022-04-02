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
#include <atomic>
#include <cstddef>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/lf_mem_alloc.hpp"
#include "alconcurrent/lf_mem_alloc_internal.hpp"

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
	char buf[CONF_LOGGER_INTERNAL_BUFF_SIZE];
	snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
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
	internal::LogOutput( log_type::DUMP, buf );

	if ( p_invalid_idx_next_element_.load() != nullptr ) {
		snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
		          "%p --> %p : invalid\n",
		          this, p_invalid_idx_next_element_.load() );
		internal::LogOutput( log_type::DUMP, buf );
	}

	if ( p_valid_idx_next_element_.load() != nullptr ) {
		snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
		          "%p --> %p : valid\n",
		          this, p_valid_idx_next_element_.load() );
		internal::LogOutput( log_type::DUMP, buf );
	}

	if ( p_waiting_next_element_ != nullptr ) {
		snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
		          "%p --> %p : waiting\n",
		          this, p_waiting_next_element_ );
		internal::LogOutput( log_type::DUMP, buf );
	}

	return;
}

waiting_element_list::waiting_element_list( void )
  : head_( nullptr )
  , tail_( nullptr )
{
	return;
}

waiting_element_list::~waiting_element_list()
{
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

void waiting_element_list::dump( void ) const
{
	char buf[CONF_LOGGER_INTERNAL_BUFF_SIZE];

	snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
	          "object waiting_element_list_%p as %p {\n"
	          "\t head_ = %p\n"
	          "\t tail_ = %p\n"
	          "}\n",
	          this, this,
	          head_,
	          tail_ );
	internal::LogOutput( log_type::DUMP, buf );

	if ( head_ != nullptr ) {
		snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
		          "%p --> %p\n",
		          this, head_ );
		internal::LogOutput( log_type::DUMP, buf );
	}

	if ( tail_ != nullptr ) {
		snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
		          "%p --> %p\n",
		          this, tail_ );
		internal::LogOutput( log_type::DUMP, buf );
	}

	return;
}

////////////////////////////////////////////////////////////////////////////////
/*!
 * @breif	インデックス管理スロットのロックフリーストレージクラス
 */
idx_element_storage_mgr::idx_element_storage_mgr(
	std::atomic<idx_mgr_element*> idx_mgr_element::*p_next_ptr_offset_arg   //!< [in] nextポインタを保持しているメンバ変数へのメンバ変数ポインタ
	)
  : tls_waiting_list_( rcv_el_by_thread_terminating( this ) )
  , head_( nullptr )
  , tail_( nullptr )
  , p_next_ptr_offset_( p_next_ptr_offset_arg )
  , rcv_wait_element_list_()
  , collision_cnt_( 0 )
{
	return;
}

/*!
 * @breif	ストレージから１つの要素を取り出す
 *
 * @return	pointer to the poped element
 * @retval	nullptr	no available element
 */
idx_mgr_element* idx_element_storage_mgr::pop_element( void )
{
	idx_mgr_element*      p_ans     = nullptr;
	waiting_element_list& wait_list = tls_waiting_list_.get_tls_instance();

	p_ans = wait_list.pop();
	if ( p_ans != nullptr ) {
		// スレッドローカルストレージからノードが取り出せたので、さらに1回リサイクルを試みる
		idx_mgr_element* p_tmp_recycle = wait_list.pop();
		if ( p_tmp_recycle != nullptr ) {
			if ( hzrd_element_.check_ptr_in_hazard_list( p_tmp_recycle ) ) {
				// まだハザードポインタに登録されていたので、スレッドローカルストレージに差し戻す
				wait_list.push( p_tmp_recycle );
			} else {
				// ハザードポインタに登録されていなかったので、共有リストへpush
				push_element_to_list( p_tmp_recycle );
			}
		}
	} else {
		// スレッドローカルストレージは空だったので、共有リストから取り出す。
		p_ans = pop_element_from_list();

		if ( p_ans == nullptr ) {
			// 共有リストからの取得も出来なかったため、回収されたリストを確認する。
			std::unique_lock<std::mutex> lk( mtx_rcv_wait_element_list_, std::try_to_lock );
			if ( lk.owns_lock() ) {
				p_ans = rcv_wait_element_list_.pop();
			}
		}
	}

	return p_ans;
}

/*!
 * @breif	ストレージへ１つの要素を戻す
 */
void idx_element_storage_mgr::push_element(
	idx_mgr_element* p_push_element   //!< [in] pointer of element to push
)
{
	waiting_element_list& wait_list     = tls_waiting_list_.get_tls_instance();
	idx_mgr_element*      p_tmp_recycle = wait_list.pop();   // 新たに登録されるポインタとのチェックが重複しないように、先に読みだしておく

	if ( hzrd_element_.check_ptr_in_hazard_list( p_push_element ) ) {
		// まだハザードポインタに登録されていたので、スレッドローカルストレージに戻す
		wait_list.push( p_push_element );
	} else {
		// ハザードポインタに登録されていなかったので、共有リストへpush
		push_element_to_list( p_push_element );
	}

	if ( p_tmp_recycle != nullptr ) {
		if ( hzrd_element_.check_ptr_in_hazard_list( p_tmp_recycle ) ) {
			// まだハザードポインタに登録されていたので、スレッドローカルストレージに差し戻す
			wait_list.push( p_tmp_recycle );
		} else {
			// ハザードポインタに登録されていなかったので、共有リストへpush
			push_element_to_list( p_tmp_recycle );
		}
	}

	return;
}

idx_mgr_element* idx_element_storage_mgr::pop_element_from_list( void )
{
	scoped_hazard_ref scoped_ref_first( hzrd_element_, (int)hazard_ptr_idx::POP_CUR );
	scoped_hazard_ref scoped_ref_next( hzrd_element_, (int)hazard_ptr_idx::POP_NEXT );

	while ( true ) {
		idx_mgr_element* p_cur_first = head_.load( std::memory_order_acquire );
		scoped_ref_first.regist_ptr_as_hazard_ptr( p_cur_first );
		if ( p_cur_first != head_.load( std::memory_order_acquire ) ) {
			collision_cnt_++;
			continue;
		}

		if ( p_cur_first == nullptr ) {
			// stackは空。
			return nullptr;
		}

		idx_mgr_element* p_cur_next = ( p_cur_first->*p_next_ptr_offset_ ).load();
		scoped_ref_next.regist_ptr_as_hazard_ptr( p_cur_next );
		if ( p_cur_next != ( p_cur_first->*p_next_ptr_offset_ ).load() ) {
			collision_cnt_++;
			continue;
		}

		// ここで、プリエンプションして、head_がA->B->A'となった時、p_cur_nextが期待値とは異なるが、
		// ハザードポインタにA相当を確保しているので、A'は現れない。よって、このようなABA問題は起きない。
		if ( head_.compare_exchange_weak( p_cur_first, p_cur_next ) ) {
			// ここで、firstの取り出しと所有権確保が完了
			// ただし、ハザードポインタをチェックしていないため、まだ参照している人がいるかもしれない。
			return p_cur_first;
		}
		collision_cnt_++;
	}
	return nullptr;
}

/*!
 * @breif	ストレージへ１つの要素を戻す
 */
void idx_element_storage_mgr::push_element_to_list(
	idx_mgr_element* p_push_element   //!< [in] pointer of element to push
)
{
	( p_push_element->*p_next_ptr_offset_ ).store( nullptr );

	scoped_hazard_ref scoped_ref_cur( hzrd_element_, (int)hazard_ptr_idx::PUSH_CUR );

	while ( true ) {
		idx_mgr_element* p_cur_top = head_.load( std::memory_order_acquire );
		scoped_ref_cur.regist_ptr_as_hazard_ptr( p_cur_top );
		if ( p_cur_top != head_.load( std::memory_order_acquire ) ) {
			collision_cnt_++;
			continue;
		}

		( p_push_element->*p_next_ptr_offset_ ).store( p_cur_top );

		// head_を更新する。
		// ここで、プリエンプションして、tail_がA->B->A'となった時、p_cur_lastが期待値とは異なるが、
		// ハザードポインタにA相当を確保しているので、A'は現れない。よって、このようなABA問題は起きない。
		if ( head_.compare_exchange_weak( p_cur_top, p_push_element ) ) {
			return;   // 追加完了
		}
		collision_cnt_++;
	}
	return;
}

void idx_element_storage_mgr::rcv_wait_element_by_thread_terminating( waiting_element_list* p_el_list )
{
	std::lock_guard<std::mutex> lk( mtx_rcv_wait_element_list_ );

	idx_mgr_element* p_poped_el = p_el_list->pop();
	while ( p_poped_el != nullptr ) {
		rcv_wait_element_list_.push( p_poped_el );
		p_poped_el = p_el_list->pop();
	}

	return;
}

////////////////////////////////////////////////////////////////////////////////

waiting_idx_list::waiting_idx_list( const int idx_buff_size_arg, const int ver_arg )
  : ver_( ver_arg )
  , idx_buff_size_( idx_buff_size_arg )
  , idx_top_idx_( 0 )
  , p_idx_buff_( nullptr )
{

	if ( idx_buff_size_arg >= 0 ) {
		p_idx_buff_ = new int[idx_buff_size_];
	}
	return;
}

waiting_idx_list::~waiting_idx_list()
{
#if 0
	// ハザードポインタの対応の為に一時的に滞留しているだけで、
	// このクラスのインスタンスが廃棄される時は、idx_mgrクラスの利用者側は、使用済みとして返却済みの為、リークを起こしているわけではない。
	// よって、ここで残留があったとしても、idx_mgrクラスの観点では、リークしていない。
	// この点を踏まえると、残留チェックは意味がないので、チェックを無効化する。
	if ( idx_top_idx_ > 0 ) {
		char buf[CONF_LOGGER_INTERNAL_BUFF_SIZE];

		snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1, "waiting_idx_list_%p is destructed. but it has some index {", this );
		internal::LogOutput( log_type::WARN, buf );
		for ( int i = 0; i < idx_top_idx_; i++ ) {
			snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1, "\t%d,", p_idx_buff_[i] );
			internal::LogOutput( log_type::WARN, buf );
		}
		internal::LogOutput( log_type::WARN, "}" );
	}
#endif

	delete[] p_idx_buff_;
}

void waiting_idx_list::chk_reset_and_set_size( const int idx_buff_size_arg, const int ver_arg )
{
	if ( ver_ == ver_arg ) return;

	delete[] p_idx_buff_;
	idx_buff_size_ = idx_buff_size_arg;
	p_idx_buff_    = new int[idx_buff_size_];
	idx_top_idx_   = 0;
	ver_           = ver_arg;
	return;
}

int waiting_idx_list::pop_from_tls( const int idx_buff_size_arg, const int ver_arg )
{
	chk_reset_and_set_size( idx_buff_size_arg, ver_arg );

	int ans = -1;

	if ( idx_top_idx_ <= 0 ) return ans;

	idx_top_idx_--;

	ans                       = p_idx_buff_[idx_top_idx_];
	p_idx_buff_[idx_top_idx_] = -1;

	return ans;
}

void waiting_idx_list::push_to_tls( const int valid_idx, const int idx_buff_size_arg, const int ver_arg )
{
	chk_reset_and_set_size( idx_buff_size_arg, ver_arg );

	if ( idx_top_idx_ >= idx_buff_size_ ) {
		throw std::exception();
		return;
	}

	p_idx_buff_[idx_top_idx_] = valid_idx;
	idx_top_idx_++;

	return;
}

void waiting_idx_list::dump( void ) const
{
	char buf[CONF_LOGGER_INTERNAL_BUFF_SIZE];

	snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
	          "object waiting_idx_list_%p as %p {\n"
	          "\t idx_buff_size_ = %d\n"
	          "\t idx_top_idx_ = %d\n"
	          "\t p_idx_buff_ = %p\n"
	          "}\n",
	          this, this,
	          idx_buff_size_,
	          idx_top_idx_,
	          p_idx_buff_ );
	internal::LogOutput( log_type::DUMP, buf );

	if ( p_idx_buff_ != nullptr ) {
		snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
		          "object p_idx_buff_%p as %p {\n",
		          this, this );
		internal::LogOutput( log_type::DUMP, buf );

		for ( int i = 0; i < idx_buff_size_; i++ ) {
			snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
			          "\t %d => %d\n",
			          i, p_idx_buff_[i] );
			internal::LogOutput( log_type::DUMP, buf );
		}

		internal::LogOutput( log_type::DUMP, "}\n" );
	}

	return;
}

////////////////////////////////////////////////////////////////////////////////
/*!
 * @breif	コンストラクタ
 */
idx_mgr::idx_mgr(
	const int idx_size_arg   //!< [in] 用意するインデックス番号の数。-1の場合、割り当てを保留する。
	)
  : idx_size_( idx_size_arg )
  , idx_size_ver_( 0 )
  , p_idx_mgr_element_array_( nullptr )
  , invalid_element_storage_( &idx_mgr_element::p_invalid_idx_next_element_ )
  , valid_element_storage_( &idx_mgr_element::p_valid_idx_next_element_ )
  , tls_waiting_idx_list_( rcv_idx_by_thread_terminating( this ) )
  , rcv_waiting_idx_list_( idx_size_, idx_size_ver_ )
{
	if ( idx_size_ < 0 ) return;

	tls_waiting_idx_list_.get_tls_instance( idx_size_, idx_size_ver_ );

	set_idx_size( idx_size_ );
	return;
}

idx_mgr::~idx_mgr()
{
	delete[] p_idx_mgr_element_array_;
}

void idx_mgr::set_idx_size( const int idx_size_arg )
{
	delete[] p_idx_mgr_element_array_;
	p_idx_mgr_element_array_ = nullptr;

	idx_size_ver_++;

	if ( idx_size_arg <= 0 ) {
		idx_size_ = -1;
		return;
	}

	// idx_mgr_elementのコンストラクタによる初期化を機能させるため、配列の再利用はしない。
	p_idx_mgr_element_array_ = new idx_mgr_element[idx_size_arg];
	idx_size_                = idx_size_arg;

	for ( int i = 0; i < idx_size_; i++ ) {
		p_idx_mgr_element_array_[i].idx_ = i;
		valid_element_storage_.push_element( &( p_idx_mgr_element_array_[i] ) );
	}

	return;
}

int idx_mgr::pop( void )
{
	int               ans       = -1;
	waiting_idx_list& wait_list = tls_waiting_idx_list_.get_tls_instance( idx_size_, idx_size_ver_ );

	ans = wait_list.pop_from_tls( idx_size_, idx_size_ver_ );
	if ( ans != -1 ) {
		return ans;
	}

	idx_mgr_element* p_valid_pop_element = valid_element_storage_.pop_element();

	if ( p_valid_pop_element == nullptr ) {
		std::unique_lock<std::mutex> lk( mtx_rcv_waiting_idx_list_, std::try_to_lock );
		if ( lk.owns_lock() ) {
			int tmp_ans = rcv_waiting_idx_list_.pop_from_tls( idx_size_, idx_size_ver_ );
			if ( tmp_ans >= 0 ) {
				return tmp_ans;
			}
		}
		return -1;
	}

	ans                       = p_valid_pop_element->idx_;
	p_valid_pop_element->idx_ = -1;

	invalid_element_storage_.push_element( p_valid_pop_element );

	return ans;
}

void idx_mgr::push(
	const int idx_arg   //!< [in] 返却するインデックス番号
)
{
	waiting_idx_list& wait_list = tls_waiting_idx_list_.get_tls_instance( idx_size_, idx_size_ver_ );

	idx_mgr_element* p_invalid_element = invalid_element_storage_.pop_element();

	if ( p_invalid_element == nullptr ) {
		// スロットに空きがないので、ローカルストレージで保持する。
		wait_list.push_to_tls( idx_arg, idx_size_, idx_size_ver_ );
	} else {

		p_invalid_element->idx_ = idx_arg;

		valid_element_storage_.push_element( p_invalid_element );
	}

	return;
}

void idx_mgr::rcv_wait_idx_by_thread_terminating( waiting_idx_list* p_idx_list )
{
	std::lock_guard<std::mutex> lock( mtx_rcv_waiting_idx_list_ );

	int tmp_idx = p_idx_list->pop_from_tls( idx_size_, idx_size_ver_ );

	while ( tmp_idx >= 0 ) {
		rcv_waiting_idx_list_.push_to_tls( tmp_idx, idx_size_, idx_size_ver_ );
		tmp_idx = p_idx_list->pop_from_tls( idx_size_, idx_size_ver_ );
	}

	return;
}

void idx_mgr::dump( void )
{
	char buf[CONF_LOGGER_INTERNAL_BUFF_SIZE];

	const waiting_idx_list& tmp_wel = tls_waiting_idx_list_.get_tls_instance( idx_size_, idx_size_ver_ );

	snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
	          "object idx_mgr_%p as %p {\n"
	          "\t idx_size_ = %d\n"
	          "\t idx_size_ver_ = %d\n"
	          "\t p_idx_mgr_element_array_ = %p\n"
	          "\t invalid_element_stack_head_ = %p\n"
	          "\t valid_element_stack_head_ = %p\n"
	          "\t waiting_element_list = %p\n"
	          "}\n",
	          this, this,
	          idx_size_,
	          idx_size_ver_,
	          p_idx_mgr_element_array_,
	          &invalid_element_storage_,
	          &valid_element_storage_,
	          &tmp_wel );
	internal::LogOutput( log_type::DUMP, buf );

	if ( p_idx_mgr_element_array_ != nullptr ) {
		snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
		          "%p --> %p \n",
		          this, p_idx_mgr_element_array_ );
		internal::LogOutput( log_type::DUMP, buf );
	}

	snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
	          "%p --> %p \n",
	          this, &invalid_element_storage_ );
	internal::LogOutput( log_type::DUMP, buf );

	snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
	          "%p --> %p \n",
	          this, &valid_element_storage_ );
	internal::LogOutput( log_type::DUMP, buf );

	snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
	          "%p --> %p \n",
	          this, &tmp_wel );
	internal::LogOutput( log_type::DUMP, buf );

	if ( p_idx_mgr_element_array_ != nullptr ) {
		for ( int i = 0; i < idx_size_; i++ ) {
			p_idx_mgr_element_array_[i].dump();
		}
	}

	tmp_wel.dump();

	return;
}

////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////

void slot_header::set_addr_of_chunk_header_multi_slot(
	chunk_header_multi_slot* p_chms_arg
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
	,
	const char* caller_src_fname,   //!< [in] caller side source file name
	const int   caller_lineno,      //!< [in] caller side line number
	const char* caller_func_name    //!< [in] function name calling this I/F
#endif
)
{
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
	set_caller_context_info( caller_src_fname, caller_lineno, caller_func_name );
#endif

	// 今は簡単な仕組みで実装する
	at_p_chms_.store( p_chms_arg, std::memory_order_release );
	at_mark_.store( 0 - reinterpret_cast<std::uintptr_t>( p_chms_arg ) - 1, std::memory_order_release );

	return;
}

#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
void slot_header::set_caller_context_info(
	const char* caller_src_fname,   //!< [in] caller side source file name
	const int   caller_lineno,      //!< [in] caller side line number
	const char* caller_func_name    //!< [in] function name calling this I/F
)
{
	p_caller_src_fname_ = caller_src_fname;
	caller_lineno_      = caller_lineno;
	p_caller_func_name_ = caller_func_name;
}
#endif

slot_chk_result slot_header::chk_header_data( void ) const
{
	// 今は簡単な仕組みで実装する
	chunk_header_multi_slot* p_this_chms = at_p_chms_.load( std::memory_order_acquire );
	std::uintptr_t           tmp         = reinterpret_cast<std::uintptr_t>( p_this_chms );
	tmp += at_mark_.load( std::memory_order_acquire ) + 1;

	slot_chk_result ans;
	ans.correct_ = ( tmp == 0 );
	ans.p_chms_  = p_this_chms;
	return ans;
}

////////////////////////////////////////////////////////////////////////////////
chunk_header_multi_slot::chunk_header_multi_slot(
	const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
	)
  : p_next_chunk_( nullptr )
  , status_( chunk_control_status::EMPTY )
  , num_of_accesser_( 0 )
  , slot_conf_ {}
  , p_free_slot_mark_( nullptr )
  , free_slot_idx_mgr_( -1 )
  , p_chunk_( nullptr )
  , statistics_alloc_req_cnt_( 0 )
  , statistics_alloc_req_err_cnt_( 0 )
  , statistics_dealloc_req_cnt_( 0 )
  , statistics_dealloc_req_err_cnt_( 0 )
  , statistics_consum_cnt_( 0 )
  , statistics_max_consum_cnt_( 0 )
{
	alloc_new_chunk( ch_param_arg );

	return;
}

chunk_header_multi_slot::~chunk_header_multi_slot()
{
	delete[] p_free_slot_mark_;
	std::free( p_chunk_ );

	return;
}

#define GM_ALIGN_SIZE ( alignof( std::max_align_t ) )

constexpr std::size_t get_slot_header_size( void )
{
	std::size_t tmp = sizeof( slot_header ) / GM_ALIGN_SIZE;

	std::size_t ans = ( tmp + 1 ) * GM_ALIGN_SIZE;

	return ans;
}

std::size_t chunk_header_multi_slot::get_size_of_one_slot(
	const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
)
{
	std::size_t tmp = ch_param_arg.size_of_one_piece_ / GM_ALIGN_SIZE;

	std::size_t ans = ( tmp + 1 ) * GM_ALIGN_SIZE;
	ans += get_slot_header_size();

	return ans;
}

void chunk_header_multi_slot::set_slot_allocation_conf(
	const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
)
{
	slot_conf_.size_of_one_piece_ = get_size_of_one_slot( ch_param_arg );
	slot_conf_.num_of_pieces_     = ( ch_param_arg.num_of_pieces_ >= 2 ) ? ch_param_arg.num_of_pieces_ : 2;

	return;
}

bool chunk_header_multi_slot::alloc_new_chunk(
	const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
)
{
	// get ownership to allocate a chunk
	internal::chunk_control_status expect = chunk_control_status::EMPTY;
	bool                           result = status_.compare_exchange_strong( expect, chunk_control_status::RESERVED_ALLOCATION );
	if ( !result ) return false;
	// resultが真となったので、以降、このchunkに対する、領域確保の処理実行権を保有。

	set_slot_allocation_conf( ch_param_arg );

	std::size_t tmp_size;
	tmp_size = (std::size_t)slot_conf_.size_of_one_piece_ * (std::size_t)slot_conf_.num_of_pieces_;

	if ( tmp_size == 0 ) {
		status_.store( chunk_control_status::EMPTY, std::memory_order_release );
		return false;
	}

	p_chunk_ = std::malloc( tmp_size );
	if ( p_chunk_ == nullptr ) {
		status_.store( chunk_control_status::EMPTY, std::memory_order_release );
		return false;
	}

	if ( p_free_slot_mark_ == nullptr ) {
		p_free_slot_mark_ = new std::atomic_bool[slot_conf_.num_of_pieces_];
		for ( unsigned int i = 0; i < slot_conf_.num_of_pieces_; i++ ) {
			p_free_slot_mark_[i].store( true );
		}
	}

	size_of_chunk_ = tmp_size;

	free_slot_idx_mgr_.set_idx_size( slot_conf_.num_of_pieces_ );

	status_.store( chunk_control_status::NORMAL, std::memory_order_release );

	statistics_consum_cnt_.store( 0, std::memory_order_release );

	return true;
}

void* chunk_header_multi_slot::allocate_mem_slot_impl(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
	const char* caller_src_fname,   //!< [in] caller side source file name
	const int   caller_lineno,      //!< [in] caller side line number
	const char* caller_func_name    //!< [in] function name calling this I/F
#else
	void
#endif
)
{
	if ( status_.load( std::memory_order_acquire ) != chunk_control_status::NORMAL ) return nullptr;

	scoped_inout_counter<std::atomic<int>> cnt_inout( num_of_accesser_ );

	if ( status_.load( std::memory_order_acquire ) != chunk_control_status::NORMAL ) return nullptr;

	statistics_alloc_req_cnt_++;

	int read_idx = free_slot_idx_mgr_.pop();
	if ( read_idx == -1 ) {
		statistics_alloc_req_err_cnt_++;
		return nullptr;
	}

	p_free_slot_mark_[read_idx].store( false );

	std::uintptr_t p_ans_addr = reinterpret_cast<std::uintptr_t>( p_chunk_ );
	p_ans_addr += read_idx * slot_conf_.size_of_one_piece_;

	// always overwrite header information to fix a corrupted header
	slot_header* p_sh = reinterpret_cast<slot_header*>( p_ans_addr );
	p_sh->set_addr_of_chunk_header_multi_slot(
		this
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
		,
		caller_src_fname, caller_lineno, caller_func_name
#endif
	);

	return reinterpret_cast<void*>( p_ans_addr + get_slot_header_size() );
}

bool chunk_header_multi_slot::recycle_mem_slot_impl(
	void* p_recycle_addr   //!< [in] pointer to the memory slot to recycle.
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
	,
	const char* caller_src_fname,   //!< [in] caller side source file name
	const int   caller_lineno,      //!< [in] caller side line number
	const char* caller_func_name    //!< [in] function name calling this I/F
#endif
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

	std::uintptr_t slot_header_addr_tmp = reinterpret_cast<std::uintptr_t>( p_recycle_addr );
	slot_header_addr_tmp -= get_slot_header_size();
	void* p_slot_addr = reinterpret_cast<void*>( slot_header_addr_tmp );

	// check correct address or not.
	if ( p_slot_addr < p_chunk_ ) return false;

	std::uintptr_t p_chking_addr = reinterpret_cast<std::uintptr_t>( p_slot_addr );
	p_chking_addr -= reinterpret_cast<std::uintptr_t>( p_chunk_ );

	std::uintptr_t idx = p_chking_addr / slot_conf_.size_of_one_piece_;
	std::uintptr_t adt = p_chking_addr % slot_conf_.size_of_one_piece_;

	if ( idx >= slot_conf_.num_of_pieces_ ) return false;
	if ( adt != 0 ) return false;

	slot_header* p_sh            = reinterpret_cast<slot_header*>( p_slot_addr );
	bool         expect_not_free = false;
	bool         result          = p_free_slot_mark_[idx].compare_exchange_strong( expect_not_free, true );

	if ( !result ) {
		// double free has occured.
		internal::LogOutput( log_type::ERR, "double free has occured." );
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
		internal::LogOutput(
			log_type::ERR,
			"previous free call is: %s, line no %d, function=%s",
			p_sh->p_caller_src_fname_, p_sh->caller_lineno_, p_sh->p_caller_func_name_ );
		internal::LogOutput(
			log_type::ERR,
			"current free call is: %s, line no %d, function=%s",
			caller_src_fname, caller_lineno, caller_func_name );
#endif
		//		fflush( NULL );
		statistics_dealloc_req_err_cnt_++;
		return true;
	}

	// OK. correct address
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
	p_sh->set_caller_context_info( caller_src_fname, caller_lineno, caller_func_name );
#endif

	free_slot_idx_mgr_.push( idx );
	statistics_dealloc_req_cnt_++;

	return true;
}

bool chunk_header_multi_slot::set_delete_reservation( void )
{
	internal::chunk_control_status expect = chunk_control_status::NORMAL;
	bool                           result = std::atomic_compare_exchange_strong( &status_, &expect, chunk_control_status::RESERVED_DELETION );
	return result;
}

bool chunk_header_multi_slot::unset_delete_reservation( void )
{
	internal::chunk_control_status expect = chunk_control_status::RESERVED_DELETION;
	bool                           result = std::atomic_compare_exchange_strong( &status_, &expect, chunk_control_status::NORMAL );
	return result;
}

bool chunk_header_multi_slot::exec_deletion( void )
{
	if ( num_of_accesser_.load() != 0 ) return false;

	internal::chunk_control_status expect = chunk_control_status::RESERVED_DELETION;
	bool                           result = std::atomic_compare_exchange_strong( &status_, &expect, chunk_control_status::DELETION );

	if ( !result ) return false;

	std::free( p_chunk_ );
	p_chunk_ = nullptr;

	delete[] p_free_slot_mark_;
	p_free_slot_mark_ = nullptr;

	status_.store( chunk_control_status::EMPTY, std::memory_order_release );

	return true;
}

slot_chk_result chunk_header_multi_slot::get_chunk(
	void* p_addr   //!< [in] void* address that allocate_mem_slot() returns
)
{
	std::uintptr_t slot_header_addr_tmp = reinterpret_cast<std::uintptr_t>( p_addr );
	slot_header_addr_tmp -= get_slot_header_size();
	slot_header* p_slot_addr = reinterpret_cast<slot_header*>( slot_header_addr_tmp );

	slot_chk_result ret = p_slot_addr->chk_header_data();
	if ( !( ret.correct_ ) ) {
		char buf[CONF_LOGGER_INTERNAL_BUFF_SIZE];

		snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
		          "a header of slot_header is corrupted %p\n",
		          p_addr );
		internal::LogOutput( log_type::ERR, buf );
	}

	return ret;
}

chunk_statistics chunk_header_multi_slot::get_statistics( void ) const
{
	chunk_statistics ans;

	ans.alloc_conf_     = slot_conf_;
	ans.chunk_num_      = 1;
	ans.total_slot_cnt_ = slot_conf_.num_of_pieces_;
	ans.free_slot_cnt_  = 0;
	for ( std::size_t i = 0; i < slot_conf_.num_of_pieces_; i++ ) {
		if ( p_free_slot_mark_[i].load( std::memory_order_acquire ) ) ans.free_slot_cnt_++;
	}
	ans.max_consum_cnt_        = statistics_max_consum_cnt_.load( std::memory_order_acquire );
	ans.alloc_req_cnt_         = statistics_alloc_req_cnt_.load( std::memory_order_acquire );
	ans.error_alloc_req_cnt_   = statistics_alloc_req_err_cnt_.load( std::memory_order_acquire );
	ans.dealloc_req_cnt_       = statistics_dealloc_req_cnt_.load( std::memory_order_acquire );
	ans.error_dealloc_req_cnt_ = statistics_dealloc_req_err_cnt_.load( std::memory_order_acquire );

	ans.alloc_collision_cnt_   = free_slot_idx_mgr_.get_collision_cnt_valid_storage();
	ans.dealloc_collision_cnt_ = free_slot_idx_mgr_.get_collision_cnt_invalid_storage();

	return ans;
}

/*!
 * @breif	dump for debug
 */
void chunk_header_multi_slot::dump( void )
{
	char buf[CONF_LOGGER_INTERNAL_BUFF_SIZE];

	if ( p_chunk_ != nullptr ) {
		snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
		          "object chunk_%p as %p \n",
		          p_chunk_, p_chunk_ );
		internal::LogOutput( log_type::DUMP, buf );
	}

	if ( p_free_slot_mark_ != nullptr ) {
		snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
		          "object p_free_slot_mark_%p as %p {\n",
		          p_free_slot_mark_, p_free_slot_mark_ );
		internal::LogOutput( log_type::DUMP, buf );

		for ( std::size_t i = 0; i < slot_conf_.num_of_pieces_; i++ ) {
			snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
			          "%zu = %s \n",
			          i, p_free_slot_mark_[i].load() ? "true" : "false" );
			internal::LogOutput( log_type::DUMP, buf );
		}
		internal::LogOutput( log_type::DUMP, "}\n" );
	}

	snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
	          "object chunk_header_multi_slot_%p as %p {\n"
	          "\t alloc_conf_.size_of_one_piece_ = %u \n"
	          "\t alloc_conf_.num_of_pieces_ = %u \n"
	          "\t size_of_chunk_ = %zu \n"
	          "\t p_free_slot_mark_ = %p \n"
	          "\t p_chunk_ = %p \n"
	          "\t free_slot_idx_mgr_ = %p \n"
	          "}\n",
	          this, this,
	          slot_conf_.size_of_one_piece_,
	          slot_conf_.num_of_pieces_,
	          size_of_chunk_,
	          p_free_slot_mark_,
	          p_chunk_,
	          &free_slot_idx_mgr_ );
	internal::LogOutput( log_type::DUMP, buf );

	free_slot_idx_mgr_.dump();

	return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
chunk_list::chunk_list(
	const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
	)
  : size_of_one_piece_( ch_param_arg.size_of_one_piece_ )
  , num_of_pieces_( ch_param_arg.num_of_pieces_ )
  , p_top_chunk_( nullptr )
  , tls_p_hint_chunk()
{
	chunk_header_multi_slot* p_new_chms = new chunk_header_multi_slot( ch_param_arg );

	p_top_chunk_.store( p_new_chms, std::memory_order_release );
	tls_p_hint_chunk.get_tls_instance( p_new_chms ) = p_new_chms;

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

void* chunk_list::allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
	const char* caller_src_fname,   //!< [in] caller side source file name
	const int   caller_lineno,      //!< [in] caller side line number
	const char* caller_func_name    //!< [in] function name calling this I/F
#else
	void
#endif
)
{
	void* p_ans = nullptr;

	// hintに登録されたchunkから空きスロット検索を開始する。
	chunk_header_multi_slot* p_start_chms = tls_p_hint_chunk.get_tls_instance( p_top_chunk_.load( std::memory_order_acquire ) );

	chunk_header_multi_slot* p_cur_chms         = p_start_chms;
	chunk_header_multi_slot* p_1st_rsv_del_chms = nullptr;
	chunk_header_multi_slot* p_1st_empty_chms   = nullptr;
	while ( p_cur_chms != nullptr ) {
		// 空きスロットが見つかれば終了
		p_ans = p_cur_chms->allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
			caller_src_fname, caller_lineno, caller_func_name
#endif
		);
		if ( p_ans != nullptr ) {
			tls_p_hint_chunk.get_tls_instance( p_cur_chms ) = p_cur_chms;
			return p_ans;
		}
		// 削除予約されたchunkがあれば、記録する。
		if ( p_cur_chms->status_.load( std::memory_order_acquire ) == chunk_control_status::RESERVED_DELETION ) {
			if ( p_1st_rsv_del_chms == nullptr ) {
				p_1st_rsv_del_chms = p_cur_chms;
			}
		}
		// メモリスロットが割り当てられていないchunkがあれば、記録する。
		if ( p_cur_chms->status_.load( std::memory_order_acquire ) == chunk_control_status::EMPTY ) {
			if ( p_1st_empty_chms == nullptr ) {
				p_1st_empty_chms = p_cur_chms;
			}
		}

		// リンクリストをたどって、次に検索するchunkを取り出す。
		// 検索を開始したchunkに到着したら、空きスロットが見つからなかったことを示すので、検索を終了する。
		chunk_header_multi_slot* p_next_chms = p_cur_chms->p_next_chunk_.load( std::memory_order_acquire );
		if ( p_next_chms == nullptr ) {
			p_next_chms = p_top_chunk_.load( std::memory_order_acquire );
		}
		if ( p_next_chms == p_start_chms ) {
			p_cur_chms = nullptr;
			break;
		}
		p_cur_chms = p_next_chms;
	}

	// 空きスロットが見つからなかったため、削除予約chunkの再利用を試みる。
	// ただし、すでに削除されてしまっているかもしれない。
	if ( p_1st_rsv_del_chms != nullptr ) {
		if ( p_1st_rsv_del_chms->unset_delete_reservation() ) {
			// 再利用成功
			p_ans = p_1st_rsv_del_chms->allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
				caller_src_fname, caller_lineno, caller_func_name
#endif
			);
			if ( p_ans != nullptr ) {
				tls_p_hint_chunk.get_tls_instance( p_1st_rsv_del_chms ) = p_1st_rsv_del_chms;
				return p_ans;
			}
		} else {
			if ( p_1st_rsv_del_chms->status_.load( std::memory_order_acquire ) == chunk_control_status::NORMAL ) {
				// すでに、再利用可能な状態になっていた
				p_ans = p_1st_rsv_del_chms->allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
					caller_src_fname, caller_lineno, caller_func_name
#endif
				);
				if ( p_ans != nullptr ) {
					tls_p_hint_chunk.get_tls_instance( p_1st_rsv_del_chms ) = p_1st_rsv_del_chms;
					return p_ans;
				}
			}
		}
	}

	// 確保するスロットサイズを増やす。
	// ここで、即座にメンバ変数の値を２倍にすると、メモリの追加確保が並行して実行された場合に、並行処理分の累乗が行われて、即座にオーバーフローしてしまう。
	// よって、メンバ変数の値の2倍化は、chunkのリスト登録が完了してから行う。
	unsigned int cur_slot_num = num_of_pieces_.load( std::memory_order_acquire );
	unsigned int new_slot_num = cur_slot_num * 2;
	if ( cur_slot_num > new_slot_num ) {
		// オーバーフローしてしまったら、2倍化を取りやめる。
		new_slot_num = cur_slot_num;
	}
	param_chunk_allocation cur_alloc_conf { size_of_one_piece_, new_slot_num };

	// 空きスロットがなく、削除予約chunkの再利用にも失敗したため、
	// 空chunkにメモリ割り当てを行い、再利用を試みる。
	if ( p_1st_empty_chms != nullptr ) {
		if ( p_1st_empty_chms->alloc_new_chunk( cur_alloc_conf ) ) {
			// 再割り当て成功
			p_ans = p_1st_empty_chms->allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
				caller_src_fname, caller_lineno, caller_func_name
#endif
			);
			if ( p_ans != nullptr ) {
				tls_p_hint_chunk.get_tls_instance( p_1st_empty_chms ) = p_1st_empty_chms;

				// chunkの登録が終わったので、スロット数の2倍化された値を登録する。
				num_of_pieces_.compare_exchange_strong( cur_slot_num, new_slot_num );

				return p_ans;
			}
		} else {
			// 再割り当て失敗
			if ( p_1st_empty_chms->status_.load( std::memory_order_acquire ) == chunk_control_status::NORMAL ) {
				//　自身による再割り当てに失敗はしたが、既に別のスレッドで再割り当て完了済みだった
				p_ans = p_1st_empty_chms->allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
					caller_src_fname, caller_lineno, caller_func_name
#endif
				);
				if ( p_ans != nullptr ) {
					tls_p_hint_chunk.get_tls_instance( p_1st_empty_chms ) = p_1st_empty_chms;
					return p_ans;
				}
			}
		}
	}

	// 既存のchunkの再利用に失敗したので、新しいchunkを確保する。
	// new演算子を使用するため、ここでロックが発生する可能性がある。
	chunk_header_multi_slot* p_new_chms = new chunk_header_multi_slot( cur_alloc_conf );
	p_ans                               = p_new_chms->allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
		caller_src_fname, caller_lineno, caller_func_name
#endif
	);
	if ( p_ans == nullptr ) {
		delete p_new_chms;
		return nullptr;   // TODO: should throw exception ?
	}

	// 新たに確保したchunkを、chunkリストの先頭に追加する。
	chunk_header_multi_slot* p_cur_top = p_top_chunk_.load( std::memory_order_acquire );
	do {
		p_new_chms->p_next_chunk_.store( p_cur_top, std::memory_order_release );
	} while ( !p_top_chunk_.compare_exchange_weak( p_cur_top, p_new_chms ) );

	tls_p_hint_chunk.get_tls_instance( p_new_chms ) = p_new_chms;

	// chunkの登録が終わったので、スロット数の2倍化された値を登録する。
	num_of_pieces_.compare_exchange_strong( cur_slot_num, new_slot_num );

	return p_ans;
}

bool chunk_list::recycle_mem_slot(
	void* p_recycle_slot   //!< [in] pointer to the memory slot to recycle.
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
	,
	const char* caller_src_fname,   //!< [in] caller side source file name
	const int   caller_lineno,      //!< [in] caller side line number
	const char* caller_func_name    //!< [in] function name calling this I/F
#endif
)
{
	chunk_header_multi_slot* p_cur_chms = p_top_chunk_.load( std::memory_order_acquire );
	while ( p_cur_chms != nullptr ) {
		bool ret = p_cur_chms->recycle_mem_slot(
			p_recycle_slot
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
			,
			caller_src_fname,
			caller_lineno,
			caller_func_name
#endif
		);
		if ( ret ) return true;
		chunk_header_multi_slot* p_next_chms = p_cur_chms->p_next_chunk_.load( std::memory_order_acquire );
		p_cur_chms                           = p_next_chms;
	}

	return false;
}

/*!
 * @breif	get statistics
 */
chunk_statistics chunk_list::get_statistics( void ) const
{
	chunk_statistics ans;

	ans.alloc_conf_            = param_chunk_allocation { size_of_one_piece_, num_of_pieces_.load( std::memory_order_acquire ) };
	ans.chunk_num_             = 0;
	ans.total_slot_cnt_        = 0;
	ans.free_slot_cnt_         = 0;
	ans.max_consum_cnt_        = 0;
	ans.alloc_req_cnt_         = 0;
	ans.error_alloc_req_cnt_   = 0;
	ans.dealloc_req_cnt_       = 0;
	ans.error_dealloc_req_cnt_ = 0;
	ans.alloc_collision_cnt_   = 0;
	ans.dealloc_collision_cnt_ = 0;

	chunk_header_multi_slot* p_cur_chms = p_top_chunk_.load( std::memory_order_acquire );
	while ( p_cur_chms != nullptr ) {
		chunk_statistics one_chms_statistics = p_cur_chms->get_statistics();
		ans.alloc_conf_                      = param_chunk_allocation { size_of_one_piece_, num_of_pieces_.load( std::memory_order_acquire ) };
		ans.chunk_num_ += one_chms_statistics.chunk_num_;
		ans.total_slot_cnt_ += one_chms_statistics.total_slot_cnt_;
		ans.free_slot_cnt_ += one_chms_statistics.free_slot_cnt_;
		ans.max_consum_cnt_ += one_chms_statistics.max_consum_cnt_;
		ans.alloc_req_cnt_ += one_chms_statistics.alloc_req_cnt_;
		ans.error_alloc_req_cnt_ += one_chms_statistics.error_alloc_req_cnt_;
		ans.dealloc_req_cnt_ += one_chms_statistics.dealloc_req_cnt_;
		ans.error_dealloc_req_cnt_ += one_chms_statistics.error_dealloc_req_cnt_;
		ans.alloc_collision_cnt_ += one_chms_statistics.alloc_collision_cnt_;
		ans.dealloc_collision_cnt_ += one_chms_statistics.dealloc_collision_cnt_;

		chunk_header_multi_slot* p_nxt_chms = p_cur_chms->p_next_chunk_.load( std::memory_order_acquire );
		p_cur_chms                          = p_nxt_chms;
	}

	return ans;
}

}   // namespace internal

////////////////////////////////////////////////////////////////////////////////////////////////////////
general_mem_allocator::general_mem_allocator( void )
  : pr_ch_size_( 0 )
  , up_param_ch_array_()
{
	return;
}

general_mem_allocator::general_mem_allocator(
	const param_chunk_allocation* p_param_array,   //!< [in] pointer to parameter array
	unsigned int                  num              //!< [in] array size
	)
  : pr_ch_size_( 0 )
  , up_param_ch_array_()
{
	set_param( p_param_array, num );
	return;
}

void* general_mem_allocator::allocate(
	std::size_t n   //!< [in] required memory size
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
	,
	const char* caller_src_fname,   //!< [in] caller side source file name
	const int   caller_lineno,      //!< [in] caller side line number
	const char* caller_func_name    //!< [in] function name calling this I/F
#endif
)
{
	void* p_ans = nullptr;
	for ( unsigned int i = 0; i < pr_ch_size_; i++ ) {
		// liner search
		if ( up_param_ch_array_[i].param_.size_of_one_piece_ >= n ) {
			p_ans = up_param_ch_array_[i].up_chunk_lst_->allocate_mem_slot(
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
				caller_src_fname, caller_lineno, caller_func_name
#endif
			);
			if ( p_ans != nullptr ) {
				return p_ans;
			}
		}
	}

	internal::slot_header* p_sh = reinterpret_cast<internal::slot_header*>( std::malloc( n + internal::get_slot_header_size() ) );

	p_sh->set_addr_of_chunk_header_multi_slot(
		nullptr
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
		,
		caller_src_fname, caller_lineno, caller_func_name
#endif
	);

	std::uintptr_t tmp_ans = reinterpret_cast<std::uintptr_t>( p_sh );
	tmp_ans += internal::get_slot_header_size();
	p_ans = reinterpret_cast<void*>( tmp_ans );

	return p_ans;
}

void general_mem_allocator::deallocate(
	void* p_mem   //!< [in] pointer to allocated memory by allocate()
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
	,
	const char* caller_src_fname,   //!< [in] caller side source file name
	const int   caller_lineno,      //!< [in] caller side line number
	const char* caller_func_name    //!< [in] function name calling this I/F
#endif
)
{
	internal::slot_chk_result chk_ret = internal::chunk_header_multi_slot::get_chunk( p_mem );

	if ( chk_ret.correct_ ) {
		if ( chk_ret.p_chms_ != nullptr ) {
			chk_ret.p_chms_->recycle_mem_slot(
				p_mem
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
				,
				caller_src_fname, caller_lineno, caller_func_name
#endif
			);
		} else {
			std::uintptr_t tmp_addr = reinterpret_cast<std::uintptr_t>( p_mem );
			tmp_addr -= internal::get_slot_header_size();
			std::free( reinterpret_cast<void*>( tmp_addr ) );
		}
	} else {

		for ( unsigned int i = 0; i < pr_ch_size_; i++ ) {
			bool ret = up_param_ch_array_[i].up_chunk_lst_->recycle_mem_slot(
				p_mem
#ifndef LF_MEM_ALLOC_NO_CALLER_CONTEXT_INFO
				,
				caller_src_fname, caller_lineno, caller_func_name
#endif
			);
			if ( ret ) return;
		}

		// header is corrupted and unknown memory slot deallocation is requested. try to call free()
		std::free( p_mem );   // TODO should not do this ?
	}

	return;
}

void general_mem_allocator::set_param(
	const param_chunk_allocation* p_param_array,   //!< [in] pointer to parameter array
	unsigned int                  num              //!< [in] array size
)
{
	if ( pr_ch_size_ > 0 ) {
		internal::LogOutput( log_type::WARN, "paramter has already set. ignore this request." );
		return;
	}

	pr_ch_size_ = num;

	std::vector<int> idx_vec( pr_ch_size_ );

	for ( unsigned int i = 0; i < pr_ch_size_; i++ ) {
		idx_vec[i] = i;
	}

	std::sort( idx_vec.begin(), idx_vec.end(),
	           [p_param_array]( int a, int b ) {
				   return p_param_array[a].size_of_one_piece_ < p_param_array[b].size_of_one_piece_;
			   } );

	up_param_ch_array_ = std::unique_ptr<param_chunk_comb[]>( new param_chunk_comb[pr_ch_size_] );
	for ( unsigned int i = 0; i < pr_ch_size_; i++ ) {
		up_param_ch_array_[i].param_        = p_param_array[idx_vec[i]];
		up_param_ch_array_[i].up_chunk_lst_ = std::unique_ptr<internal::chunk_list>( new internal::chunk_list( up_param_ch_array_[i].param_ ) );
	}

	return;
}

/*!
 * @breif	get statistics
 */
std::list<chunk_statistics> general_mem_allocator::get_statistics( void ) const
{
	std::list<chunk_statistics> ans;

	for ( unsigned int i = 0; i < pr_ch_size_; i++ ) {
		chunk_statistics tmp_st = up_param_ch_array_[i].up_chunk_lst_->get_statistics();
		ans.push_back( tmp_st );
	}

	return ans;
}

std::string chunk_statistics::print( void )
{
	char buf[CONF_LOGGER_INTERNAL_BUFF_SIZE];
	snprintf( buf, CONF_LOGGER_INTERNAL_BUFF_SIZE - 1,
	          "chunk conf.size=%d, .num=%d, chunk_num: %d, total_slot=%d, free_slot=%d, max consum cnt=%d, alloc cnt=%d, alloc err=%d, dealloc cnt=%d, dealloc err=%d, alloc_collision=%d, dealloc_collision=%d",
	          (int)alloc_conf_.size_of_one_piece_,
	          (int)alloc_conf_.num_of_pieces_,
	          (int)chunk_num_,
	          (int)total_slot_cnt_,
	          (int)free_slot_cnt_,
	          (int)max_consum_cnt_,
	          (int)alloc_req_cnt_,
	          (int)error_alloc_req_cnt_,
	          (int)dealloc_req_cnt_,
	          (int)error_dealloc_req_cnt_,
	          (int)alloc_collision_cnt_,
	          (int)dealloc_collision_cnt_ );

	return std::string( buf );
}

}   // namespace concurrent
}   // namespace alpha
