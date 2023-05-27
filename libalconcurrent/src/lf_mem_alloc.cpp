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
#include <cassert>
#include <cstddef>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/lf_mem_alloc.hpp"
#include "lf_mem_alloc_internal.hpp"

namespace alpha {
namespace concurrent {

bool test_platform_std_atomic_lockfree_condition( void )
{
	bool ans;

	ans = ( ATOMIC_BOOL_LOCK_FREE == 2 );
	ans = ans && std::atomic<internal::chunk_control_status> { internal::chunk_control_status::EMPTY }.is_lock_free();
	ans = ans && std::atomic<internal::chunk_header_multi_slot*> { nullptr }.is_lock_free();
	ans = ans && ( ATOMIC_INT_LOCK_FREE == 2 );

	// using fp_t = void ( * )( void* );
	ans = ans && std::atomic<void ( * )( void* )>().is_lock_free();

	return ans;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
namespace internal {

chunk_statistics chunk_list_statistics::get_statistics( void ) const
{
	chunk_statistics ans { 0 };

	ans.chunk_num_       = chunk_num_.load( std::memory_order_acquire );
	ans.valid_chunk_num_ = valid_chunk_num_.load( std::memory_order_acquire );
	ans.total_slot_cnt_  = total_slot_cnt_.load( std::memory_order_acquire );
	ans.free_slot_cnt_   = free_slot_cnt_.load( std::memory_order_acquire );
	ans.consum_cnt_      = consum_cnt_.load( std::memory_order_acquire );
	ans.max_consum_cnt_  = max_consum_cnt_.load( std::memory_order_acquire );
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	ans.alloc_req_cnt_         = alloc_req_cnt_.load( std::memory_order_acquire );
	ans.error_alloc_req_cnt_   = alloc_req_err_cnt_.load( std::memory_order_acquire );
	ans.dealloc_req_cnt_       = dealloc_req_cnt_.load( std::memory_order_acquire );
	ans.error_dealloc_req_cnt_ = dealloc_req_err_cnt_.load( std::memory_order_acquire );
	ans.alloc_collision_cnt_   = alloc_collision_cnt_.load( std::memory_order_acquire );
	ans.dealloc_collision_cnt_ = dealloc_collision_cnt_.load( std::memory_order_acquire );
#endif

	return ans;
}

idx_mgr_element::idx_mgr_element( void )
  : idx_( -1 )
  , p_invalid_idx_next_element_( nullptr )
  , p_valid_idx_next_element_( nullptr )
  , p_waiting_next_element_( nullptr )
{
	return;
}

/*!
 * @brief	dump for debug
 */
void idx_mgr_element::dump( void ) const
{
	internal::LogOutput( log_type::DUMP,
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

	if ( p_invalid_idx_next_element_.load() != nullptr ) {
		internal::LogOutput( log_type::DUMP,
		                     "%p --> %p : invalid\n",
		                     this, p_invalid_idx_next_element_.load() );
	}

	if ( p_valid_idx_next_element_.load() != nullptr ) {
		internal::LogOutput( log_type::DUMP,
		                     "%p --> %p : valid\n",
		                     this, p_valid_idx_next_element_.load() );
	}

	if ( p_waiting_next_element_ != nullptr ) {
		internal::LogOutput( log_type::DUMP,
		                     "%p --> %p : waiting\n",
		                     this, p_waiting_next_element_ );
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
	head_ = nullptr;
	tail_ = nullptr;
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
	internal::LogOutput( log_type::DUMP,
	                     "object waiting_element_list_%p as %p {\n"
	                     "\t head_ = %p\n"
	                     "\t tail_ = %p\n"
	                     "}\n",
	                     this, this,
	                     head_,
	                     tail_ );

	if ( head_ != nullptr ) {
		internal::LogOutput( log_type::DUMP,
		                     "%p --> %p\n",
		                     this, head_ );
	}

	if ( tail_ != nullptr ) {
		internal::LogOutput( log_type::DUMP,
		                     "%p --> %p\n",
		                     this, tail_ );
	}

	return;
}

////////////////////////////////////////////////////////////////////////////////
/*!
 * @brief	インデックス管理スロットのロックフリーストレージクラス
 */
idx_element_storage_mgr::idx_element_storage_mgr(
	std::atomic<idx_mgr_element*> idx_mgr_element::*p_next_ptr_offset_arg,   //!< [in] nextポインタを保持しているメンバ変数へのメンバ変数ポインタ
	std::atomic<unsigned int>*                      p_collition_counter      //!< [in] 衝突が発生した回数を記録するアトミック変数へのポインタ
	)
  : tls_waiting_list_( rcv_el_by_thread_terminating( this ) )
  , head_( nullptr )
  , tail_( nullptr )
  , p_next_ptr_offset_( p_next_ptr_offset_arg )
  , rcv_wait_element_list_()
  , p_collision_cnt_( p_collition_counter )
{
	return;
}

/*!
 * @brief	ストレージから１つの要素を取り出す
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
 * @brief	ストレージへ１つの要素を戻す
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

void idx_element_storage_mgr::clear( void )
{
	while ( pop_element() != nullptr ) {}
	return;
}

void idx_element_storage_mgr::dump( void ) const
{
	rcv_wait_element_list_.dump();
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
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
			( *p_collision_cnt_ )++;
#endif
			continue;
		}

		if ( p_cur_first == nullptr ) {
			// stackは空。
			return nullptr;
		}

		idx_mgr_element* p_cur_next = ( p_cur_first->*p_next_ptr_offset_ ).load();
		scoped_ref_next.regist_ptr_as_hazard_ptr( p_cur_next );
		if ( p_cur_next != ( p_cur_first->*p_next_ptr_offset_ ).load() ) {
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
			( *p_collision_cnt_ )++;
#endif
			continue;
		}

		// ここで、プリエンプションして、head_がA->B->A'となった時、p_cur_nextが期待値とは異なるが、
		// ハザードポインタにA相当を確保しているので、A'は現れない。よって、このようなABA問題は起きない。
		if ( head_.compare_exchange_weak( p_cur_first, p_cur_next ) ) {
			// ここで、firstの取り出しと所有権確保が完了
			// ただし、ハザードポインタをチェックしていないため、まだ参照している人がいるかもしれない。
			return p_cur_first;
		}
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
		( *p_collision_cnt_ )++;
#endif
	}
	return nullptr;
}

/*!
 * @brief	ストレージへ１つの要素を戻す
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
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
			( *p_collision_cnt_ )++;
#endif
			continue;
		}

		( p_push_element->*p_next_ptr_offset_ ).store( p_cur_top );

		// head_を更新する。
		// ここで、プリエンプションして、tail_がA->B->A'となった時、p_cur_lastが期待値とは異なるが、
		// ハザードポインタにA相当を確保しているので、A'は現れない。よって、このようなABA問題は起きない。
		if ( head_.compare_exchange_weak( p_cur_top, p_push_element ) ) {
			return;   // 追加完了
		}
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
		( *p_collision_cnt_ )++;
#endif
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

		internal::LogOutput( log_type::WARN, "waiting_idx_list_%p is destructed. but it has some index {", this );
		for ( int i = 0; i < idx_top_idx_; i++ ) {
			internal::LogOutput( log_type::WARN, "\t%d,", p_idx_buff_[i] );
		}
		internal::LogOutput( log_type::WARN, "}" );
	}
#endif

	delete[] p_idx_buff_;
	p_idx_buff_ = nullptr;
}

void waiting_idx_list::chk_reset_and_set_size( const int idx_buff_size_arg, const int ver_arg )
{
	if ( ver_ == ver_arg ) return;

	delete[] p_idx_buff_;
	p_idx_buff_    = nullptr;
	idx_buff_size_ = idx_buff_size_arg;
	idx_top_idx_   = 0;
	ver_           = ver_arg;

	if ( idx_buff_size_ > 0 ) {
		p_idx_buff_ = new int[idx_buff_size_];
	}

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
	internal::LogOutput( log_type::DUMP,
	                     "object waiting_idx_list_%p as %p {\n"
	                     "\t idx_buff_size_ = %d\n"
	                     "\t idx_top_idx_ = %d\n"
	                     "\t p_idx_buff_ = %p\n"
	                     "}\n",
	                     this, this,
	                     idx_buff_size_,
	                     idx_top_idx_,
	                     p_idx_buff_ );

	if ( p_idx_buff_ != nullptr ) {
		internal::LogOutput( log_type::DUMP,
		                     "object p_idx_buff_%p as %p {\n",
		                     this, this );

		for ( int i = 0; i < idx_buff_size_; i++ ) {
			internal::LogOutput( log_type::DUMP,
			                     "\t %d => %d\n",
			                     i, p_idx_buff_[i] );
		}

		internal::LogOutput( log_type::DUMP, "}\n" );
	}

	return;
}

////////////////////////////////////////////////////////////////////////////////
/*!
 * @brief	コンストラクタ
 */
idx_mgr::idx_mgr(
	const int                  idx_size_arg,                 //!< [in] 用意するインデックス番号の数。-1の場合、割り当てを保留する。
	std::atomic<unsigned int>* p_alloc_collision_cnt_arg,    //!< [in] 衝突回数をカウントする変数へのポインタ。ポインタ先のインスタンスは、このインスタンス以上のライフタイムを持つこと
	std::atomic<unsigned int>* p_dealloc_collision_cnt_arg   //!< [in] 衝突回数をカウントする変数へのポインタ。ポインタ先のインスタンスは、このインスタンス以上のライフタイムを持つこと
	)
  : idx_size_( idx_size_arg )
  , idx_size_ver_( 0 )
  , p_alloc_collision_cnt_( p_alloc_collision_cnt_arg )
  , p_dealloc_collision_cnt_( p_dealloc_collision_cnt_arg )
  , p_idx_mgr_element_array_( nullptr )
  , invalid_element_storage_( &idx_mgr_element::p_invalid_idx_next_element_, p_alloc_collision_cnt_ )
  , valid_element_storage_( &idx_mgr_element::p_valid_idx_next_element_, p_dealloc_collision_cnt_ )
  , tls_waiting_idx_list_( rcv_idx_by_thread_terminating( this ) )
  , rcv_waiting_idx_list_( idx_size_.load(), idx_size_ver_.load() )
{
	if ( idx_size_.load() <= 0 ) return;

	set_idx_size( idx_size_.load() );
	return;
}

idx_mgr::~idx_mgr()
{
	delete[] p_idx_mgr_element_array_;
	p_idx_mgr_element_array_ = nullptr;
}

void idx_mgr::set_idx_size( const int idx_size_arg )
{
	if ( idx_size_arg <= 0 ) {
		idx_size_.store( -1 );
		return;
	}

	idx_size_ver_++;

	// idx_mgr_elementのコンストラクタによる初期化を機能させるため、配列の再利用はしない。
	p_idx_mgr_element_array_ = new idx_mgr_element[idx_size_arg];
	idx_size_.store( idx_size_arg );

	tls_waiting_idx_list_.get_tls_instance( idx_size_.load(), idx_size_ver_.load() );

	for ( int i = 0; i < idx_size_.load(); i++ ) {
		p_idx_mgr_element_array_[i].idx_ = i;
		valid_element_storage_.push_element( &( p_idx_mgr_element_array_[i] ) );
	}

	return;
}

void idx_mgr::clear( void )
{
	std::lock_guard<std::recursive_mutex> lg( dynamic_tls_global_exclusive_control_for_destructions );

	invalid_element_storage_.~idx_element_storage_mgr();   // 強引に動的スレッドローカルストレージを含めて破棄する。
	valid_element_storage_.~idx_element_storage_mgr();     // 強引に動的スレッドローカルストレージを含めて破棄する。
	tls_waiting_idx_list_.~dynamic_tls();                  // 強引に動的スレッドローカルストレージを含めて破棄する。

	delete[] p_idx_mgr_element_array_;
	p_idx_mgr_element_array_ = nullptr;

	// 強引に初期化
	new ( &invalid_element_storage_ ) idx_element_storage_mgr( &idx_mgr_element::p_invalid_idx_next_element_, p_alloc_collision_cnt_ );
	new ( &valid_element_storage_ ) idx_element_storage_mgr( &idx_mgr_element::p_valid_idx_next_element_, p_dealloc_collision_cnt_ );
	new ( &tls_waiting_idx_list_ ) dynamic_tls<waiting_idx_list, rcv_idx_by_thread_terminating>( rcv_idx_by_thread_terminating( this ) );
}

int idx_mgr::pop( void )
{
	int               ans       = -1;
	waiting_idx_list& wait_list = tls_waiting_idx_list_.get_tls_instance( idx_size_.load(), idx_size_ver_.load() );

	ans = wait_list.pop_from_tls( idx_size_.load(), idx_size_ver_.load() );
	if ( ans != -1 ) {
		return ans;
	}

	idx_mgr_element* p_valid_pop_element = valid_element_storage_.pop_element();

	if ( p_valid_pop_element == nullptr ) {
		std::unique_lock<std::mutex> lk( mtx_rcv_waiting_idx_list_, std::try_to_lock );
		if ( lk.owns_lock() ) {
			int tmp_ans = rcv_waiting_idx_list_.pop_from_tls( idx_size_.load(), idx_size_ver_.load() );
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
	waiting_idx_list& wait_list = tls_waiting_idx_list_.get_tls_instance( idx_size_.load(), idx_size_ver_.load() );

	idx_mgr_element* p_invalid_element = invalid_element_storage_.pop_element();

	if ( p_invalid_element == nullptr ) {
		// スロットに空きがないので、ローカルストレージで保持する。
		wait_list.push_to_tls( idx_arg, idx_size_.load(), idx_size_ver_.load() );
	} else {

		p_invalid_element->idx_ = idx_arg;

		valid_element_storage_.push_element( p_invalid_element );
	}

	return;
}

void idx_mgr::rcv_wait_idx_by_thread_terminating( waiting_idx_list* p_idx_list )
{
	std::lock_guard<std::mutex> lock( mtx_rcv_waiting_idx_list_ );

	int tmp_idx = p_idx_list->pop_from_tls( idx_size_.load(), idx_size_ver_.load() );

	while ( tmp_idx >= 0 ) {
		rcv_waiting_idx_list_.push_to_tls( tmp_idx, idx_size_.load(), idx_size_ver_.load() );
		tmp_idx = p_idx_list->pop_from_tls( idx_size_.load(), idx_size_ver_.load() );
	}

	return;
}

void idx_mgr::dump( void )
{
	const waiting_idx_list& tmp_wel = tls_waiting_idx_list_.get_tls_instance( idx_size_.load(), idx_size_ver_.load() );

	internal::LogOutput( log_type::DUMP,
	                     "object idx_mgr_%p as %p {\n"
	                     "\t idx_size_ = %d\n"
	                     "\t idx_size_ver_ = %d\n"
	                     "\t p_idx_mgr_element_array_ = %p\n"
	                     "\t invalid_element_stack_head_ = %p\n"
	                     "\t valid_element_stack_head_ = %p\n"
	                     "\t waiting_element_list = %p\n"
	                     "}\n",
	                     this, this,
	                     idx_size_.load(),
	                     idx_size_ver_.load(),
	                     p_idx_mgr_element_array_,
	                     &invalid_element_storage_,
	                     &valid_element_storage_,
	                     &tmp_wel );

	if ( p_idx_mgr_element_array_ != nullptr ) {
		internal::LogOutput( log_type::DUMP,
		                     "%p --> %p \n",
		                     this, p_idx_mgr_element_array_ );
	}

	internal::LogOutput( log_type::DUMP,
	                     "%p --> %p \n",
	                     this, &invalid_element_storage_ );
	invalid_element_storage_.dump();

	internal::LogOutput( log_type::DUMP,
	                     "%p --> %p \n",
	                     this, &valid_element_storage_ );
	valid_element_storage_.dump();

	internal::LogOutput( log_type::DUMP,
	                     "%p --> %p \n",
	                     this, &tmp_wel );

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
	chunk_header_multi_slot* p_chms_arg,
	caller_context&&         caller_ctx_arg   //!< [in] caller context information
)
{
	caller_ctx_ = std::move( caller_ctx_arg );
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	RECORD_BACKTRACE_GET_BACKTRACE( alloc_bt_info_ );
	if ( p_chms_arg != nullptr ) {
		RECORD_BACKTRACE_INVALIDATE_BACKTRACE( free_bt_info_ );
	} else {
		// mallocで確保された領域なので、free_bt_info_側の情報は、クリアする。
		free_bt_info_ = bt_info {};
	}
#endif

	// 今は簡単な仕組みで実装する
	at_p_chms_.store( p_chms_arg, std::memory_order_release );
	at_mark_.store( 0 - reinterpret_cast<std::uintptr_t>( p_chms_arg ) - 1, std::memory_order_release );

	return;
}

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
	const param_chunk_allocation& ch_param_arg,      //!< [in] chunk allocation paramter
	const unsigned int            owner_tl_id_arg,   //!< [in] owner tl_id_
	chunk_list_statistics*        p_chms_stat_arg    //!< [in] pointer to statistics inforation to store activity
	)
  : p_next_chunk_( nullptr )
  , status_( chunk_control_status::EMPTY )
  , owner_tl_id_( owner_tl_id_arg )
  , num_of_accesser_( 0 )
  , p_statistics_( p_chms_stat_arg )
  , slot_conf_ {}
  , free_slot_idx_mgr_( -1, &( p_statistics_->alloc_collision_cnt_ ), &( p_statistics_->dealloc_collision_cnt_ ) )
  , p_free_slot_mark_( nullptr )
  , p_chunk_( nullptr )
{
	assert( p_statistics_ != nullptr );

	p_statistics_->chunk_num_++;

	alloc_new_chunk( ch_param_arg, owner_tl_id_ );

	return;
}

chunk_header_multi_slot::~chunk_header_multi_slot()
{
	delete[] p_free_slot_mark_;
	std::free( p_chunk_ );

	p_free_slot_mark_ = nullptr;
	p_chunk_          = nullptr;

	p_statistics_->chunk_num_--;

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
	// access可能状態なので、accesserのカウントアップを行い、access 開始を表明
	scoped_inout_counter<std::atomic<int>> cnt_inout( num_of_accesser_ );

	slot_conf_.size_of_one_piece_ = get_size_of_one_slot( ch_param_arg );
	slot_conf_.num_of_pieces_     = ( ch_param_arg.num_of_pieces_ >= 2 ) ? ch_param_arg.num_of_pieces_ : 2;

	return;
}

bool chunk_header_multi_slot::alloc_new_chunk(
	const param_chunk_allocation& ch_param_arg,     //!< [in] chunk allocation paramter
	const unsigned int            owner_tl_id_arg   //!< [in] owner tl_id_
)
{
	// access可能状態なので、accesserのカウントアップを行い、access 開始を表明
	scoped_inout_counter<std::atomic<int>> cnt_inout( num_of_accesser_ );

	// get ownership to allocate a chunk	// access権を取得を試みる
	chunk_control_status expect = chunk_control_status::EMPTY;
	bool                 result = status_.compare_exchange_strong( expect, chunk_control_status::RESERVED_ALLOCATION );
	if ( !result ) return false;
	// resultが真となったので、以降、このchunkに対する、領域確保の処理実行権を保有。

	set_slot_allocation_conf( ch_param_arg );

	std::size_t tmp_size;
	tmp_size = (std::size_t)slot_conf_.size_of_one_piece_ * (std::size_t)slot_conf_.num_of_pieces_;

	if ( tmp_size == 0 ) {
		status_.store( chunk_control_status::EMPTY, std::memory_order_release );
		return false;
	}

	p_free_slot_mark_ = new std::atomic<slot_status_mark>[slot_conf_.num_of_pieces_];
	if ( p_free_slot_mark_ == nullptr ) {
		status_.store( chunk_control_status::EMPTY, std::memory_order_release );
		return false;
	}
	for ( unsigned int i = 0; i < slot_conf_.num_of_pieces_; i++ ) {
		p_free_slot_mark_[i].store( slot_status_mark::FREE );
	}

	p_chunk_ = std::malloc( tmp_size );
	if ( p_chunk_ == nullptr ) {
		delete[] p_free_slot_mark_;
		p_free_slot_mark_ = nullptr;
		status_.store( chunk_control_status::EMPTY, std::memory_order_release );
		return false;
	}

	size_of_chunk_ = tmp_size;

	free_slot_idx_mgr_.set_idx_size( slot_conf_.num_of_pieces_ );
	owner_tl_id_.store( owner_tl_id_arg, std::memory_order_release );

	p_statistics_->valid_chunk_num_.fetch_add( 1 );
	p_statistics_->total_slot_cnt_.fetch_add( slot_conf_.num_of_pieces_ );
	p_statistics_->free_slot_cnt_.fetch_add( slot_conf_.num_of_pieces_ );

	status_.store( chunk_control_status::NORMAL, std::memory_order_release );

	return true;
}

void* chunk_header_multi_slot::allocate_mem_slot_impl(
	caller_context&& caller_ctx_arg   //!< [in] caller context information
)
{
	// access可能状態かどうかを事前チェック
	if ( status_.load( std::memory_order_acquire ) != chunk_control_status::NORMAL ) return nullptr;

	// access可能状態なので、accesserのカウントアップを行い、access 開始を表明
	scoped_inout_counter<std::atomic<int>> cnt_inout( num_of_accesser_ );

	// access権を取得を試みる
	if ( status_.load( std::memory_order_acquire ) != chunk_control_status::NORMAL ) return nullptr;

#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	p_statistics_->alloc_req_cnt_++;
#endif

	int read_idx = free_slot_idx_mgr_.pop();
	if ( read_idx == -1 ) {
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
		p_statistics_->alloc_req_err_cnt_++;
#endif
		return nullptr;
	}

	// フリースロットからスロットを確保したので、使用中のマークを付ける。
	// Since we got a slot from a free slot, mark it as in use.
	p_free_slot_mark_[read_idx].store( slot_status_mark::INUSE );

	// 得たスロットに対応する実際のメモリ領域のアドレス情報を生成する。
	// Generate the address information of the actual memory area corresponding to the obtained slot.
	std::uintptr_t p_ans_addr = reinterpret_cast<std::uintptr_t>( p_chunk_ );
	p_ans_addr += read_idx * slot_conf_.size_of_one_piece_;

	// always overwrite header information to fix a corrupted header
	slot_header* p_sh = reinterpret_cast<slot_header*>( p_ans_addr );
	p_sh->set_addr_of_chunk_header_multi_slot( this, std::move( caller_ctx_arg ) );

	return reinterpret_cast<void*>( p_ans_addr + get_slot_header_size() );
}

bool chunk_header_multi_slot::recycle_mem_slot_impl(
	void*            p_recycle_addr,   //!< [in] pointer to the memory slot to recycle.
	caller_context&& caller_ctx_arg    //!< [in] caller context information
)
{
	// access可能状態かどうかを事前チェック
	switch ( status_.load( std::memory_order_acquire ) ) {
		default:
		case chunk_control_status::EMPTY:
		case chunk_control_status::RESERVED_ALLOCATION:
		case chunk_control_status::DELETION:
			return false;   // すでにaccessできない状態になっている。
			break;

		case chunk_control_status::NORMAL:
		case chunk_control_status::RESERVED_DELETION:
		case chunk_control_status::ANNOUNCEMENT_DELETION:
			break;
	}

	// access可能状態なので、accesserのカウントアップを行い、access 開始を表明
	scoped_inout_counter<std::atomic<int>> cnt_inout( num_of_accesser_ );

	// まだaccess可能状態かどうかを事前チェック
	switch ( status_.load( std::memory_order_acquire ) ) {
		default:
		case chunk_control_status::EMPTY:
		case chunk_control_status::RESERVED_ALLOCATION:
		case chunk_control_status::DELETION:
			return false;   // すでにaccessできない状態になっている。
			break;

		case chunk_control_status::NORMAL:
		case chunk_control_status::RESERVED_DELETION:
		case chunk_control_status::ANNOUNCEMENT_DELETION:
			break;
	}

	// 自分のものかを確認する
	std::uintptr_t slot_header_addr_tmp = reinterpret_cast<std::uintptr_t>( p_recycle_addr );
	slot_header_addr_tmp -= get_slot_header_size();
	void* p_slot_addr = reinterpret_cast<void*>( slot_header_addr_tmp );

	// check correct address or not.
	if ( p_slot_addr < p_chunk_ ) return false;   // 自分のものではなかった。

	std::uintptr_t p_chking_addr = reinterpret_cast<std::uintptr_t>( p_slot_addr );
	p_chking_addr -= reinterpret_cast<std::uintptr_t>( p_chunk_ );

	std::uintptr_t idx = p_chking_addr / slot_conf_.size_of_one_piece_;
	std::uintptr_t adt = p_chking_addr % slot_conf_.size_of_one_piece_;

	if ( idx >= slot_conf_.num_of_pieces_ ) return false;   // 自分のものではなかった。
	if ( adt != 0 ) return false;                           // 自分のものではなかった。
		                                                    // ここに到達した時点で、自分のものであること判明

#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	p_statistics_->dealloc_req_cnt_++;
#endif

	slot_header*     p_sh            = reinterpret_cast<slot_header*>( p_slot_addr );
	slot_status_mark expect_not_free = slot_status_mark::INUSE;
#ifdef ALCONCURRENT_CONF_ENABLE_NON_REUSE_MEMORY_SLOT
	bool result = p_free_slot_mark_[idx].compare_exchange_strong( expect_not_free, slot_status_mark::DISCARED );
#else
	bool result = p_free_slot_mark_[idx].compare_exchange_strong( expect_not_free, slot_status_mark::FREE );
#endif

	if ( !result ) {
		// double free has occured.
		internal::LogOutput( log_type::ERR, "double free has occured." );
		internal::LogOutput(
			log_type::ERR,
			"previous free call is: %s, line no %d, function=%s",
			p_sh->caller_ctx_.p_caller_src_fname_, p_sh->caller_ctx_.caller_lineno_, p_sh->caller_ctx_.p_caller_func_name_ );
		internal::LogOutput(
			log_type::ERR,
			"current free call is: %s, line no %d, function=%s",
			caller_ctx_arg.p_caller_src_fname_, caller_ctx_arg.caller_lineno_, caller_ctx_arg.p_caller_func_name_ );
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
		static std::atomic_int double_free_counter( 0 );
		int                    id_count = double_free_counter.fetch_add( 1 );

		internal::LogOutput( log_type::ERR, "[%d-f] backtrace of previous free call", id_count );
		p_sh->free_bt_info_.dump_to_log( log_type::ERR, 'f', id_count );

		bt_info cur_bt;
		RECORD_BACKTRACE_GET_BACKTRACE( cur_bt );
		internal::LogOutput( log_type::ERR, "[%d-h] backtrace of current free call", id_count );
		cur_bt.dump_to_log( log_type::ERR, 'h', id_count );

		internal::LogOutput( log_type::ERR, "[%d-a] backtrace of allocation call", id_count );
		p_sh->alloc_bt_info_.dump_to_log( log_type::ERR, 'a', id_count );
#endif
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
		p_statistics_->dealloc_req_err_cnt_++;
#endif
		return true;
	}

	// OK. correct address
	p_sh->caller_ctx_ = std::move( caller_ctx_arg );

#ifdef ALCONCURRENT_CONF_ENABLE_NON_REUSE_MEMORY_SLOT
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	RECORD_BACKTRACE_GET_BACKTRACE( p_sh->free_bt_info_ );
#endif
#else
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	RECORD_BACKTRACE_GET_BACKTRACE( p_sh->free_bt_info_ );
	RECORD_BACKTRACE_INVALIDATE_BACKTRACE( p_sh->alloc_bt_info_ );
#endif

	free_slot_idx_mgr_.push( idx );
#endif

	return true;
}

void* chunk_header_multi_slot::try_allocate_mem_slot_impl(
	const unsigned int expect_tl_id_arg,   //!< [in] owner tl_id_
	const unsigned int owner_tl_id_arg,    //!< [in] owner tl_id_
	caller_context&&   caller_ctx_arg      //!< [in] caller context information
)
{
	// access可能状態かどうかを事前チェック
	switch ( status_.load( std::memory_order_acquire ) ) {
		default:
		case chunk_control_status::EMPTY:
		case chunk_control_status::RESERVED_ALLOCATION:
		case chunk_control_status::DELETION:
		case chunk_control_status::ANNOUNCEMENT_DELETION:
			return nullptr;
			break;

		case chunk_control_status::NORMAL:
		case chunk_control_status::RESERVED_DELETION:
			break;
	}

	// access可能状態なので、accesserのカウントアップを行い、access 開始を表明
	scoped_inout_counter<std::atomic<int>> cnt_inout( num_of_accesser_ );

	// access権を取得を試みる
	chunk_control_status cur_as_st = status_.load( std::memory_order_acquire );
	switch ( cur_as_st ) {
		default:
		case chunk_control_status::EMPTY:
		case chunk_control_status::RESERVED_ALLOCATION:
		case chunk_control_status::DELETION:
		case chunk_control_status::ANNOUNCEMENT_DELETION:
			return nullptr;
			break;

		case chunk_control_status::NORMAL:
		case chunk_control_status::RESERVED_DELETION:
			break;
	}

	// オーナー権の確認 or 確保を試みる
	unsigned int expect_tl_id = expect_tl_id_arg;
	if ( !owner_tl_id_.compare_exchange_strong( expect_tl_id, owner_tl_id_arg ) ) {
		return nullptr;
	}

	// NORMAL状態へ復帰を試みる
	if ( cur_as_st == chunk_control_status::RESERVED_DELETION ) {
		if ( !unset_delete_reservation() ) {
			return nullptr;
		}
	}

	// オーナー権の確保と、NORMAL状態への復帰に成功したので、メモリスロットの確保を試みる
	return allocate_mem_slot( std::move( caller_ctx_arg ) );
}

bool chunk_header_multi_slot::set_delete_reservation( void )
{
	// access可能状態なので、accesserのカウントアップを行い、access 開始を表明
	scoped_inout_counter<std::atomic<int>> cnt_inout( num_of_accesser_ );

	chunk_control_status expect = chunk_control_status::NORMAL;
	bool                 result = std::atomic_compare_exchange_strong( &status_, &expect, chunk_control_status::RESERVED_DELETION );
	return result;
}

bool chunk_header_multi_slot::unset_delete_reservation( void )
{
	// access可能状態なので、accesserのカウントアップを行い、access 開始を表明
	scoped_inout_counter<std::atomic<int>> cnt_inout( num_of_accesser_ );

	chunk_control_status expect = chunk_control_status::RESERVED_DELETION;
	bool                 result = std::atomic_compare_exchange_strong( &status_, &expect, chunk_control_status::NORMAL );
	return result;
}

bool chunk_header_multi_slot::exec_deletion( void )
{
	// access者がいないことを確認する
	if ( num_of_accesser_.load() != 0 ) return false;

	if ( owner_tl_id_.load( std::memory_order_acquire ) != NON_OWNERED_TL_ID ) {
		// 明確な所有者スレッドが存在するため、削除対象としない。
		return false;
	}

	chunk_control_status expect;
	bool                 result;

	// 削除予約付けを試みる。
	expect = chunk_control_status::NORMAL;
	result = status_.compare_exchange_strong( expect, chunk_control_status::RESERVED_DELETION );
	if ( !result ) {
		// 削除予約付けに失敗した。ただし、すでに、削除予約付け済みかもしれないため、チェックする。
		if ( expect != chunk_control_status::RESERVED_DELETION ) {
			// すでに何らかのほかの状態に遷移済みのため、削除を取りやめる。
			return false;
		}
	}
	for ( unsigned int ii = 0; ii < slot_conf_.num_of_pieces_; ii++ ) {
		if ( p_free_slot_mark_[ii].load( std::memory_order_acquire ) != slot_status_mark::FREE ) {
			// 使用中のslotが見つかったため、削除を取りやめる。
			return false;
		}
	}

	// DELETEの予告を試みる。
	expect = chunk_control_status::RESERVED_DELETION;
	result = status_.compare_exchange_strong( expect, chunk_control_status::ANNOUNCEMENT_DELETION );
	if ( !result ) return false;

	for ( unsigned int ii = 0; ii < slot_conf_.num_of_pieces_; ii++ ) {
		if ( p_free_slot_mark_[ii].load( std::memory_order_acquire ) != slot_status_mark::FREE ) {
			// 使用中のslotが見つかったため、削除を取りやめる。
			status_.store( chunk_control_status::RESERVED_DELETION, std::memory_order_release );
			return false;
		}
	}

	// access者の有無を再確認
	if ( num_of_accesser_.load() != 0 ) {
		// access者がいるため、削除を取りやめる。
		status_.store( chunk_control_status::RESERVED_DELETION, std::memory_order_release );
		return false;
	}

	// DELETEの実行権の獲得を試みる。
	expect = chunk_control_status::ANNOUNCEMENT_DELETION;
	result = status_.compare_exchange_strong( expect, chunk_control_status::ANNOUNCEMENT_DELETION );
	if ( !result ) return false;

	std::lock_guard<std::recursive_mutex> lg( dynamic_tls_global_exclusive_control_for_destructions );

	std::free( p_chunk_ );
	delete[] p_free_slot_mark_;

	p_chunk_          = nullptr;
	p_free_slot_mark_ = nullptr;
	owner_tl_id_.store( NON_OWNERED_TL_ID, std::memory_order_release );
	free_slot_idx_mgr_.clear();

	p_statistics_->valid_chunk_num_.fetch_sub( 1 );
	p_statistics_->total_slot_cnt_.fetch_sub( slot_conf_.num_of_pieces_ );
	p_statistics_->free_slot_cnt_.fetch_sub( slot_conf_.num_of_pieces_ );

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
		internal::LogOutput( log_type::ERR,
		                     "a header of slot_header is corrupted %p\n",
		                     p_addr );
	}

	return ret;
}

chunk_statistics chunk_header_multi_slot::get_statistics( void ) const
{
	// access可能状態なので、accesserのカウントアップを行い、access 開始を表明
	scoped_inout_counter<std::atomic<int>> cnt_inout( num_of_accesser_ );

	chunk_statistics ans { 0 };

	ans = p_statistics_->get_statistics();

	ans.alloc_conf_ = slot_conf_;

	return ans;
}

/*!
 * @brief	dump for debug
 */
void chunk_header_multi_slot::dump( void )
{
	// access可能状態なので、accesserのカウントアップを行い、access 開始を表明
	scoped_inout_counter<std::atomic<int>> cnt_inout( num_of_accesser_ );

	if ( p_chunk_ != nullptr ) {
		internal::LogOutput( log_type::DUMP,
		                     "object chunk_%p as %p \n",
		                     p_chunk_, p_chunk_ );
	}

	if ( p_free_slot_mark_ != nullptr ) {
		internal::LogOutput( log_type::DUMP,
		                     "object p_free_slot_mark_%p as %p {\n",
		                     p_free_slot_mark_, p_free_slot_mark_ );

		for ( std::size_t i = 0; i < slot_conf_.num_of_pieces_; i++ ) {
			const char* p_value_str = nullptr;
			switch ( p_free_slot_mark_[i].load() ) {
				case slot_status_mark::FREE: {
					p_value_str = "slot_status_mark::FREE";
				} break;
				case slot_status_mark::INUSE: {
					p_value_str = "slot_status_mark::INUSE";
				} break;
				case slot_status_mark::DISCARED: {
					p_value_str = "slot_status_mark::DISCARED";
				} break;
				default: {
					p_value_str = "slot_status_mark::unknown";
				} break;
			}
			internal::LogOutput( log_type::DUMP,
			                     "%zu = %s \n",
			                     i, p_value_str );
		}
		internal::LogOutput( log_type::DUMP, "}\n" );
	}

	internal::LogOutput( log_type::DUMP,
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

	free_slot_idx_mgr_.dump();

	return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::atomic<unsigned int> chunk_list::tl_chunk_param::tl_id_counter_( 1 );

unsigned int chunk_list::tl_chunk_param::get_new_tl_id( void )
{
#ifdef ALCONCURRENT_CONF_PREFER_TO_SHARE_CHUNK
	// tl_idを固定値とするとことで、chunkの区別がつかないようにする。
	return 1;
#else
	unsigned int ans = tl_id_counter_.fetch_add( 1 );
	if ( ans == NON_OWNERED_TL_ID ) {
		ans = tl_id_counter_.fetch_add( 1 );
	}
	return ans;
#endif
}

chunk_list::tl_chunk_param::tl_chunk_param(
	chunk_list*  p_owner_chunk_list_arg,   //!< [in] pointer to onwer chunk_list
	unsigned int init_num_of_pieces_arg    //!< [in] initiall number of slots for allocation
	)
  : p_owner_chunk_list_( p_owner_chunk_list_arg )
  , tl_id_( get_new_tl_id() )
  , num_of_pieces_( init_num_of_pieces_arg )
  , tls_p_hint_chunk( nullptr )
{
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool chunk_list::tl_chunk_param_destructor::release(
	tl_chunk_param& data   //!< [in/out] ファンクタの処理対象となるインスタンスへの参照
)
{
	data.p_owner_chunk_list_->release_all_of_ownership( data.tl_id_, nullptr );
	return true;
}

void chunk_list::tl_chunk_param_destructor::destruct(
	tl_chunk_param& data   //!< [in/out] ファンクタの処理対象となるインスタンスへの参照
)
{
	// data.p_owner_chunk_list_->release_all_of_ownership( data.tl_id_, nullptr );
	return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
chunk_list::chunk_list(
	const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
	)
  : chunk_param_( ch_param_arg )
  , p_top_chunk_( nullptr )
  , tls_hint_()
  , statistics_()
{
	unsigned int cur_tl_id = tls_hint_.get_tls_instance( this, chunk_param_.num_of_pieces_ ).tl_id_;

	chunk_header_multi_slot* p_new_chms = new chunk_header_multi_slot( ch_param_arg, cur_tl_id, &statistics_ );
	p_top_chunk_.store( p_new_chms, std::memory_order_release );

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

void chunk_list::mark_as_reserved_deletion(
	unsigned int             target_tl_id_arg,    //!< [in] 削除予約対象に変更するtl_id_
	chunk_header_multi_slot* p_non_deletion_arg   //!< [in] 削除予約対象としないchunk_header_multi_slotへのポインタ
)
{
	chunk_header_multi_slot* p_cur_chms = p_top_chunk_.load( std::memory_order_acquire );
	while ( p_cur_chms != nullptr ) {
		if ( p_cur_chms->owner_tl_id_.load( std::memory_order_acquire ) == target_tl_id_arg ) {
			if ( p_cur_chms != p_non_deletion_arg ) {
				p_cur_chms->set_delete_reservation();
			}
		}
		chunk_header_multi_slot* p_next_chms = p_cur_chms->p_next_chunk_.load( std::memory_order_acquire );
		p_cur_chms                           = p_next_chms;
	}

	return;
}

unsigned int chunk_list::get_cur_max_slot_size(
	unsigned int target_tl_id_arg   //!< [in] オーナー権を開放する対象のtl_id_
)
{
	unsigned int             ans_cur_max_size = 0;
	chunk_header_multi_slot* p_cur_chms       = p_top_chunk_.load( std::memory_order_acquire );
	while ( p_cur_chms != nullptr ) {
		if ( p_cur_chms->owner_tl_id_.load( std::memory_order_acquire ) == target_tl_id_arg ) {
			const param_chunk_allocation& pca = p_cur_chms->get_param_chunk_allocation();
			if ( pca.num_of_pieces_ > ans_cur_max_size ) {
				ans_cur_max_size = pca.num_of_pieces_;
			}
		}
		chunk_header_multi_slot* p_next_chms = p_cur_chms->p_next_chunk_.load( std::memory_order_acquire );
		p_cur_chms                           = p_next_chms;
	}

	return ans_cur_max_size;
}

void chunk_list::release_all_of_ownership(
	unsigned int                   target_tl_id_arg,         //!< [in] オーナー権を開放する対象のtl_id_
	const chunk_header_multi_slot* p_non_release_chunk_arg   //!< [in] オーナー権解放の対象外とするchunk_header_multi_slotへのポインタ。指定しない場合は、nullptrを指定すること。
)
{
	chunk_header_multi_slot* p_cur_chms = p_top_chunk_.load( std::memory_order_acquire );
	while ( p_cur_chms != nullptr ) {
		if ( p_cur_chms != p_non_release_chunk_arg ) {
			if ( p_cur_chms->owner_tl_id_.load( std::memory_order_acquire ) == target_tl_id_arg ) {
				p_cur_chms->owner_tl_id_.store( NON_OWNERED_TL_ID, std::memory_order_release );
			}
		}
		chunk_header_multi_slot* p_next_chms = p_cur_chms->p_next_chunk_.load( std::memory_order_acquire );
		p_cur_chms                           = p_next_chms;
	}

	return;
}

void* chunk_list::allocate_mem_slot(
	caller_context&& caller_ctx_arg   //!< [in] caller context information
)
{
	void*          p_ans  = nullptr;
	caller_context cur_cc = std::move( caller_ctx_arg );

	// hintに登録されたchunkから空きスロット検索を開始する。
	tl_chunk_param& hint_params = tls_hint_.get_tls_instance( this, chunk_param_.num_of_pieces_ );

	chunk_header_multi_slot* const p_start_chms = hint_params.tls_p_hint_chunk;
	chunk_header_multi_slot*       p_cur_chms   = nullptr;
	if ( p_start_chms != nullptr ) {
		// 自スレッド用の割り当てが終わっている場合、自スレッド用のchunk_header_multi_slotからの割り当てを試みる
		p_cur_chms = p_start_chms;
		while ( p_cur_chms != nullptr ) {
			if ( p_cur_chms->owner_tl_id_.load( std::memory_order_acquire ) == hint_params.tl_id_ ) {
				// tl_id_が一致する場合は、自スレッドの持ち物となる。自スレッドの持ち物のchunk_header_multi_slotからメモリを割り当てる。
				p_ans = p_cur_chms->allocate_mem_slot( caller_context( cur_cc ) );
				if ( p_ans != nullptr ) {
					// 空きスロットが見つかれば終了
					hint_params.tls_p_hint_chunk = p_cur_chms;
					return p_ans;
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

		// 自スレッドの持ち物で、削除予約されたchunkからの再使用を試みる。
		// ただし、すでに削除されてしまっているかもしれない。
		// なお、自スレッドの持ち物だったとしても、処理を行っている最中に、削除ー＞別スレッドに再割り当てー＞削除予約状態となっている可能性もある。
		// たとえ別スレッドの物になっていても、新たにメモリを割り当てるよりは性能観点でましな可能性が高いため、そのまま割り当てを試みる。
		p_cur_chms = p_top_chunk_.load( std::memory_order_acquire );
		while ( p_cur_chms != nullptr ) {
			p_ans = p_cur_chms->try_allocate_mem_slot_from_reserved_deletion( hint_params.tl_id_, caller_context( cur_cc ) );
			if ( p_ans != nullptr ) {
				// スロット確保成功
				hint_params.tls_p_hint_chunk = p_cur_chms;   // 別スレッドのものかもしれないが、hintを更新する。次の呼び出しで、是正される。
				return p_ans;
			}
			chunk_header_multi_slot* p_next_chms = p_cur_chms->p_next_chunk_.load( std::memory_order_acquire );
			p_cur_chms                           = p_next_chms;
		}
	}

	// オーナーなしchunk_header_multi_slotからの割り当てを試みる
	p_cur_chms = p_top_chunk_.load( std::memory_order_acquire );
	while ( p_cur_chms != nullptr ) {
		p_ans = p_cur_chms->try_get_ownership_allocate_mem_slot( hint_params.tl_id_, caller_context( cur_cc ) );
		if ( p_ans != nullptr ) {
			// オーナー権とスロット確保成功
			hint_params.tls_p_hint_chunk = p_cur_chms;   // 別スレッドのものかもしれないが、hintを更新する。次の呼び出しで、是正される。
			return p_ans;
		}
		chunk_header_multi_slot* p_next_chms = p_cur_chms->p_next_chunk_.load( std::memory_order_acquire );
		p_cur_chms                           = p_next_chms;
	}

	// 以降は、新しいメモリ領域をmallocで確保することになる。
	// 確保するスロットサイズを決める。既存のスロット数では足りなかったので、スロット数を2倍化する。
	// unsigned int cur_slot_num = hint_params.num_of_pieces_;
	unsigned int cur_slot_num = get_cur_max_slot_size( hint_params.tl_id_ );
	unsigned int new_slot_num = cur_slot_num * 2;
	if ( cur_slot_num == 0 ) {
		new_slot_num = chunk_param_.num_of_pieces_;
	}
	if ( ( (size_t)cur_slot_num * (size_t)chunk_param_.size_of_one_piece_ ) >= MAX_ALLOC_SIZE_LIMIT ) {
		// オーバーフローしてしまったら、2倍化を取りやめる。
		new_slot_num = cur_slot_num;
	}
	param_chunk_allocation cur_alloc_conf { chunk_param_.size_of_one_piece_, new_slot_num };

	// 空きchunk_header_multi_slotに対して、再利用を試みる。
	p_cur_chms = p_top_chunk_.load( std::memory_order_acquire );
	while ( p_cur_chms != nullptr ) {
		if ( p_cur_chms->status_.load( std::memory_order_acquire ) == chunk_control_status::EMPTY ) {
			// 空きchunk_header_multi_slotに対して、メモリ割り当てを試みる。ここでロックが発生する可能性がある。
			if ( p_cur_chms->alloc_new_chunk( cur_alloc_conf, hint_params.tl_id_ ) ) {
				// 再割り当て成功
				// 既存のオーナー権を持つチャンクを解放
				release_all_of_ownership( hint_params.tl_id_, p_cur_chms );
				// メモリスロットを確保する。
				p_ans = p_cur_chms->allocate_mem_slot( caller_context( cur_cc ) );
				if ( p_ans != nullptr ) {
					hint_params.tls_p_hint_chunk = p_cur_chms;

					// chunkの登録が終わったので、スロット数の2倍化された値を登録する。
					hint_params.num_of_pieces_ = new_slot_num;

					// 2倍化chunkが登録されたので、それまでのchunkは削除候補に変更にする。
					mark_as_reserved_deletion( hint_params.tl_id_, p_cur_chms );

					return p_ans;
				}
			} else {
				// 再割り当て失敗
				if ( p_cur_chms->status_.load( std::memory_order_acquire ) == chunk_control_status::NORMAL ) {
					// 自身による再割り当てに失敗はしたが、既に別のスレッドで再割り当て完了済みだった。
					// たとえ別スレッドの物になっていても、新たにメモリを割り当てるよりは性能観点でましな可能性が高いため、そのまま割り当てを試みる。
					p_ans = p_cur_chms->allocate_mem_slot( caller_context( cur_cc ) );
					if ( p_ans != nullptr ) {
						hint_params.tls_p_hint_chunk = p_cur_chms;
						return p_ans;
					}
				}
			}
		}

		// リンクリストをたどって、次に検索するchunkを取り出す。
		// 検索を開始したchunkに到着したら、空きスロットが見つからなかったことを示すので、検索を終了する。
		chunk_header_multi_slot* p_next_chms = p_cur_chms->p_next_chunk_.load( std::memory_order_acquire );
		p_cur_chms                           = p_next_chms;
	}

	// 既存のchunkの再利用に失敗したので、新しいchunkを確保する。
	// new演算子を使用するため、ここでもロックが発生する可能性がある。
	chunk_header_multi_slot* p_new_chms = new chunk_header_multi_slot( cur_alloc_conf, hint_params.tl_id_, &statistics_ );
	if ( p_new_chms == nullptr ) {
		return nullptr;   // TODO: should throw exception ?
	}
	p_ans = p_new_chms->allocate_mem_slot( caller_context( cur_cc ) );
	if ( p_ans == nullptr ) {
		delete p_new_chms;
		return nullptr;   // TODO: should throw exception ?
	}

	// 既存のオーナー権を持つチャンクを解放。
	release_all_of_ownership( hint_params.tl_id_, nullptr );   // まだ新しいchunkをリストに登録していないので、nullptrを指定する。

	// 新たに確保したchunkを、chunkリストの先頭に追加する。
	chunk_header_multi_slot* p_cur_top = p_top_chunk_.load( std::memory_order_acquire );
	do {
		p_new_chms->p_next_chunk_.store( p_cur_top, std::memory_order_release );
	} while ( !p_top_chunk_.compare_exchange_weak( p_cur_top, p_new_chms ) );

	hint_params.tls_p_hint_chunk = p_new_chms;

	// スロット数の2倍化されたchunkの登録が終わったので、スロット数の2倍化された値を登録する。
	hint_params.num_of_pieces_ = new_slot_num;

	// 2倍化chunkが登録されたので、それまでのchunkは削除候補に変更にする。
	mark_as_reserved_deletion( hint_params.tl_id_, p_new_chms );

	return p_ans;
}

bool chunk_list::recycle_mem_slot(
	void*            p_recycle_slot,   //!< [in] pointer to the memory slot to recycle.
	caller_context&& caller_ctx_arg    //!< [in] caller context information
)
{
	chunk_header_multi_slot* p_cur_chms = p_top_chunk_.load( std::memory_order_acquire );
	while ( p_cur_chms != nullptr ) {
		bool ret = p_cur_chms->recycle_mem_slot( p_recycle_slot, std::move( caller_ctx_arg ) );
		if ( ret ) return true;
		chunk_header_multi_slot* p_next_chms = p_cur_chms->p_next_chunk_.load( std::memory_order_acquire );
		p_cur_chms                           = p_next_chms;
	}

#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	statistics_.dealloc_req_err_cnt_++;
#endif
	return false;
}

void chunk_list::prune( void )
{
	chunk_header_multi_slot* p_cur_chms;
	p_cur_chms = p_top_chunk_.load( std::memory_order_acquire );

	while ( p_cur_chms != nullptr ) {
		p_cur_chms->exec_deletion();
		chunk_header_multi_slot* p_next_chms = p_cur_chms->p_next_chunk_.load( std::memory_order_acquire );
		p_cur_chms                           = p_next_chms;
	}

	return;
}

/*!
 * @brief	get statistics
 */
chunk_statistics chunk_list::get_statistics( void ) const
{
	chunk_statistics ans = statistics_.get_statistics();

	ans.alloc_conf_ = param_chunk_allocation { chunk_param_.size_of_one_piece_, 0 };

	return ans;
}

}   // namespace internal

////////////////////////////////////////////////////////////////////////////////////////////////////////
general_mem_allocator::general_mem_allocator( void )
  : pr_ch_size_( 0 )
  , up_param_ch_array_()
  , exclusive_ctl_of_prune_( false )
{
	return;
}

general_mem_allocator::general_mem_allocator(
	const param_chunk_allocation* p_param_array,   //!< [in] pointer to parameter array
	unsigned int                  num              //!< [in] array size
	)
  : pr_ch_size_( 0 )
  , up_param_ch_array_()
  , exclusive_ctl_of_prune_( false )
{
#ifdef ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
#else
	set_param( p_param_array, num );
#endif
	return;
}

general_mem_allocator::~general_mem_allocator()
{
	return;
}

void* general_mem_allocator::allocate(
	std::size_t      n,               //!< [in] required memory size
	caller_context&& caller_ctx_arg   //!< [in] caller context information
)
{
	void* p_ans = nullptr;
	for ( unsigned int i = 0; i < pr_ch_size_; i++ ) {
		// liner search
		if ( up_param_ch_array_[i].param_.size_of_one_piece_ >= n ) {
			p_ans = up_param_ch_array_[i].up_chunk_lst_->allocate_mem_slot( std::move( caller_ctx_arg ) );
			if ( p_ans != nullptr ) {
				return p_ans;
			}
		}
	}

	// 対応するサイズのメモリスロットが見つからなかったので、mallocでメモリ領域を確保する。
	internal::slot_header* p_sh = reinterpret_cast<internal::slot_header*>( std::malloc( n + internal::get_slot_header_size() ) );

	p_sh->set_addr_of_chunk_header_multi_slot( nullptr, std::move( caller_ctx_arg ) );

	std::uintptr_t tmp_ans = reinterpret_cast<std::uintptr_t>( p_sh );
	tmp_ans += internal::get_slot_header_size();
	p_ans = reinterpret_cast<void*>( tmp_ans );

	return p_ans;
}

void general_mem_allocator::deallocate(
	void*            p_mem,           //!< [in] pointer to allocated memory by allocate()
	caller_context&& caller_ctx_arg   //!< [in] caller context information
)
{
	if ( p_mem == nullptr ) return;

	internal::slot_chk_result chk_ret = internal::chunk_header_multi_slot::get_chunk( p_mem );

	if ( chk_ret.correct_ ) {
		if ( chk_ret.p_chms_ != nullptr ) {
			chk_ret.p_chms_->recycle_mem_slot( p_mem, std::move( caller_ctx_arg ) );
		} else {
			std::uintptr_t tmp_addr = reinterpret_cast<std::uintptr_t>( p_mem );
			tmp_addr -= internal::get_slot_header_size();
			internal::slot_header* p_sh = reinterpret_cast<internal::slot_header*>( tmp_addr );
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
			RECORD_BACKTRACE_GET_BACKTRACE( p_sh->free_bt_info_ );
			RECORD_BACKTRACE_INVALIDATE_BACKTRACE( p_sh->alloc_bt_info_ );
#endif
			std::free( reinterpret_cast<void*>( p_sh ) );
		}
	} else {
		internal::LogOutput( log_type::WARN, "Header is corrupted. full search correct chunk and try free" );
		for ( unsigned int i = 0; i < pr_ch_size_; i++ ) {
			bool ret = up_param_ch_array_[i].up_chunk_lst_->recycle_mem_slot( p_mem, std::move( caller_ctx_arg ) );
			if ( ret ) {
				internal::LogOutput( log_type::WARN, "Header is corrupted, but luckily success to find and free" );
				return;
			}
		}

		// header is corrupted and unknown memory slot deallocation is requested. try to call free()
		internal::LogOutput( log_type::WARN, "header is corrupted and unknown memory slot deallocation is requested. try to free by calling free()" );
		std::free( p_mem );   // TODO should not do this ?
	}

	return;
}

void general_mem_allocator::prune( void )
{
	bool expected = false;
	bool ret      = exclusive_ctl_of_prune_.compare_exchange_strong( expected, true );

	if ( !ret ) return;   // other thread/s is executing now

	for ( unsigned int ii = 0; ii < pr_ch_size_; ii++ ) {
		up_param_ch_array_[ii].up_chunk_lst_->prune();
	}

	exclusive_ctl_of_prune_.store( false, std::memory_order_release );
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
 * @brief	get statistics
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
	          "chunk conf{.size=%d, .num=%d}, chunk_num: %d, valid chunk_num: %d"
	          ", total_slot=%d, free_slot=%d, consum cnt=%d, max consum cnt=%d"
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	          ", alloc cnt=%d, alloc err=%d, dealloc cnt=%d, dealloc err=%d, alloc_colli=%d, dealloc_colli=%d"
#endif
	          ,
	          (int)alloc_conf_.size_of_one_piece_,
	          (int)alloc_conf_.num_of_pieces_,
	          (int)chunk_num_,
	          (int)valid_chunk_num_,
	          (int)total_slot_cnt_,
	          (int)free_slot_cnt_,
	          (int)consum_cnt_,
	          (int)max_consum_cnt_
#ifdef ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
	          ,
	          (int)alloc_req_cnt_,
	          (int)error_alloc_req_cnt_,
	          (int)dealloc_req_cnt_,
	          (int)error_dealloc_req_cnt_,
	          (int)alloc_collision_cnt_,
	          (int)dealloc_collision_cnt_
#endif
	);

	return std::string( buf );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
std::tuple<bool,
           alpha::concurrent::bt_info,
           alpha::concurrent::bt_info>
get_backtrace_info(
	const void* p_mem   //!< [in] pointer to allocated memory by allocate()
)
{
	std::uintptr_t tmp_addr = reinterpret_cast<std::uintptr_t>( p_mem );
	tmp_addr -= internal::get_slot_header_size();
	internal::slot_header* p_sh = reinterpret_cast<internal::slot_header*>( tmp_addr );

#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	return std::tuple<bool,
	                  alpha::concurrent::bt_info,
	                  alpha::concurrent::bt_info>(
		p_sh->chk_header_data().correct_,
		p_sh->alloc_bt_info_,
		p_sh->free_bt_info_ );
#else
	return std::tuple<bool,
	                  alpha::concurrent::bt_info,
	                  alpha::concurrent::bt_info>(
		p_sh->chk_header_data().correct_,
		bt_info(),
		bt_info() );
#endif
}

void output_backtrace_info(
	const log_type lt,     //!< [in] log type
	const void*    p_mem   //!< [in] pointer to allocated memory by allocate()
)
{
	static std::atomic_int counter( 0 );
	int                    id_count = counter.fetch_add( 1 );

	std::uintptr_t tmp_addr = reinterpret_cast<std::uintptr_t>( p_mem );
	tmp_addr -= internal::get_slot_header_size();
	internal::slot_header* p_sh = reinterpret_cast<internal::slot_header*>( tmp_addr );

	auto chk_ret = p_sh->chk_header_data();
	internal::LogOutput(
		lt,
		"[%d] header check result of %p: correct_=%s, p_chms_=%p",
		id_count, p_mem, chk_ret.correct_ ? "true" : "false", chk_ret.p_chms_ );

#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
	internal::LogOutput( lt, "[%d-a] alloc_bt_info_ of %p", id_count, p_mem );
	p_sh->alloc_bt_info_.dump_to_log( lt, 'a', id_count );
	internal::LogOutput( lt, "[%d-f] free_bt_info_ of %p", id_count, p_mem );
	p_sh->free_bt_info_.dump_to_log( lt, 'f', id_count );
#else
	internal::LogOutput( lt, "[%d] no backtrace information, because the library is not compiled with ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE", id_count );
#endif

	return;
}

}   // namespace concurrent
}   // namespace alpha
