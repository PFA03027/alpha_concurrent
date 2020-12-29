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
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>

#define USE_PTHREAD_THREAD_LOCAL_STORAGE   //!<	Use pthread thread local storage instead of thread_local

namespace alpha {
namespace concurrent {

namespace hazard_ptr_internal {

enum class ocupied_status {
	UNUSED,
	USING
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
struct node_for_hazard_ptr {
	T* p_target_ = nullptr;

	node_for_hazard_ptr( void )
	  : status_( ocupied_status::USING )
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

	node_for_hazard_ptr* get_next( void )
	{
		return next_.load();
	}

	void set_next( node_for_hazard_ptr* p_new_next )
	{
		next_.store( p_new_next );
		return;
	}

	bool next_CAS( node_for_hazard_ptr** pp_expect_ptr, node_for_hazard_ptr* p_desired_ptr )
	{
		return next_.compare_exchange_weak( *pp_expect_ptr, p_desired_ptr );
	}

private:
	std::atomic<ocupied_status>       status_;
	std::atomic<node_for_hazard_ptr*> next_;
};

/*!
 * @breif	削除予定のポインタをキープするためのノード。
 *
 * 追加するのは、各スレッド。
 * 実際に削除するのは、削除専用スレッド
 *
 */
template <typename T>
struct node_for_delete_ptr {

	node_for_delete_ptr( T* p_delete_ptr_arg = nullptr )
	  : p_target_delete_( p_delete_ptr_arg )
	  , next_( nullptr )
	{
	}

	~node_for_delete_ptr()
	{
		delete p_target_delete_.load();
		return;
	}

	void clear_delete_ptr( void )
	{
		p_target_delete_.store( nullptr );
		return;
	}

	T* get_delete_ptr( void )
	{
		return p_target_delete_.load();
	}

	bool is_emptry( void )
	{
		return p_target_delete_.load() == nullptr;
	}

	bool try_to_set_delete_ptr( T* p_delete_ptr_arg )
	{
		T* p_cur = p_target_delete_.load();

		if ( p_cur != nullptr ) return false;

		return p_target_delete_.compare_exchange_strong( p_cur, p_delete_ptr_arg );
	}

	// 登録されているポインタに対して、削除を行う。呼び出し前にハザードポインタに存在しないことを確認すること。
	void do_delete_ptr( void )
	{
		T* p_del = p_target_delete_.load();
		delete p_del;
		p_target_delete_.store( nullptr );

		return;
	}

	node_for_delete_ptr* get_next( void )
	{
		return next_.load();
	}

	void set_next( node_for_delete_ptr* p_new_next )
	{
		next_.store( p_new_next );
		return;
	}

	bool next_CAS( node_for_delete_ptr** pp_expect_ptr, node_for_delete_ptr* p_desired_ptr )
	{
		return next_.compare_exchange_weak( *pp_expect_ptr, p_desired_ptr );
	}

private:
	std::atomic<T*>                   p_target_delete_;
	std::atomic<node_for_delete_ptr*> next_;
};

class Garbage_collector;

class hazard_node_glist_base {
public:
	hazard_node_glist_base( void );

	virtual ~hazard_node_glist_base();

protected:
	// ハザードポインタに存在するかどうかを確認してから削除を行う。
	virtual void try_clean_up_delete_ptr( void ) = 0;

	void post_trigger_gc( void );

	void deregist_self_from_list( void );

private:
	void regist_self_to_list( void );
	void add_one_new_glist_node( void );

	friend Garbage_collector;
	static node_for_delete_ptr<hazard_node_glist_base> head_node_glist_;
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
class hazard_node_glist : public hazard_node_glist_base {
public:
#ifdef USE_PTHREAD_THREAD_LOCAL_STORAGE
	pthread_key_t tls_key;   //!<	key for thread local storage of POSIX.
#endif

	static hazard_node_glist& get_instance( void );

	~hazard_node_glist()
	{
		{
			node_for_hazard_ptr<T>* p_cur = hazard_ptr_head_node_.get_next();
			while ( p_cur != nullptr ) {
				node_for_hazard_ptr<T>* p_nxt = p_cur->get_next();
				delete p_cur;
				p_cur = p_nxt;
			}
		}

		{
			node_for_delete_ptr<T>* p_cur = delete_ptr_head_node_.get_next();
			while ( p_cur != nullptr ) {
				node_for_delete_ptr<T>* p_nxt = p_cur->get_next();
				delete p_cur;
				p_cur = p_nxt;
			}
		}
	}

	node_for_hazard_ptr<T>* request_hazard_ptr_node( void )
	{
		// 空きノードを探す。
		node_for_hazard_ptr<T>* p_ans = hazard_ptr_head_node_.get_next();
		while ( p_ans != nullptr ) {
			if ( p_ans->get_status() == ocupied_status::UNUSED ) {
				if ( p_ans->try_to_get_owner() ) {
					return p_ans;
				}
			}
			p_ans = p_ans->get_next();
		}

		// 空きノードが見つからなかったので、新しいノードを用意する。
		p_ans = add_one_new_hazard_ptr_node();

		//		printf( "glist is added.\n" );
		return p_ans;
	}

	void regist_delete_ptr( T* p_delete_target )
	{
		// 空きノードを探す。
		node_for_delete_ptr<T>* p_ans = delete_ptr_head_node_.get_next();
		while ( p_ans != nullptr ) {
			if ( p_ans->is_emptry() ) {
				if ( p_ans->try_to_set_delete_ptr( p_delete_target ) ) {
					post_trigger_gc();
					return;
				}
			}
			p_ans = p_ans->get_next();
		}

		// 空きノードが見つからなかったので、新しいノードを用意する。
		add_one_new_delete_ptr_node( p_delete_target );

//		printf( "glist is added.\n" );

		post_trigger_gc();

		return;
	}

	std::tuple<int, int> debug_get_glist_size( void )
	{
		return std::tuple<int, int>( glist_hzrd_ptr_node_count_, glist_dlt_ptr_node_count_ );
	}

private:
	/*!
	 * @breif	新しいノードを用意し、リストに追加する。
	 *
	 * 追加したノードは、USING状態で返される。
	 *
	 * @return	追加した新しいノードへのポインタ
	 */
	node_for_hazard_ptr<T>* add_one_new_hazard_ptr_node( void )
	{
		node_for_hazard_ptr<T>* p_ans = new node_for_hazard_ptr<T>();
		node_for_hazard_ptr<T>* p_next_check;
		p_next_check = hazard_ptr_head_node_.get_next();
		while ( true ) {
			p_ans->set_next( p_next_check );
			if ( hazard_ptr_head_node_.next_CAS( &p_next_check, p_ans ) ) {
				break;
			}
		}
		glist_hzrd_ptr_node_count_++;

		//		printf( "glist is added.\n" );
		return p_ans;
	}

	/*!
	 * @breif	新しいノードを用意し、削除リストに追加する。
	 *
	 * 空ノードを追加する場合は、引数にnullptrを指定する。
	 */
	void add_one_new_delete_ptr_node( T* p_target_node_ )
	{
		node_for_delete_ptr<T>* p_ans = new node_for_delete_ptr<T>( p_target_node_ );
		node_for_delete_ptr<T>* p_next_check;
		p_next_check = delete_ptr_head_node_.get_next();
		while ( true ) {
			p_ans->set_next( p_next_check );
			if ( delete_ptr_head_node_.next_CAS( &p_next_check, p_ans ) ) {
				break;
			}
		}
		glist_dlt_ptr_node_count_++;

		//		printf( "glist is added.\n" );
		return;
	}

	/*!
	 * @breif	指定されたポインタが、ハザードポインタに存在するかどうかを確認する。
	 *
	 * @retval	true	ハザードポインタに存在する。
	 * @retval	false	ハザードポインタに存在しない。
	 */
	bool scan_hazard_ptr( T* p_chk )
	{
		bool                    ans        = false;
		node_for_hazard_ptr<T>* p_cur_node = hazard_ptr_head_node_.get_next();
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

	// ハザードポインタに存在するかどうかを確認してから削除を行う。
	void try_clean_up_delete_ptr( void ) override
	{
		node_for_delete_ptr<T>* p_ans = delete_ptr_head_node_.get_next();
		while ( p_ans != nullptr ) {
			if ( !( p_ans->is_emptry() ) ) {
				T* p_chk_del_ptr = p_ans->get_delete_ptr();
				if ( !( scan_hazard_ptr( p_chk_del_ptr ) ) ) {
					p_ans->do_delete_ptr();
				}
			}
			p_ans = p_ans->get_next();
		}
	}

#ifdef USE_PTHREAD_THREAD_LOCAL_STORAGE
	static void destr_fn( void* parm )
	{
		//		printf( "thread local destructor now being called -- " );

		if ( parm == nullptr ) return;   // なぜかnullptrで呼び出された。

		node_for_hazard_ptr<T>* p_target_node = reinterpret_cast<node_for_hazard_ptr<T>*>( parm );

		p_target_node->release_owner();

		//		printf( "thread local destructor is done.\n" );
		return;
	}
#endif

	hazard_node_glist( void )
	  : hazard_ptr_head_node_()
	{
#ifdef USE_PTHREAD_THREAD_LOCAL_STORAGE
		int status = pthread_key_create( &tls_key, destr_fn );
		if ( status < 0 ) {
			printf( "pthread_key_create failed, errno=%d", errno );
			exit( 1 );
		}
#endif
	}

	node_for_hazard_ptr<T> hazard_ptr_head_node_;
	int                    glist_hzrd_ptr_node_count_ = 0;

	node_for_delete_ptr<T> delete_ptr_head_node_;
	int                    glist_dlt_ptr_node_count_ = 0;
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
	 * 現在設定されているハザードポインタに対し、参照権を獲得している状態から削除権を獲得した状態として、削除候補リストへ登録する。
	 */
	void move_hazard_ptr_to_del_list( void )
	{
		check_local_storage();

		std::atomic_thread_fence( std::memory_order_acquire );
		T* p_try_delete            = p_hzd_ptr_node_->p_target_;
		p_hzd_ptr_node_->p_target_ = nullptr;
		std::atomic_thread_fence( std::memory_order_release );

		hazard_ptr_internal::hazard_node_glist<T>::get_instance().regist_delete_ptr( p_try_delete );

		return;
	}

	static std::tuple<int, int> debug_get_glist_size( void )
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
	static __thread hazard_ptr_internal::node_for_hazard_ptr<T>* p_hzd_ptr_node_;
	// C++11で、thread_localが使用可能となるが、g++でコンパイルした場合、スレッド終了時のdestructor処理実行時に、
	// すでにメモリ領域が破壊されている場合があるため、destructor処理の正常動作が期待できない。
	// そのため、その点を明示する意図として __thread を使用する。
	// また、 __thread の変数は、暗黙的にゼロ初期化されていることを前提としている。
#else
	static thread_local hazard_ptr_internal::node_for_hazard_ptr<T>* p_hzd_ptr_node_;

	struct destructor_func {
		hazard_ptr_internal::node_for_hazard_ptr<T>** pp_hzd_ptr_node_ = nullptr;

		~destructor_func()
		{
			//			printf( "thread local destructor now being called -- " );

			if ( pp_hzd_ptr_node_ == nullptr ) return;
			if ( *pp_hzd_ptr_node_ == nullptr ) return;

			( *pp_hzd_ptr_node_ )->head_thread_local_retire_list_.move_hazard_ptr_to_del_list();
			( *pp_hzd_ptr_node_ )->release_owner();

			//			printf( "thread local destructor is done.\n" );
			return;
		}
	};
	static thread_local destructor_func dest_inst_;
#endif
};

#ifdef USE_PTHREAD_THREAD_LOCAL_STORAGE
template <typename T>
__thread hazard_ptr_internal::node_for_hazard_ptr<T>* hazard_ptr<T>::p_hzd_ptr_node_;
#else
template <typename T>
thread_local hazard_ptr_internal::node_for_hazard_ptr<T>* hazard_ptr<T>::p_hzd_ptr_node_ = nullptr;
template <typename T>
thread_local typename hazard_ptr<T>::destructor_func hazard_ptr<T>::dest_inst_;
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
