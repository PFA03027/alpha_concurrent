/*
 * hazard_ptr.hpp
 *
 *  Created on: 2020/12/24
 *      Author: alpha
 *
 *  US Patent of Hazard pointer algrithm: US20040107227A1 is now abandoned.
 *  https://patents.google.com/patent/US20040107227
 */

#ifndef SRC_HAZARD_PTR_HPP_
#define SRC_HAZARD_PTR_HPP_

#include <pthread.h>

#include <atomic>
#include <chrono>
#include <list>
#include <memory>
#include <mutex>
#include <thread>

#define USE_PTHREAD_THREAD_LOCAL_STORAGE   //!<	Use pthread thread local storage instead of thread_local

namespace alpha {
namespace concurrent {

namespace hazard_ptr_internal {

enum class ocupied_status {
	UNUSED,
	USING
};

template <typename T>
class hazard_node_glist;

/*!
 * @breif	削除予定のインスタンスへのポインタ集合のリスト
 *
 * @note
 * スレッドストレージに割り当てたいが、スレッドストレージに割り当てた場合、なぜかdestruct処理が正常に動作しない。
 * destructorが動作する時点で、割り当てられていたメモリエリアがすでに破棄されているように見える。
 *
 */
template <typename T>
class thd_local_pointer_list_for_delete {
public:
	thd_local_pointer_list_for_delete( hazard_node_glist<T>& r_hzd_node_glist )
	  : hzd_glist_ref_( r_hzd_node_glist )
	{
	}

	~thd_local_pointer_list_for_delete()
	{
		//		printf( "thd_local_pointer_list_for_delete's destructor: size: %lld\n", delete_candidate_list_.size() );
		//		fflush( NULL );
		if ( delete_candidate_list_.size() == 0 ) return;
		return;

		// 残っているポインタを削除する。
		// スレッドローカルストレージなので、削除されるのは、スレッドが終了するとき。
		// スレッド終了時点では、リアルタイム処理性等の制約からは解放されているとする。
		// 制約から解放されているので、スリープしつつも、残っているポインタのインスタンスを確実に解放する。
		for ( auto& e : delete_candidate_list_ ) {
			//			printf( "Checking, %p\n", e );
			int i;
			for ( i = 10; i > 0; i-- ) {
				if ( hzd_glist_ref_.scan( e ) ) {
					// まだ、ハザードポインタリストに存在したので、とりあえず、待つ。
					std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
				} else {
					delete e;
					break;
				}
			}
			if ( i == 0 ) {
				// 削除に失敗した。
				// TODO: 一旦、リークで放置。本当は、グローバルな何かに、移管するべき。。。
				printf( "Error: Fail to delete\n" );
			}
		}
	}

	void emplace_back( T* p_new_candidate )
	{
		delete_candidate_list_.emplace_back( p_new_candidate );
		//		printf( "added, %p\n", delete_candidate_list_.front() );
		return;
	}

	void try_delete_instance( void )
	{
		//		printf( "thd_local_pointer_list_for_delete.try_delete_instance: size: %lld\n", delete_candidate_list_.size() );

		for ( auto& e : delete_candidate_list_ ) {
			//			printf( "Checking in try, %p\n", e );
			//			fflush( NULL );
			if ( hzd_glist_ref_.scan( e ) ) {
				// ハザードポインタリストに対象ポインタが存在したので、次回の削除に回す。
				// nothing to do
				//				printf( "Skip try_delete_instance().\n" );
				//				fflush( NULL );
			} else {
				// ハザードポインタリストに対象ポインタが存在しなかったので、誰も参照していない。
				// よって、削除する。
				delete e;
				e = nullptr;
			}
		}
		delete_candidate_list_.remove( nullptr );

		//		printf( "thd_local_pointer_list_for_delete.try_delete_instance end: size: %lld\n", delete_candidate_list_.size() );
		return;
	}

private:
	std::list<T*>         delete_candidate_list_;
	hazard_node_glist<T>& hzd_glist_ref_;
};

/*!
 * @breif	ハザードポインタとしてキープするためのノード。
 *
 * スレッド毎に使用権が占有される。
 *
 * @note
 * このノードの集合を管理するリストは、ノード自体を削除することを行わない設計となっている。
 *
 */
template <typename T>
struct node_for_pointer {
	T*                                   p_target_ = nullptr;
	thd_local_pointer_list_for_delete<T> head_thread_local_retire_list_;

	node_for_pointer( hazard_node_glist<T>& r_hzd_node_glist )
	  : head_thread_local_retire_list_( r_hzd_node_glist )
	  , status_( ocupied_status::USING )
	  , next_( 0 )
	{
	}

	void set_hazard_ptr( T* p_target_arg )
	{
		p_target_ = p_target_arg;
		std::atomic_thread_fence( std::memory_order_release );
		return;
	}

	void clear_hazard_ptr( void )
	{
		p_target_ = nullptr;
		std::atomic_thread_fence( std::memory_order_release );
		return;
	}

	ocupied_status get_status( void )
	{
		return status_.load();
	}

	bool try_to_get_owner( void )
	{
		ocupied_status cur;
		cur = get_status();
		if ( cur != ocupied_status::UNUSED ) {
			return false;
		}

		return status_.compare_exchange_strong( cur, ocupied_status::USING );
	}

	void release_owner( void )
	{
		p_target_ = nullptr;
		std::atomic_thread_fence( std::memory_order_release );
		status_.store( ocupied_status::UNUSED );
		return;
	}

	node_for_pointer* get_next( void )
	{
		return next_.load();
	}

	void set_next( node_for_pointer* p_new_next )
	{
		next_.store( p_new_next );
		return;
	}

	bool next_CAS( node_for_pointer** pp_expect_ptr, node_for_pointer* p_desired_ptr )
	{
		return next_.compare_exchange_weak( *pp_expect_ptr, p_desired_ptr );
	}

private:
	std::atomic<ocupied_status>    status_;
	std::atomic<node_for_pointer*> next_;
};

/*!
 * @breif	ハザードポインタを保持するノード集合を管理するリスト
 *
 * グローバル変数として、静的に確保すること
 *
 * @note
 * このノードの集合を管理するリストは、ノード自体を削除することを行わず、再利用する設計ととしている。
 *
 */
template <typename T>
class hazard_node_glist {
public:
#ifdef USE_PTHREAD_THREAD_LOCAL_STORAGE
	pthread_key_t tls_key;   //!<	key for thread local storage of POSIX.
#endif

	static hazard_node_glist& get_instance( void );

	~hazard_node_glist()
	{
		node_for_pointer<T>* p_cur = head_node_.get_next();
		while ( p_cur != nullptr ) {
			node_for_pointer<T>* p_nxt = p_cur->get_next();
			delete p_cur;
			p_cur = p_nxt;
		}
	}

	node_for_pointer<T>* request_hazard_ptr_node( void )
	{
		// 空きノードを探す。
		node_for_pointer<T>* p_ans = head_node_.get_next();
		while ( p_ans != nullptr ) {
			if ( p_ans->get_status() == ocupied_status::UNUSED ) {
				if ( p_ans->try_to_get_owner() ) {
					return p_ans;
				}
			}
			p_ans = p_ans->get_next();
		}

		// 空きノードが見つからなかったので、新しいノードを用意する。
		p_ans = new node_for_pointer<T>( *this );
		node_for_pointer<T>* p_next_check;
		p_next_check = head_node_.get_next();
		while ( true ) {
			p_ans->set_next( p_next_check );
			if ( head_node_.next_CAS( &p_next_check, p_ans ) ) {
				break;
			}
		}
		glist_count_++;

		//		printf( "glist is added.\n" );
		return p_ans;
	}

	bool scan( T* p_chk )
	{
		bool                 ans        = false;
		node_for_pointer<T>* p_cur_node = head_node_.get_next();
		while ( p_cur_node != nullptr ) {
			std::atomic_thread_fence( std::memory_order_acquire );
			if ( p_cur_node->p_target_ == p_chk ) {
				ans = true;
				break;
			}
			p_cur_node = p_cur_node->get_next();
		}
		return ans;
	}

	int debug_get_glist_size( void )
	{
		return glist_count_;
	}

private:
#ifdef USE_PTHREAD_THREAD_LOCAL_STORAGE
	static void destr_fn( void* parm )
	{
		//		printf( "thread local destructor now being called -- " );

		node_for_pointer<T>* p_target_node = reinterpret_cast<node_for_pointer<T>*>( parm );

		p_target_node->head_thread_local_retire_list_.try_delete_instance();
		p_target_node->release_owner();

		//		printf( "thread local destructor is done.\n" );
		return;
	}
#endif

	hazard_node_glist( void )
	  : head_node_( *this )
	{
#ifdef USE_PTHREAD_THREAD_LOCAL_STORAGE
		int status = pthread_key_create( &tls_key, destr_fn );
		if ( status < 0 ) {
			printf( "pthread_key_create failed, errno=%d", errno );
			exit( 1 );
		}
#endif
	}

	node_for_pointer<T> head_node_;
	int                 glist_count_ = 0;
};

template <typename T>
hazard_node_glist<T>& hazard_node_glist<T>::get_instance( void )
{
	static hazard_node_glist<T> singleton;   //!<	singleton instance and delay evaluation is expected.

	return singleton;
}

}   // namespace hazard_ptr_internal

/*!
 * @breif	hazard pointer support class
 *
 * 削除権が、１つのインスタンスのみに制約できる場合に使用できるハザードポインタ。
 * 参照権に対しては、制限なし。ただし、参照が終了した場合は、ハザードポインタをクリアしなければならない。
 * 参照権のクリアには、clear_hazard_ptr()を使用する。
 *
 * ハザードポインタとして登録したポインタが指し示すオブジェクトは、delete演算子によって削除される。
 * ただし、delete演算子による削除は非同期で行われるため、try_delete_instance()を呼び出したとしても、その時点でdelete演算子による削除が行われるわけではない。
 * そのため、デストラクタによるリソース開放を行うクラスがテンプレートパラメータとして指定される場合、リソース開放が遅延されても、新たなリソース確保側と衝突を起こさない構成である必要がある。
 *
 * （ハザードポインタ自身はlock-freeであるが）lock-freeとして動作するには、テンプレートパラメータで指定されたクラスのデストラクタがlock-freeでなければならない。
 *
 */
template <typename T>
class hazard_ptr {
public:
	hazard_ptr( void )
	{
		check_local_storage();
		//		p_hzd_ptr_node_ = hazard_ptr_internal::hazard_node_glist<T>::get_instance().request_hazard_ptr_node();
	}

	~hazard_ptr( void )
	{
		check_local_storage();

		p_hzd_ptr_node_->head_thread_local_retire_list_.try_delete_instance();
		p_hzd_ptr_node_->release_owner();
		p_hzd_ptr_node_ = nullptr;

#ifdef USE_PTHREAD_THREAD_LOCAL_STORAGE
		int status;
		status = pthread_setspecific( hazard_ptr_internal::hazard_node_glist<T>::get_instance().tls_key, (void*)nullptr );
		if ( status < 0 ) {
			printf( "pthread_setspecific failed, errno %d", errno );
		}
#endif
	}

	/*!
	 * @breif	Make a reservation to acquire the reference right.
	 *
	 * 参照権を獲得予約をするために、ハザードポインタに登録する。
	 *
	 * 本I/Fを呼び出しただけでは、参照権を獲得したことにはならない。 @n
	 * 本I/Fを呼び出した後に、登録したポインタがまだ有効であるかどうかを確認すること。 @n
	 * @li	確認の結果、有効である場合は、参照権を獲得したとみなせる。
	 * @li	確認の結果、有効ではなかった場合、clear_hazard_ptr()を呼び出すか、新しいポインタを新たなハザードポインタとして本I/Fで再設定し、現在保持しているハザードポインタを解放すること。
	 *
	 * @note
	 * 確実なclear_hazard_ptr()呼び出しをサポートするクラスとして、 hazard_ptr_scoped_ref<T> を用意している。
	 */
	inline void regist_ptr_as_hazard_ptr( T* p_target )
	{
		check_local_storage();

		p_hzd_ptr_node_->p_target_ = p_target;
		std::atomic_thread_fence( std::memory_order_release );
	}

	/*!
	 * @breif	Release a reservation to acquire the reference right or the reference right itself.
	 *
	 * @note
	 * 確実なclear_hazard_ptr()呼び出しをサポートするクラスとして、 hazard_ptr_scoped_ref<T> を用意している。
	 */
	inline void clear_hazard_ptr( void )
	{
		check_local_storage();

		p_hzd_ptr_node_->p_target_ = nullptr;
		std::atomic_thread_fence( std::memory_order_release );
	}

	/*!
	 * @breif	hazard pointer support class
	 *
	 * 現在設定されているハザードポインタに対し、参照権を獲得している状態から削除権を獲得した状態へ変更するとともに、
	 * 削除が可能であれば、削除を行う。
	 */
	void try_delete_instance( void )
	{
		check_local_storage();

		std::atomic_thread_fence( std::memory_order_acquire );
		T* p_try_delete            = p_hzd_ptr_node_->p_target_;
		p_hzd_ptr_node_->p_target_ = nullptr;
		std::atomic_thread_fence( std::memory_order_release );

		if ( p_try_delete != nullptr ) {
			p_hzd_ptr_node_->head_thread_local_retire_list_.emplace_back( p_try_delete );
		}
		p_hzd_ptr_node_->head_thread_local_retire_list_.try_delete_instance();

		return;
	}

	static int debug_get_glist_size( void )
	{
		return hazard_ptr_internal::hazard_node_glist<T>::get_instance().debug_get_glist_size();
	}

private:
	void check_local_storage( void )
	{
		if ( p_hzd_ptr_node_ == nullptr ) {
			p_hzd_ptr_node_ = hazard_ptr_internal::hazard_node_glist<T>::get_instance().request_hazard_ptr_node();

#ifdef USE_PTHREAD_THREAD_LOCAL_STORAGE
			int status;
			status = pthread_setspecific( hazard_ptr_internal::hazard_node_glist<T>::get_instance().tls_key, (void*)p_hzd_ptr_node_ );
			if ( status < 0 ) {
				printf( "pthread_setspecific failed, errno %d", errno );
				pthread_exit( (void*)1 );
			}
#else
			dest_inst_.pp_hzd_ptr_node_ = &p_hzd_ptr_node_;
#endif
		}
	}

#ifdef USE_PTHREAD_THREAD_LOCAL_STORAGE
	static __thread hazard_ptr_internal::node_for_pointer<T>* p_hzd_ptr_node_;
	// C++11で、thread_localが使用可能となるが、g++でコンパイルした場合、スレッド終了時のdestructor処理実行時に、
	// すでにメモリ領域が破壊されている場合があるため、destructor処理の正常動作が期待できない。
	// そのため、その点を明示する意図として __thread を使用する。
	// また、 __thread の変数は、暗黙的にゼロ初期化されていることを前提としている。
#else
	static thread_local hazard_ptr_internal::node_for_pointer<T>* p_hzd_ptr_node_;

	struct destructor_func {
		hazard_ptr_internal::node_for_pointer<T>** pp_hzd_ptr_node_ = nullptr;

		~destructor_func()
		{
			//			printf( "thread local destructor now being called -- " );

			if ( pp_hzd_ptr_node_ == nullptr ) return;
			if ( *pp_hzd_ptr_node_ == nullptr ) return;

			node_for_pointer<T>* p_target_node = reinterpret_cast<node_for_pointer<T>*>( parm );

			( *pp_hzd_ptr_node_ )->head_thread_local_retire_list_.try_delete_instance();
			( *pp_hzd_ptr_node_ )->release_owner();

			//			printf( "thread local destructor is done.\n" );
			return;
		}

		static thread_local destructor_func dest_inst_;
	};
#endif
};

#ifdef USE_PTHREAD_THREAD_LOCAL_STORAGE
template <typename T>
__thread hazard_ptr_internal::node_for_pointer<T>* hazard_ptr<T>::p_hzd_ptr_node_;
#else
template <typename T>
thread_local hazard_ptr_internal::node_for_pointer<T>* hazard_ptr<T>::p_hzd_ptr_node_ = nullptr;
template <typename T>
thread_local hazard_ptr<T>::destructor_func hazard_ptr<T>::dest_inst_;
#endif

/*!
 * @breif	scoped reference control support class for hazard_ptr
 *
 * スコープベースでの、参照権の解放制御をサポートするクラス。
 * 削除権を確保する場合は、スコープアウトする前に、try_delete_instance()を呼び出すこと。
 */
template <typename T>
class hazard_ptr_scoped_ref {
public:
	hazard_ptr_scoped_ref( hazard_ptr<T>& ref )
	  : monitor_ref_( ref )
	{
	}

	~hazard_ptr_scoped_ref()
	{
		monitor_ref_.clear_hazard_ptr();
	}

private:
	hazard_ptr<T>& monitor_ref_;
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_HAZARD_PTR_HPP_ */
