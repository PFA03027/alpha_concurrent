/*!
 * @file	dynamic_tls.cpp
 * @brief	Dynamic allocatable thread local storage
 * @author	Teruaki Ata
 * @date	Created on 2021/01/04
 * @details	This class provide thread local storage dynamic allocation.
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#include <limits.h>
#include <pthread.h>
#include <string.h>

#include <atomic>
#include <cerrno>
#include <cstdlib>

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/dynamic_tls.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

#define STRERROR_BUFF_SIZE ( 256 )

static std::atomic<int> cur_count_of_tls_keys( 0 );
static std::atomic<int> max_count_of_tls_keys( 0 );

void error_log_output( int errno_arg, const char* p_func_name )
{
	char buff[STRERROR_BUFF_SIZE];

#if defined( _WIN32 ) || defined( _WIN64 )
	int tr_ret                   = strerror_s( buff, STRERROR_BUFF_SIZE, errno_arg );
	buff[STRERROR_BUFF_SIZE - 1] = 0;

	if ( tr_ret == 0 ) {
		internal::LogOutput(
			log_type::ERR,
			"%s failed, num of used tls key: %d, max num of used tls key: %d, errno=%d, %s",
			p_func_name, get_num_of_tls_key(), get_max_num_of_tls_key(), errno_arg, buff );
	} else {
		internal::LogOutput(
			log_type::ERR,
			"%s failed, num of used tls key: %d, max num of used tls key: %d, errno=%d, and strerror_r() also fail %d",
			p_func_name, get_num_of_tls_key(), get_max_num_of_tls_key(), errno_arg, tr_ret );
	}
#else
#if ( _POSIX_C_SOURCE >= 200112L ) && !_GNU_SOURCE
	int tr_ret                   = strerror_r( errno_arg, buff, STRERROR_BUFF_SIZE - 1 );
	buff[STRERROR_BUFF_SIZE - 1] = 0;

	if ( tr_ret == 0 ) {
		internal::LogOutput(
			log_type::ERR,
			"%s failed, num of used tls key: %d, max num of used tls key: %d, errno=%d, %s",
			p_func_name, get_num_of_tls_key(), get_max_num_of_tls_key(), errno_arg, buff );
	} else {
		internal::LogOutput(
			log_type::ERR,
			"%s failed, num of used tls key: %d, max num of used tls key: %d, errno=%d, and strerror_r() also fail %d",
			p_func_name, get_num_of_tls_key(), get_max_num_of_tls_key(), errno_arg, tr_ret );
	}
#else
	char* errstr                 = strerror_r( errno_arg, buff, STRERROR_BUFF_SIZE - 1 );
	buff[STRERROR_BUFF_SIZE - 1] = 0;

	internal::LogOutput(
		log_type::ERR,
		"%s failed, num of used tls key: %d, max num of used tls key: %d, errno=%d, %s",
		p_func_name, get_num_of_tls_key(), get_max_num_of_tls_key(), errno_arg, errstr );
#endif
#endif

	return;
}

class dynamic_tls_content_array;
class dynamic_tls_key_array;
void call_destructor_for_array( dynamic_tls_key_array* p_key_array_arg, dynamic_tls_content_array* p_content_array_arg );

class dynamic_tls_content_array {
public:
	struct get_result {
		bool  is_in_this_array_;   //!< このdynamic_tls_content_arrayに含まれる場合にtrue
		void* p_data_;             //!< dynamic_tls_content_arrayから読みだされた値
	};

	dynamic_tls_content_array( unsigned int base_idx_arg )
	  : p_next_( nullptr )
	  , base_idx_( base_idx_arg )
	{
		for ( auto& e : content_array_ ) {
			e = nullptr;
		}
	}

	get_result get_tls( unsigned int idx ) const
	{
		get_result ans { false, nullptr };
		if ( idx < base_idx_ ) return ans;
		if ( idx >= ( base_idx_ + ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE ) ) return ans;

		ans.is_in_this_array_ = true;
		ans.p_data_           = content_array_[idx - base_idx_];

		return ans;
	}

	bool set_tls( unsigned int idx, void* p_data_arg )
	{
		if ( idx < base_idx_ ) return false;
		if ( idx >= ( base_idx_ + ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE ) ) return false;

		content_array_[idx - base_idx_] = p_data_arg;

		return true;
	}

	dynamic_tls_content_array* p_next_;   //!< thread local storage方向で、次のarrayへのポインタ
	const unsigned int         base_idx_;

private:
	void* content_array_[ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE];

	friend void call_destructor_for_array_and_clear_data( dynamic_tls_key_array* p_key_array_arg, dynamic_tls_content_array* p_content_array_arg );
};

class dynamic_tls_content_head {
public:
	enum class cnt_arry_state {
		// NOT_ALLOC,   //!< dynamic_tls_content_arrayが割り当てられていない
		NOT_USED,   //!< dynamic_tls_content_arrayは割り当てられているが、スレッド終了などにより未使用状態となっている
		USED        //!< dynamic_tls_content_arrayは使用中
	};

	dynamic_tls_content_head( void )
	  : p_next_( nullptr )
	  , ownership_state_( cnt_arry_state::USED )
	  , p_head_content_( nullptr )
	{
	}

	void* get_tls( unsigned int idx )
	{
		if ( ownership_state_.load( std::memory_order_acquire ) != cnt_arry_state::USED ) return nullptr;

		dynamic_tls_content_array* p_cur_tls_ca = p_head_content_;
		while ( p_cur_tls_ca != nullptr ) {
			auto ret = p_cur_tls_ca->get_tls( idx );
			if ( ret.is_in_this_array_ ) {
				return ret.p_data_;
			}
			p_cur_tls_ca = p_cur_tls_ca->p_next_;
		}

		// 見つからなかったので、dynamic_tls_content_arrayが不足しているので、追加
		unsigned int               base_idx = ( idx / ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE ) * ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE;
		dynamic_tls_content_array* p_new    = new dynamic_tls_content_array( base_idx );
		p_new->p_next_                      = p_head_content_;
		p_head_content_                     = p_new;

		return nullptr;   // 見つからなかったので、nullptrを返す
	}

	bool set_tls( unsigned int idx, void* p_data_arg )
	{
		if ( ownership_state_.load( std::memory_order_acquire ) != cnt_arry_state::USED ) return false;

		dynamic_tls_content_array* p_cur_tls_ca = p_head_content_;
		while ( p_cur_tls_ca != nullptr ) {
			auto ret = p_cur_tls_ca->set_tls( idx, p_data_arg );
			if ( ret ) {
				return true;
			}
			p_cur_tls_ca = p_cur_tls_ca->p_next_;
		}

		// 見つからなかったので、dynamic_tls_content_arrayが不足しているので、追加
		unsigned int               base_idx = ( idx / ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE ) * ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE;
		dynamic_tls_content_array* p_new    = new dynamic_tls_content_array( base_idx );
		p_new->p_next_                      = p_head_content_;
		p_head_content_                     = p_new;

		return p_head_content_->set_tls( idx, p_data_arg );
	}

	bool try_get_ownership( void )
	{
		cnt_arry_state expect = cnt_arry_state::NOT_USED;
		return ownership_state_.compare_exchange_strong( expect, cnt_arry_state::USED );
	}

	void call_destructor_and_release_ownership( void );

	std::atomic<dynamic_tls_content_head*> p_next_;   //!< thread方向で、次のarrayへのポインタ

private:
	std::atomic<cnt_arry_state> ownership_state_;   //!< 所有者の有無の状態
	dynamic_tls_content_array*  p_head_content_;
};

class dynamic_tls_key_array {
public:
	dynamic_tls_key_array( unsigned int base_idx_arg )
	  : p_next_( nullptr )
	  , base_idx_( base_idx_arg )
	  , num_of_free_( ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE )
	  , hint_to_alloc_( 0 )
	{
		for ( int i = 0; i < ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE; i++ ) {
			key_array_[i].idx_ = i + base_idx_;
		}
	}

	dynamic_tls_key* allocate_key( void ( *destructor_arg )( void* ) )
	{
		if ( num_of_free_.load( std::memory_order_acquire ) <= 0 ) {
			return nullptr;
		}

		int new_hint = hint_to_alloc_.load( std::memory_order_acquire );
		for ( int i = 0; i < ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE; i++ ) {
			dynamic_tls_key::alloc_stat expected_alloc_stat = dynamic_tls_key::alloc_stat::NOT_USED;
			int                         cur_hint            = new_hint;
			new_hint++;
			if ( new_hint >= ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE ) {
				new_hint = 0;
			}
			if ( key_array_[cur_hint].is_used_.compare_exchange_strong( expected_alloc_stat, dynamic_tls_key::alloc_stat::USED ) ) {
				// 割り当て成功
				num_of_free_.fetch_sub( 1 );
				key_array_[cur_hint].tls_destructor_.store( destructor_arg, std::memory_order_release );
				hint_to_alloc_.store( new_hint, std::memory_order_release );
				return &( key_array_[cur_hint] );
			}
		}

		return nullptr;
	}

	bool release_key( dynamic_tls_key* p_key_arg );

	std::atomic<dynamic_tls_key_array*> p_next_;
	const unsigned int                  base_idx_;

private:
	std::atomic<int> num_of_free_;
	std::atomic<int> hint_to_alloc_;
	dynamic_tls_key  key_array_[ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE];

	friend void call_destructor_for_array_and_clear_data( dynamic_tls_key_array* p_key_array_arg, dynamic_tls_content_array* p_content_array_arg );
};

class dynamic_tls_mgr {
public:
	static dynamic_tls_mgr& get_instance( void );

	dynamic_tls_key* allocate_key( void ( *destructor_arg )( void* ) )
	{
		dynamic_tls_key* p_ans = nullptr;
		do {
			p_ans = search_key( destructor_arg );
			if ( p_ans != nullptr ) {
				break;
			}
			push_front_dynamic_tls_key_array();
		} while ( p_ans == nullptr );

		return p_ans;
	}

	bool release_key( dynamic_tls_key* p_key_arg )
	{
		dynamic_tls_key_array* p_cur_dtls_ka = p_top_dtls_key_array_.load( std::memory_order_acquire );
		while ( p_cur_dtls_ka != nullptr ) {
			bool ans = p_cur_dtls_ka->release_key( p_key_arg );
			if ( ans ) {
				return ans;
			}
			dynamic_tls_key_array* p_nxt_dtls_ka = p_cur_dtls_ka->p_next_.load( std::memory_order_acquire );
			p_cur_dtls_ka                        = p_nxt_dtls_ka;
		}
		return false;
	}

	dynamic_tls_key_array* get_dynamic_tls_key_array( unsigned int base_idx_arg )
	{
		dynamic_tls_key_array* p_cur_dtls_ka = p_top_dtls_key_array_.load( std::memory_order_acquire );
		while ( p_cur_dtls_ka != nullptr ) {
			if ( base_idx_arg == p_cur_dtls_ka->base_idx_ ) {
				return p_cur_dtls_ka;
			}
			dynamic_tls_key_array* p_nxt_dtls_ka = p_cur_dtls_ka->p_next_.load( std::memory_order_acquire );
			p_cur_dtls_ka                        = p_nxt_dtls_ka;
		}
		return nullptr;
	}

	/**
	 * @brief Get the top dynamic tls content head object list
	 *
	 * @return dynamic_tls_content_head*
	 */
	dynamic_tls_content_head* get_top_dynamic_tls_content_head( void )
	{
		return p_top_dtls_content_head_.load( std::memory_order_acquire );
	}

	/**
	 * @brief Get the current thread dynamic tls content head object
	 *
	 * @return dynamic_tls_content_head*
	 */
	dynamic_tls_content_head* get_current_thread_dynamic_tls_content_head( void )
	{
		dynamic_tls_content_head* p_ans = reinterpret_cast<dynamic_tls_content_head*>( pthread_getspecific( key_ ) );
		if ( p_ans != nullptr ) return p_ans;

		// 空きdynamic_tls_content_headを探す
		dynamic_tls_content_head* p_cur = p_top_dtls_content_head_.load( std::memory_order_acquire );
		while ( p_cur != nullptr ) {
			if ( p_cur->try_get_ownership() ) {
				p_ans = p_cur;
				break;
			}
			p_cur = p_cur->p_next_.load( std::memory_order_acquire );
		}
		if ( p_ans == nullptr ) {
			p_ans                               = new dynamic_tls_content_head;   // 生成されたdynamic_tls_content_headは、コンストラクタによって、使用中状態で初期化されている。
			dynamic_tls_content_head* p_cur_top = p_top_dtls_content_head_.load( std::memory_order_acquire );
			do {
				p_ans->p_next_.store( p_cur_top, std::memory_order_release );
			} while ( !p_top_dtls_content_head_.compare_exchange_strong( p_cur_top, p_ans ) );
			dtls_content_head_cnt_.fetch_add( 1 );
		}

		pthread_setspecific( key_, reinterpret_cast<dynamic_tls_content_head*>( p_ans ) );

		return p_ans;
	}

	dynamic_tls_status_info get_status( void ) const
	{
		dynamic_tls_status_info ans { dtls_content_head_cnt_.load( std::memory_order_acquire ), next_base_idx_.load( std::memory_order_acquire ) };
		return ans;
	}

	static void destructor( void* p_data );

private:
	dynamic_tls_mgr( void )
	  : next_base_idx_( 0 )
	  , p_top_dtls_key_array_( nullptr )
	  , p_top_dtls_content_head_( nullptr )
	  , dtls_content_head_cnt_( 0 )
	{
		int status = pthread_key_create( &key_, destructor );
		if ( status != 0 ) {
			error_log_output( status, "pthread_key_create()" );
			internal::LogOutput( log_type::ERR, "PTHREAD_KEYS_MAX: %d", PTHREAD_KEYS_MAX );
			std::abort();   // because of the critical error, let's exit. TODO: should throw std::runtime_error ?
		}

		push_front_dynamic_tls_key_array();
	}

	~dynamic_tls_mgr()
	{
		int status = pthread_key_delete( key_ );
		if ( status != 0 ) {
			error_log_output( status, "pthread_key_delete()" );
			internal::LogOutput( log_type::ERR, "PTHREAD_KEYS_MAX: %d", PTHREAD_KEYS_MAX );
			std::abort();   // because of the critical error, let's exit. TODO: should throw std::runtime_error ?
		}

		dynamic_tls_key_array* p_cur_dtls_ka = p_top_dtls_key_array_.load( std::memory_order_acquire );
		p_top_dtls_key_array_.store( nullptr, std::memory_order_release );
		while ( p_cur_dtls_ka != nullptr ) {
			dynamic_tls_key_array* p_nxt_dtls_ka = p_cur_dtls_ka->p_next_.load( std::memory_order_acquire );
			delete p_cur_dtls_ka;
			p_cur_dtls_ka = p_nxt_dtls_ka;
		}

		return;
	}

	void push_front_dynamic_tls_key_array( void )
	{
		unsigned int           cur_base_idx     = next_base_idx_.fetch_add( ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE );
		dynamic_tls_key_array* p_new_dtls_ka    = new dynamic_tls_key_array( cur_base_idx );   // 可変長とするために、malloc()使われてしまうのは割り切る
		dynamic_tls_key_array* p_expect_dtls_ka = p_top_dtls_key_array_.load( std::memory_order_acquire );
		do {   // 置き換えに成功するまでビジーループ
			p_new_dtls_ka->p_next_.store( p_expect_dtls_ka, std::memory_order_release );
		} while ( !p_top_dtls_key_array_.compare_exchange_strong( p_expect_dtls_ka, p_new_dtls_ka ) );
	}

	dynamic_tls_key* search_key( void ( *destructor_arg )( void* ) )
	{
		dynamic_tls_key*       p_ans         = nullptr;
		dynamic_tls_key_array* p_cur_dtls_ka = p_top_dtls_key_array_.load( std::memory_order_acquire );
		while ( p_cur_dtls_ka != nullptr ) {
			p_ans = p_cur_dtls_ka->allocate_key( destructor_arg );
			if ( p_ans != nullptr ) {
				return p_ans;
			}
			dynamic_tls_key_array* p_nxt_dtls_ka = p_cur_dtls_ka->p_next_.load( std::memory_order_acquire );
			p_cur_dtls_ka                        = p_nxt_dtls_ka;
		}
		return nullptr;
	}

	pthread_key_t key_;

	std::atomic<unsigned int>              next_base_idx_;
	std::atomic<dynamic_tls_key_array*>    p_top_dtls_key_array_;
	std::atomic<dynamic_tls_content_head*> p_top_dtls_content_head_;
	std::atomic<unsigned int>              dtls_content_head_cnt_;
};

void dynamic_tls_mgr::destructor( void* p_data )
{
	dynamic_tls_content_head* p_destruct_dtls_head = reinterpret_cast<dynamic_tls_content_head*>( p_data );
	p_destruct_dtls_head->call_destructor_and_release_ownership();

	return;
}

dynamic_tls_mgr& dynamic_tls_mgr::get_instance( void )
{
	static dynamic_tls_mgr singleton;
	return singleton;
}

void call_destructor_for_array_and_clear_data( dynamic_tls_key_array* p_key_array_arg, dynamic_tls_content_array* p_content_array_arg )
{
	for ( int j = 0; j < ALCONCURRENT_CONF_DYNAMIC_TLS_DESTUCT_ITERATE_MAX; j++ ) {
		for ( int i = 0; i < ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE; i++ ) {
			if ( p_key_array_arg->key_array_[i].is_used_.load( std::memory_order_acquire ) != dynamic_tls_key::alloc_stat::USED ) continue;

			void* p_tmp                            = p_content_array_arg->content_array_[i];
			p_content_array_arg->content_array_[i] = nullptr;
			( *( p_key_array_arg->key_array_[i].tls_destructor_.load( std::memory_order_acquire ) ) )( p_tmp );
		}
		bool is_finish = true;
		for ( int i = 0; i < ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE; i++ ) {
			if ( p_content_array_arg->content_array_[i] != nullptr ) {
				is_finish = false;
				break;
			}
		}
		if ( is_finish ) {
			return;
		}
	}
	for ( int i = 0; i < ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE; i++ ) {
		p_content_array_arg->content_array_[i] = nullptr;
	}

	return;
}

void dynamic_tls_content_head::call_destructor_and_release_ownership( void )
{
	dynamic_tls_content_array* p_cur_tls_ca = p_head_content_;
	while ( p_cur_tls_ca != nullptr ) {
		dynamic_tls_key_array* p_pare = dynamic_tls_mgr::get_instance().get_dynamic_tls_key_array( p_cur_tls_ca->base_idx_ );
		if ( p_pare != nullptr ) {
			call_destructor_for_array_and_clear_data( p_pare, p_cur_tls_ca );
		}
		p_cur_tls_ca = p_cur_tls_ca->p_next_;
	}

	ownership_state_.store( cnt_arry_state::NOT_USED );

	return;
}

bool dynamic_tls_key_array::release_key( dynamic_tls_key* p_key_arg )
{
	if ( p_key_arg < key_array_ ) return false;
	if ( &( key_array_[ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE] ) <= p_key_arg ) return false;

	// releaseの作業権の確保
	dynamic_tls_key::alloc_stat expected_alloc_stat = dynamic_tls_key::alloc_stat::USED;
	if ( !p_key_arg->is_used_.compare_exchange_strong( expected_alloc_stat, dynamic_tls_key::alloc_stat::RELEASING ) ) return false;

	// releaseの作業権が確保できた
	p_key_arg->tls_destructor_.store( nullptr, std::memory_order_release );

	// すべての該当するスレッドローカルストレージをクリアする
	dynamic_tls_content_head* p_cur_dtls_c = dynamic_tls_mgr::get_instance().get_top_dynamic_tls_content_head();
	while ( p_cur_dtls_c != nullptr ) {
		p_cur_dtls_c->set_tls( p_key_arg->idx_, nullptr );
		p_cur_dtls_c = p_cur_dtls_c->p_next_.load( std::memory_order_acquire );
	}

	num_of_free_.fetch_add( 1 );

	// release作業が完了したので、未使用状態に変更する。
	p_key_arg->is_used_.store( dynamic_tls_key::alloc_stat::NOT_USED, std::memory_order_release );

	return true;
}

void dynamic_tls_key_create( dynamic_tls_key_t* p_key, void ( *destructor )( void* ) )
{
	*p_key = dynamic_tls_mgr::get_instance().allocate_key( destructor );

	cur_count_of_tls_keys++;

	int cur_max = max_count_of_tls_keys.load();
	if ( cur_max < cur_count_of_tls_keys.load() ) {
		max_count_of_tls_keys.compare_exchange_strong( cur_max, cur_count_of_tls_keys.load() );
	}
	return;
}

void dynamic_tls_key_release( dynamic_tls_key_t key )
{
	if ( dynamic_tls_mgr::get_instance().release_key( key ) ) {
		cur_count_of_tls_keys--;
	}
	return;
}

bool dynamic_tls_setspecific( dynamic_tls_key_t key, void* p_data_arg )
{
	if ( key->is_used_.load( std::memory_order_acquire ) != dynamic_tls_key::alloc_stat::USED ) return false;

	dynamic_tls_content_head* p_cur_dtls = dynamic_tls_mgr::get_instance().get_current_thread_dynamic_tls_content_head();
	return p_cur_dtls->set_tls( key->idx_, p_data_arg );
}

void* dynamic_tls_getspecific( dynamic_tls_key_t key )
{
	if ( key->is_used_.load( std::memory_order_acquire ) != dynamic_tls_key::alloc_stat::USED ) return nullptr;

	dynamic_tls_content_head* p_cur_dtls = dynamic_tls_mgr::get_instance().get_current_thread_dynamic_tls_content_head();
	return p_cur_dtls->get_tls( key->idx_ );
}

dynamic_tls_status_info dynamic_tls_get_status( void )
{
	return dynamic_tls_mgr::get_instance().get_status();
}

/*!
 * @brief	get the number of dynamic thread local memory
 *
 *　pthread_key_create()での割り当て数を取得する。
 *　不具合解析など使用する。
 */
int get_num_of_tls_key( void )
{
	return cur_count_of_tls_keys.load();
}

/*!
 * @brief	get the max number of dynamic thread local memory
 *
 *　pthread_key_create()での割り当て数を取得する。
 *　不具合解析など使用する。
 */
int get_max_num_of_tls_key( void )
{
	return max_count_of_tls_keys.load();
}

}   // namespace internal

}   // namespace concurrent
}   // namespace alpha
