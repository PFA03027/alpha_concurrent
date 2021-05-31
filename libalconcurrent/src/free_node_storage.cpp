/*!
 * @file	free_node_storage.hpp
 * @brief
 * @author	Teruaki Ata
 * @date	Created on 2021/01/03
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#include "alconcurrent/free_node_storage.hpp"
#include "alconcurrent/lf_mem_alloc.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

thread_local_fifo_list::thread_local_fifo_list( void )
  : head_( nullptr )
  , tail_( nullptr )
{
	return;
}

thread_local_fifo_list::~thread_local_fifo_list()
{
	node_pointer p_cur = head_;

	if ( p_cur != nullptr ) {
		do {
			node_pointer const p_nxt = p_cur->get_next( next_slot_idx_ );
			delete p_cur;
			p_cur = p_nxt;
		} while ( p_cur != nullptr );
	}

	return;
}

void thread_local_fifo_list::push( thread_local_fifo_list::node_pointer const p_push_node )
{
	p_push_node->set_next( nullptr, next_slot_idx_ );

	if ( is_empty() ) {
		head_ = p_push_node;
		tail_ = p_push_node;
	} else {
		tail_->set_next( p_push_node, next_slot_idx_ );
		tail_ = p_push_node;
	}

	return;
}

thread_local_fifo_list::node_pointer thread_local_fifo_list::pop( void )
{
	if ( is_empty() ) {
		return nullptr;
	}

	node_pointer p_ans = head_;

	node_pointer p_cur_next = head_->get_next( next_slot_idx_ );
	head_                   = p_cur_next;

	if ( head_ == nullptr ) {
		tail_ = nullptr;
	}

	return p_ans;
}

fifo_free_nd_list::fifo_free_nd_list( void )
  : head_( nullptr )
  , tail_( nullptr )
{
	return;
}

fifo_free_nd_list::~fifo_free_nd_list()
{
	node_pointer p_cur = head_.load( std::memory_order_acquire );
	head_.store( nullptr, std::memory_order_release );
	tail_.store( nullptr, std::memory_order_release );

	if ( p_cur != nullptr ) {
		// 先頭ノードは番兵のため、nullptrであることはありえないが、チェックする。
		do {
			node_pointer const p_nxt = p_cur->get_next( next_slot_idx_ );
			delete p_cur;
			p_cur = p_nxt;
		} while ( p_cur != nullptr );
	}

	return;
}

void fifo_free_nd_list::initial_push( fifo_free_nd_list::node_pointer const p_push_node )
{
	if ( ( head_.load() != nullptr ) || ( tail_.load() != nullptr ) ) {
		LogOutput( log_type::ERR, "Because already this fifo_free_nd_list instance has sentinel node, fail to initial_push()." );
		return;
	}

	head_.store( p_push_node, std::memory_order_release );
	tail_.store( p_push_node, std::memory_order_release );

	return;
}

void fifo_free_nd_list::push( fifo_free_nd_list::node_pointer const p_push_node )
{
	p_push_node->set_next( nullptr, next_slot_idx_ );

	scoped_hazard_ref scoped_ref_cur( hzrd_ptr_, (int)hazard_ptr_idx::PUSH_FUNC_LAST );
	scoped_hazard_ref scoped_ref_nxt( hzrd_ptr_, (int)hazard_ptr_idx::PUSH_FUNC_NEXT );

	while ( true ) {
		node_pointer p_cur_last = tail_.load( std::memory_order_acquire );

		hzrd_ptr_.regist_ptr_as_hazard_ptr( p_cur_last, (int)hazard_ptr_idx::PUSH_FUNC_LAST );

		if ( p_cur_last != tail_.load( std::memory_order_acquire ) ) continue;

		node_pointer p_cur_next = p_cur_last->get_next( next_slot_idx_ );
		hzrd_ptr_.regist_ptr_as_hazard_ptr( p_cur_next, (int)hazard_ptr_idx::PUSH_FUNC_NEXT );
		if ( p_cur_next != p_cur_last->get_next( next_slot_idx_ ) ) continue;

		if ( p_cur_next == nullptr ) {
			if ( p_cur_last->next_CAS( &p_cur_next, p_push_node, next_slot_idx_ ) ) {
				tail_.compare_exchange_weak( p_cur_last, p_push_node );
				return;   // 追加完了
			}
		} else {
			// tail_を更新する。
			// ここで、プリエンプションして、tail_がA->B->A'となった時、p_cur_lastが期待値とは異なるが、
			// ハザードポインタにA相当を確保しているので、A'は現れない。よって、このようなABA問題は起きない。
			tail_.compare_exchange_weak( p_cur_last, p_cur_next );
		}
	}
	return;
}

/*!
	 * @breif	FIFOキューからノードを取り出す。
	 *
	 * @return	取り出された管理ノードへのポインタ。nullptrの場合、FIFOキューが空だったことを示す。管理対象ポインタは、
	 */
fifo_free_nd_list::node_pointer fifo_free_nd_list::pop( void )
{
	scoped_hazard_ref scoped_ref_first( hzrd_ptr_, (int)hazard_ptr_idx::POP_FUNC_FIRST );
	scoped_hazard_ref scoped_ref_last( hzrd_ptr_, (int)hazard_ptr_idx::POP_FUNC_LAST );
	scoped_hazard_ref scoped_ref_next( hzrd_ptr_, (int)hazard_ptr_idx::POP_FUNC_NEXT );

	while ( true ) {
		node_pointer p_cur_first = head_.load( std::memory_order_acquire );
		node_pointer p_cur_last  = tail_.load( std::memory_order_acquire );

		hzrd_ptr_.regist_ptr_as_hazard_ptr( p_cur_first, (int)hazard_ptr_idx::POP_FUNC_FIRST );
		if ( p_cur_first != head_.load( std::memory_order_acquire ) ) continue;

		hzrd_ptr_.regist_ptr_as_hazard_ptr( p_cur_last, (int)hazard_ptr_idx::POP_FUNC_LAST );
		if ( p_cur_last != tail_.load( std::memory_order_acquire ) ) continue;

		node_pointer p_cur_next = p_cur_first->get_next( next_slot_idx_ );
		hzrd_ptr_.regist_ptr_as_hazard_ptr( p_cur_next, (int)hazard_ptr_idx::POP_FUNC_NEXT );
		if ( p_cur_next != p_cur_first->get_next( next_slot_idx_ ) ) continue;

		if ( p_cur_first == p_cur_last ) {
			if ( p_cur_next == nullptr ) {
				// 番兵ノードしかないので、FIFOキューは空。
				return nullptr;
			} else {
				// 番兵ノードしかないように見えるが、Tailがまだ更新されていないだけ。
				// tailを更新して、pop処理をし直す。
				// ここで、プリエンプションして、head_がA->B->A'となった時、p_cur_nextが期待値とは異なるが、
				// ハザードポインタにA相当を確保しているので、A'は現れない。よって、このようなABA問題は起きない。
				tail_.compare_exchange_weak( p_cur_last, p_cur_next );
			}
		} else {
			if ( p_cur_next == nullptr ) {
				// headが他のスレッドでpopされた。
				continue;
			}
			// ここで、プリエンプションして、head_がA->B->A'となった時、p_cur_nextが期待値とは異なるが、
			// ハザードポインタにA相当を確保しているので、A'は現れない。よって、このようなABA問題は起きない。
			if ( head_.compare_exchange_weak( p_cur_first, p_cur_next ) ) {
				// ここで、firstの取り出しと所有権確保が完了
				// ただし、ハザードポインタをチェックしていないため、まだ参照している人がいるかもしれないため、
				// ハザードポインタチェックで存在しないこと確認したあと、戻すこと。
				return p_cur_first;
			}
		}
	}
	return nullptr;
}

bool fifo_free_nd_list::check_hazard_list( fifo_free_nd_list::node_pointer const p_chk_node )
{
	return hzrd_ptr_.check_ptr_in_hazard_list( p_chk_node );
}

////////////////////////////////////////////////////////////////////////////

free_nd_storage::free_nd_storage( void )
  : allocated_node_count_( 0 )
  , tls_fifo_( rcv_fifo_list_by_thread_terminating( this ) )
  , rcv_thread_local_fifo_list_()
{
}

free_nd_storage::~free_nd_storage()
{
	LogOutput( log_type::DEBUG, "Final: number of the allocated nodes -> %d", allocated_node_count_.load( std::memory_order_acquire ) );
}

bool free_nd_storage::recycle( free_nd_storage::node_pointer p_retire_node )
{
	thread_local_fifo_list* p_tls_fifo_ = check_local_storage();

	if ( p_retire_node != nullptr ) {
		p_tls_fifo_->push( p_retire_node );
	}

	if ( p_tls_fifo_->is_empty() ) {
		return false;
	}

	for ( int i = 0; i < 2; i++ ) {
		node_pointer p_chk = p_tls_fifo_->pop();
		if ( p_chk == nullptr ) break;
		if ( node_list_.check_hazard_list( p_chk ) ) {
			p_tls_fifo_->push( p_chk );   // 差し戻し
		} else {
			node_list_.push( p_chk );
		}
	}

	return true;
}

int free_nd_storage::get_allocated_num( void )
{
	return allocated_node_count_.load( std::memory_order_acquire );
}

void free_nd_storage::rcv_thread_local_fifo_list( thread_local_fifo_list* p_rcv )
{
	std::lock_guard<std::mutex> lock( mtx_rcv_thread_local_fifo_list_ );

	auto p_np = p_rcv->pop();

	while ( p_np != nullptr ) {
		rcv_thread_local_fifo_list_.push( p_np );
		p_np = p_rcv->pop();
	}

	return;
}

#ifdef USE_LOCK_FREE_MEM_ALLOC

static param_chunk_allocation param[] = {
	{ 32, 100 },
	{ 64, 100 },
	{ 128, 100 },
};

static general_mem_allocator& get_gma( void )
{
	static general_mem_allocator siglton( param, 3 );
	return siglton;
}

void* node_of_list::operator new( std::size_t n )   // usual new...(1)
{
	void* p_ans;
	p_ans = get_gma().allocate( n );

	if ( p_ans == nullptr ) throw std::bad_alloc();

	return p_ans;
}
void node_of_list::operator delete( void* p_mem ) noexcept   // usual new...(2)
{
	get_gma().deallocate( p_mem );
	return;
}

void* node_of_list::operator new[]( std::size_t n )   // usual new...(1)
{
	void* p_ans;
	p_ans = get_gma().allocate( n );

	if ( p_ans == nullptr ) throw std::bad_alloc();

	return p_ans;
}
void node_of_list::operator delete[]( void* p_mem ) noexcept   // usual new...(2)
{
	get_gma().deallocate( p_mem );
	return;
}

void* node_of_list::operator new( std::size_t n, void* p )   // placement new
{
	return p;
}
void node_of_list::operator delete( void* p, void* p2 ) noexcept   // placement delete...(3)
{
	return;
}

std::list<chunk_statistics> node_of_list::get_statistics( void )
{
	return get_gma().get_statistics();
}

#endif

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
