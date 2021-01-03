/*
 * lf_fifo.hpp
 *
 * lock free fifo
 *
 *  Created on: 2020/12/31
 *      Author: alpha
 */

#ifndef SRC_LF_FIFO_HPP_
#define SRC_LF_FIFO_HPP_

#include <atomic>
#include <memory>
#include <tuple>

#include "hazard_ptr.hpp"

namespace alpha {
namespace concurrent {

namespace internal {
/*!
 * @breif	T型に対するFIFOキュー内のリスト構造に使用するノード
 *
 * Tは、trivially copyableでなければならい。
 */
template <typename T, int NEXT_SLOTS = 3>
struct node_of_list {

	node_of_list( void )
	  : target_()
	{
		for ( auto& e : next_ ) {
			e.store( nullptr );
		}
	}

	node_of_list( const T& cont_arg )
	  : target_( cont_arg )
	{
		for ( auto& e : next_ ) {
			e.store( nullptr );
		}
	}

	node_of_list( T&& cont_arg )
	  : target_( std::move( cont_arg ) )
	{
		for ( auto& e : next_ ) {
			e.store( nullptr );
		}
	}

	~node_of_list()
	{
		return;
	}

	T get_value( void ) const
	{
		std::atomic_thread_fence( std::memory_order_acquire );
		return target_;
	}

	void set_value( const T& value_arg )
	{
		target_ = value_arg;
		std::atomic_thread_fence( std::memory_order_release );
	}

	node_of_list* get_next( int cur_slot_idx )
	{
		std::atomic_thread_fence( std::memory_order_acquire );
		return next_[cur_slot_idx].load();
	}

	void set_next( node_of_list* p_new_next, int cur_slot_idx )
	{
		std::atomic_thread_fence( std::memory_order_acquire );
		next_[cur_slot_idx].store( p_new_next );
		return;
	}

	bool next_CAS( node_of_list** pp_expect_ptr, node_of_list* p_desired_ptr, int cur_slot_idx )
	{
		std::atomic_thread_fence( std::memory_order_acquire );
		return next_[cur_slot_idx].compare_exchange_weak( *pp_expect_ptr, p_desired_ptr );
	}

private:
	T                          target_;
	std::atomic<node_of_list*> next_[NEXT_SLOTS];
};

/*!
 * @breif	フリーノードを管理するクラスで使用するスレッドローカルストレージのリスト型のFIFOキュークラス
 */
template <typename T, int NEXT_SLOTS_IDX = 1, int NEXT_SLOTS = 3>
class thread_local_fifo_list {
public:
	using value_type   = T;
	using node_type    = node_of_list<T, NEXT_SLOTS>;
	using node_pointer = node_of_list<T, NEXT_SLOTS>*;

	thread_local_fifo_list( void )
	  : head_( nullptr )
	  , tail_( nullptr )
	{
		node_pointer p_initial_node = new node_type();
		head_                       = p_initial_node;
		tail_                       = p_initial_node;

		return;
	}

	~thread_local_fifo_list()
	{
		node_pointer p_cur = head_;

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

	void push( node_pointer const p_push_node )
	{
		p_push_node->set_next( nullptr, next_slot_idx_ );

		tail_->set_next( p_push_node, next_slot_idx_ );
		tail_ = p_push_node;

		return;
	}

	node_pointer pop( void )
	{
		node_pointer p_ans = head_;

		if ( head_ == tail_ ) {
			// 番兵ノードしかないので、FIFOキューは空。
			return nullptr;
		}

		node_pointer p_cur_next = head_->get_next( next_slot_idx_ );
		head_                   = p_cur_next;

		return p_ans;
	}

private:
	static constexpr int next_slot_idx_ = NEXT_SLOTS_IDX;

	node_pointer head_;
	node_pointer tail_;
};

/*!
 * @breif	フリーノードを管理するクラスの基本となるリスト型のFIFOキュークラス
 *
 * Tは、trivially copyableでなければならい。
 *
 * @note
 * https://www.slideshare.net/kumagi/lock-free-safe?next_slideshow=1 @n
 * 明記はされていないが、必ずノードが１つ残る構造。
 */
template <typename T, int NEXT_SLOTS_IDX = 0, int NEXT_SLOTS = 3>
class fifo_free_nd_list {
public:
	static constexpr int hzrd_max_slot_ = 5;
	using value_type                    = T;
	using node_type                     = node_of_list<T, NEXT_SLOTS>;
	using node_pointer                  = node_of_list<T, NEXT_SLOTS>*;

	fifo_free_nd_list( void )
	  : next_slot_idx_( 0 )
	  , head_( nullptr )
	  , tail_( nullptr )
	{
		node_pointer p_initial_node = new node_type();
		head_.store( p_initial_node );
		tail_.store( p_initial_node );

		return;
	}

	~fifo_free_nd_list()
	{
		node_pointer p_cur = head_.load();
		head_.store( nullptr );
		tail_.store( nullptr );

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

	void push( node_pointer const p_push_node )
	{
		p_push_node->set_next( nullptr, next_slot_idx_ );

		scoped_hazard_ref scoped_ref_cur( hzrd_ptr_, (int)hazard_ptr_idx::PUSH_FUNC_LAST );
		scoped_hazard_ref scoped_ref_nxt( hzrd_ptr_, (int)hazard_ptr_idx::PUSH_FUNC_NEXT );

		while ( true ) {
			node_pointer p_cur_last = tail_.load();

			hzrd_ptr_.regist_ptr_as_hazard_ptr( p_cur_last, (int)hazard_ptr_idx::PUSH_FUNC_LAST );

			if ( p_cur_last != tail_.load() ) continue;

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
	 * 先頭ノードのポインタに対し、引数の pred が真を返すノードの場合にFIFOキューから取り出す。
	 * pred がfalseとなった場合、nullptrを返す。
	 *
	 * @return	1st: 取り出された管理ノードへのポインタ。nullptrの場合、FIFOキューが空だったことを示す。管理対象ポインタは、
	 * @return	2nd: 取り出された管理ノードへのポインタが有効の場合、FIFOから取り出された値
	 *
	 * @warning
	 * 管理ノードは、使用中の可能性がある。ハザードポインタに登録されていないことの確認が完了するまで、管理ノードの破棄や、APIを呼び出しで、値を変更してはならない。
	 */
	node_pointer pop( void )
	{
		scoped_hazard_ref scoped_ref_first( hzrd_ptr_, (int)hazard_ptr_idx::POP_FUNC_FIRST );
		scoped_hazard_ref scoped_ref_last( hzrd_ptr_, (int)hazard_ptr_idx::POP_FUNC_LAST );
		scoped_hazard_ref scoped_ref_next( hzrd_ptr_, (int)hazard_ptr_idx::POP_FUNC_NEXT );

		while ( true ) {
			node_pointer p_cur_first = head_.load();
			node_pointer p_cur_last  = tail_.load();

			hzrd_ptr_.regist_ptr_as_hazard_ptr( p_cur_first, (int)hazard_ptr_idx::POP_FUNC_FIRST );
			if ( p_cur_first != head_.load() ) continue;

			hzrd_ptr_.regist_ptr_as_hazard_ptr( p_cur_last, (int)hazard_ptr_idx::POP_FUNC_LAST );
			if ( p_cur_last != tail_.load() ) continue;

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

	bool check_hazard_list( node_pointer const p_chk_node )
	{
		return hzrd_ptr_.check_ptr_in_hazard_list( p_chk_node );
	}

private:
	using hazard_ptr_storage = hazard_ptr<node_of_list<T, NEXT_SLOTS>, hzrd_max_slot_, true>;
	using scoped_hazard_ref  = hazard_ptr_scoped_ref<node_of_list<T, NEXT_SLOTS>, hzrd_max_slot_, true>;

	enum class hazard_ptr_idx : int {
		PUSH_FUNC_LAST = 0,
		PUSH_FUNC_NEXT = 1,
		POP_FUNC_FIRST = 2,
		POP_FUNC_LAST  = 3,
		POP_FUNC_NEXT  = 4
	};

	int                       next_slot_idx_;
	std::atomic<node_pointer> head_;
	std::atomic<node_pointer> tail_;

	hazard_ptr_storage hzrd_ptr_;
};

/*!
 * @breif	フリーノードを管理するクラス
 *
 * Tは、trivially copyableでなければならい。
 *
 * node_of_list クラスは、nextスロットを NEXT_SLOT_LINES+1 保持し、
 * クライアント側は、nextスロットのインデックス0を使用することができる。
 * このクラス内ではインデックス1以降を使用する。
 *
 * @note
 * freeノードを管理するラインを２つ以上保持し、一つ以上のラインに対するハザードポインタ登録がないものが次に使用できるフリーノードとなる。
 * そのようなフリーノードがあれば、それを提供する。
 * そのようなフリーノードない場合、あるいはフリーノード自体が枯渇している場合、
 * ヒープからメモリをallocate()する。この時、ヒープメモリからのallocate中にロックが発生する。
 *
 * ヒープメモリからのallocateをなるべく回避するには、以下の２つの調整を行うこと。
 * @li	ハザードポインタによる衝突を回避するためにライン数を増やす。同時アクセス数が多い場合に効果がある。
 * @li	フリーノード自体の枯渇を回避するために、初期フリーノード生成数を増やす。同時アクセス数が多く、ノードの使用時間が長い場合に効果がある。
 *
 */
template <typename T, int NEXT_SLOT_LINES = 3>
class free_nd_storage {
public:
	static constexpr int next_slot_lines_ = NEXT_SLOT_LINES;
	using value_type                      = T;
	using node_type                       = node_of_list<T, NEXT_SLOT_LINES>;
	using node_pointer                    = node_of_list<T, NEXT_SLOT_LINES>*;

	free_nd_storage( void )
	  : allocated_node_count_( 0 )
	{
		int status = pthread_key_create( &tls_key, destr_fn );
		if ( status < 0 ) {
			printf( "pthread_key_create failed, errno=%d", errno );
			exit( 1 );
		}
		check_local_storage();
	}

	~free_nd_storage()
	{
		printf( "Final: new node is allocated -> %d\n", allocated_node_count_.load() );
	}

	void recycle( node_pointer p_retire_node )
	{
		thread_local_fifo_list<T>* p_tls_fifo_ = check_local_storage();

		if ( p_retire_node == nullptr ) return;

		p_tls_fifo_->push( p_retire_node );

		for ( int i = 0; i < num_recycle_exec; i++ ) {
			node_pointer p_chk = p_tls_fifo_->pop();
			if ( p_chk == nullptr ) break;
			if ( node_list_.check_hazard_list( p_chk ) ) {
				p_tls_fifo_->push( p_chk );   // 差し戻し
			} else {
				node_list_.push( p_chk );
			}
		}

		return;
	}

	/*!
	 * @breif	フリーノードを取り出す。
	 *
	 * フリーノードがなければ、ヒープからメモリをallocate()する。
	 *
	 * pred は、引数で指定されたノードポインタに対し、呼び出し側から見た使用可否を判定する関数オブジェクト
	 * @li	true 使用可能
	 * @li	false 使用不可能
	 *
	 * pred が、trueを返したフリーノードをallocateするか、見つからなければ、ヒープメモリから新しいノードを確保する。
	 *
	 * predは、下記の呼び出しができること
	 * @li	bool pred(node_pointer p_chk_node)
	 *
	 */
	template <typename TFUNC>
	node_pointer allocate( TFUNC pred )
	{
		node_pointer p_ans = node_list_.pop();

		if ( p_ans != nullptr ) {
			if ( pred( p_ans ) ) {
				return p_ans;
			}
		}

		// ここに来たら、利用可能なfree nodeがなかったこと示すので、新しいノードをヒープから確保する。
		allocated_node_count_++;
		return new node_type();
	}

	int get_allocated_num( void )
	{
		return allocated_node_count_.load();
	}

private:
	static void destr_fn( void* parm )
	{
		//			printf( "thread local destructor now being called -- " );

		if ( parm == nullptr ) return;   // なぜかnullptrで呼び出された。多分pthread内でのrace conditionのせい。

		thread_local_fifo_list<T>* p_target_node = reinterpret_cast<thread_local_fifo_list<T>*>( parm );

		delete p_target_node;

		//			printf( "thread local destructor is done.\n" );
		return;
	}

	void allocate_local_storage( void )
	{
		p_tls_fifo__ = new thread_local_fifo_list<T>();

		int status;
		status = pthread_setspecific( tls_key, (void*)p_tls_fifo__ );
		if ( status < 0 ) {
			printf( "pthread_setspecific failed, errno %d", errno );
			pthread_exit( (void*)1 );
		}
		//		printf( "pthread_setspecific set pointer to tls.\n" );
	}

	inline thread_local_fifo_list<T>* check_local_storage( void )
	{
		thread_local_fifo_list<T>* p_ans = p_tls_fifo__;
		if ( p_ans == nullptr ) {
			allocate_local_storage();
			p_ans = p_tls_fifo__;
		}
		return p_ans;
	}

	static constexpr int num_recycle_exec = 2;   //!< recycle処理を行うノード数。処理量が一定化するために、ループ回数を定数化する。２以上とする事。

	std::atomic<int> allocated_node_count_;

	fifo_free_nd_list<T> node_list_;

	static thread_local thread_local_fifo_list<T>* p_tls_fifo__;

	pthread_key_t tls_key;   //!<	key for thread local storage of POSIX.

	//	friend pthread_key_create;
};

template <typename T, int NEXT_SLOT_LINES>
thread_local thread_local_fifo_list<T>* free_nd_storage<T, NEXT_SLOT_LINES>::p_tls_fifo__;

/*!
 * @breif	リスト型のFIFOキューの基本要素となるFIFOキュークラス
 *
 * Tは、trivially copyableでなければならい。
 *
 * @note
 * https://www.slideshare.net/kumagi/lock-free-safe?next_slideshow=1 @n
 * 明記はされていないが、必ずノードが１つ残る構造。
 *
 * キューに登録された値そのものは、head_.nextが指すノードに存在する。
 */
template <typename T, int NEXT_SLOT_IDX, int NEXT_SLOTS = 2>
class fifo_nd_list {
public:
	static constexpr int next_slot_idx_ = NEXT_SLOT_IDX;
	static constexpr int hzrd_max_slot_ = 5;
	using value_type                    = T;
	using node_type                     = node_of_list<T, NEXT_SLOTS>;
	using node_pointer                  = node_of_list<T, NEXT_SLOTS>*;
	using hazard_ptr_storage            = hazard_ptr<node_of_list<T, NEXT_SLOTS>, hzrd_max_slot_, true>;

	fifo_nd_list( void )
	  : head_( nullptr )
	  , tail_( nullptr )
	  , size_count_( 0 )
	{
		node_pointer p_initial_node = new node_type();
		head_.store( p_initial_node );
		tail_.store( p_initial_node );

		return;
	}

	~fifo_nd_list()
	{
		node_pointer p_cur = head_.load();
		head_.store( nullptr );
		tail_.store( nullptr );

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

	void push( node_pointer const p_push_node )
	{
		p_push_node->set_next( nullptr, next_slot_idx_ );

		scoped_hazard_ref scoped_ref_cur( hzrd_ptr_, (int)hazard_ptr_idx::PUSH_FUNC_LAST );
		scoped_hazard_ref scoped_ref_nxt( hzrd_ptr_, (int)hazard_ptr_idx::PUSH_FUNC_NEXT );

		while ( true ) {
			node_pointer p_cur_last = tail_.load();
			hzrd_ptr_.regist_ptr_as_hazard_ptr( p_cur_last, (int)hazard_ptr_idx::PUSH_FUNC_LAST );
			if ( p_cur_last != tail_.load() ) continue;

			node_pointer p_cur_next = p_cur_last->get_next( next_slot_idx_ );
			hzrd_ptr_.regist_ptr_as_hazard_ptr( p_cur_next, (int)hazard_ptr_idx::PUSH_FUNC_NEXT );
			if ( p_cur_next != p_cur_last->get_next( next_slot_idx_ ) ) continue;

			if ( p_cur_next == nullptr ) {
				if ( p_cur_last->next_CAS( &p_cur_next, p_push_node, next_slot_idx_ ) ) {
					tail_.compare_exchange_weak( p_cur_last, p_push_node );
					size_count_++;
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
	 * @breif	FIFOキューからポインタを取り出す。
	 *
	 * 返り値の管理ノードに保管されている。
	 *
	 * @return	1st: 取り出された管理ノードへのポインタ。nullptrの場合、FIFOキューが空だったことを示す。管理対象ポインタは、
	 * @return	2nd: 取り出された管理ノードへのポインタが有効の場合、FIFOから取り出された値
	 *
	 * @warning
	 * 管理ノードは、使用中の可能性がある。ハザードポインタに登録されていないことの確認が完了するまで、管理ノードの破棄や、APIを呼び出しで、値を変更してはならない。
	 */
	std::tuple<node_pointer, value_type> pop( void )
	{
		scoped_hazard_ref scoped_ref_first( hzrd_ptr_, (int)hazard_ptr_idx::POP_FUNC_FIRST );
		scoped_hazard_ref scoped_ref_last( hzrd_ptr_, (int)hazard_ptr_idx::POP_FUNC_LAST );
		scoped_hazard_ref scoped_ref_next( hzrd_ptr_, (int)hazard_ptr_idx::POP_FUNC_NEXT );

		while ( true ) {
			node_pointer p_cur_first = head_.load();
			node_pointer p_cur_last  = tail_.load();

			hzrd_ptr_.regist_ptr_as_hazard_ptr( p_cur_first, (int)hazard_ptr_idx::POP_FUNC_FIRST );
			if ( p_cur_first != head_.load() ) continue;

			hzrd_ptr_.regist_ptr_as_hazard_ptr( p_cur_last, (int)hazard_ptr_idx::POP_FUNC_LAST );
			if ( p_cur_last != tail_.load() ) continue;

			node_pointer p_cur_next = p_cur_first->get_next( next_slot_idx_ );
			hzrd_ptr_.regist_ptr_as_hazard_ptr( p_cur_next, (int)hazard_ptr_idx::POP_FUNC_NEXT );
			if ( p_cur_next != p_cur_first->get_next( next_slot_idx_ ) ) continue;

			if ( p_cur_first == p_cur_last ) {
				if ( p_cur_next == nullptr ) {
					// 番兵ノードしかないので、FIFOキューは空。
					return std::tuple<node_pointer, value_type>( nullptr, value_type() );
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
				T ans_2nd = p_cur_next->get_value();   // この処理が必要になるため、T型は、trivially copyableでなければならい。
				// ここで、プリエンプションして、head_がA->B->A'となった時、p_cur_nextが期待値とは異なるが、
				// ハザードポインタにA相当を確保しているので、A'は現れない。よって、このようなABA問題は起きない。
				if ( head_.compare_exchange_weak( p_cur_first, p_cur_next ) ) {
					size_count_--;
					// ここで、firstの取り出しと所有権確保が完了
					// ただし、ハザードポインタをチェックしていないため、まだ参照している人がいるかもしれない。
					return std::tuple<node_pointer, value_type>( p_cur_first, ans_2nd );
				}
			}
		}
		return std::tuple<node_pointer, value_type>( nullptr, value_type() );
	}

	/*!
	 * @breif	Check whether a pointer is in this hazard list
	 *
	 * @retval	true	p_chk_ptr is in this hazard list.
	 * @retval	false	p_chk_ptr is not in this hazard list.
	 */
	bool check_hazard_list( node_pointer const p_chk_node )
	{
		return hzrd_ptr_.check_ptr_in_hazard_list( p_chk_node );
	}

	int get_size( void )
	{
		return size_count_.load();
	}

private:
	using scoped_hazard_ref = hazard_ptr_scoped_ref<node_of_list<T, NEXT_SLOTS>, hzrd_max_slot_, true>;

	enum class hazard_ptr_idx : int {
		PUSH_FUNC_LAST = 0,
		PUSH_FUNC_NEXT = 1,
		POP_FUNC_FIRST = 2,
		POP_FUNC_LAST  = 3,
		POP_FUNC_NEXT  = 4
	};

	std::atomic<node_pointer> head_;
	std::atomic<node_pointer> tail_;
	std::atomic<int>          size_count_;

	hazard_ptr_storage hzrd_ptr_;
};

}   // namespace internal

/*!
 * @breif	リスト型のFIFOキュー
 *
 * Tは、trivially copyableでなければならい。
 *
 * @note
 */
template <typename T>
class fifo_list {
public:
	using value_type = T;

	fifo_list( void )
	{
		return;
	}

	void push( const T& cont_arg )
	{
		node_pointer p_new_node = free_nd_.allocate(
			[this]( node_pointer p_chk_node ) {
				return !( this->fifo_.check_hazard_list( p_chk_node ) );
				//				return false;
			} );

		p_new_node->set_value( cont_arg );

		fifo_.push( p_new_node );
		return;
	}

	std::tuple<bool, value_type> pop( void )
	{
		auto [p_poped_node, ans_value] = fifo_.pop();

		if ( p_poped_node == nullptr ) return std::tuple<bool, value_type>( false, ans_value );

		free_nd_.recycle( p_poped_node );

		return std::tuple<bool, value_type>( true, ans_value );
	}

	int get_size( void )
	{
		return fifo_.get_size();
	}

	int get_allocated_num( void )
	{
		return free_nd_.get_allocated_num();
	}


private:
	using free_nd_storage_type              = internal::free_nd_storage<T>;
	static constexpr int num_of_slot_lines_ = free_nd_storage_type::next_slot_lines_;
	using fifo_type                         = internal::fifo_nd_list<T, 2, num_of_slot_lines_>;
	using node_type                         = typename free_nd_storage_type::node_type;
	using node_pointer                      = typename free_nd_storage_type::node_pointer;

	fifo_type            fifo_;
	free_nd_storage_type free_nd_;
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_LF_FIFO_HPP_ */
