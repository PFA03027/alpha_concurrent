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

#include <cerrno>
#include <cstdlib>

#include <atomic>
#include <mutex>
#include <thread>

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/dynamic_tls.hpp"

#include "alconcurrent/internal/alloc_only_allocator.hpp"
#include "utility.hpp"

namespace alpha {
namespace concurrent {

std::recursive_mutex dynamic_tls_global_exclusive_control_for_destructions;   //!< to avoid rece condition b/w thread local destruction and normal destruction globally

namespace internal {

constexpr size_t STRERROR_BUFF_SIZE = 256;

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

////////////////////////////////////////////////////////////////////////////////////////////////////////
// configuration value
constexpr size_t conf_pre_mmap_size = 1024 * 1024;

static alloc_only_chamber g_alloc_only_inst( false, conf_pre_mmap_size );   // グローバルインスタンスは、プロセス終了までメモリ領域を維持するために、デストラクタが呼ばれてもmmapした領域を解放しない。

/**
 * @brief allocate memory that requester side does not free
 *
 * memory allocated by this I/F could not free.
 *
 * @param req_size memory size to allocate
 * @param req_align allocated memory address alignment
 * @return void* pointer to the allocated memory
 */
inline void* dynamic_tls_key_allocating_only( size_t req_size, size_t req_align = default_align_size )
{
	return g_alloc_only_inst.allocate( req_size, req_align );
}

/**
 * @brief dump log of pre-defined global dynamic_tls_key_allocating_only()
 *
 */
void dynamic_tls_key_allocating_only_dump_to_log( log_type lt, char c, int id )
{
	g_alloc_only_inst.dump_to_log( lt, c, id );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

struct dynamic_tls_key {
	enum class alloc_stat {
		NOT_USED = 0,
		RELEASING,
		USED
	};

	size_t                  idx_;       //!< index of key
	std::atomic<alloc_stat> is_used_;   //!< flag whether this key is used or not.
#ifdef ALCONCURRENT_CONF_ENABLE_INDIVIDUAL_KEY_EXCLUSIVE_ACCESS
	std::atomic<int> acc_cnt_;   //!< count of accessor
#endif
	std::atomic<void*>                          tls_p_data_;        //!< atomic pointer of the paramter data for thread local storage
	std::atomic<uintptr_t ( * )( void* )>       tls_allocator_;     //!< atomic pointer of allocator for thread local storage
	std::atomic<void ( * )( uintptr_t, void* )> tls_deallocator_;   //!< atomic pointer of deallocator for thread local storage
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	bt_info bt_when_allocate_;   //!< back trace information when key is allocated
#endif

	constexpr dynamic_tls_key( void )
	  : idx_( 0 )
	  , is_used_( alloc_stat::NOT_USED )
	  , tls_p_data_( nullptr )
	  , tls_allocator_( nullptr )
	  , tls_deallocator_( nullptr )
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	  , bt_when_allocate_()
#endif
	{
	}
};

class dynamic_tls_key_array;

class dynamic_tls_content_array {
public:
	struct tls_data_and_stat {
		enum class stat {
			UNINITIALIZED = 0,
			USED,
			DESTRUCTING
		};

		constexpr tls_data_and_stat( void )
		  : tls_stat_( stat::UNINITIALIZED )
		  , tls_data_( 0 )
		{
		}

		get_result get_tls( dynamic_tls_key_t key )
		{
			tls_data_and_stat::stat cur_stat = tls_stat_.load( std::memory_order_acquire );
#if 1
			if ( cur_stat != tls_data_and_stat::stat::USED ) {
				// not initialized yet
				if ( cur_stat == tls_data_and_stat::stat::UNINITIALIZED ) {
					// if before initialized
					auto p_func = key->tls_allocator_.load( std::memory_order_acquire );
					if ( p_func != nullptr ) {
						tls_data_ = ( *p_func )( key->tls_p_data_ );
					}
					tls_stat_.store( tls_data_and_stat::stat::USED, std::memory_order_release );
				} else {
					// cur_stat == tls_data_and_stat::stat::DESTRUCTING or unknown value
					// Should not happen this condition
					internal::LogOutput( log_type::ERR, "into the unexpected condition for dynamic_tls_content_array::get_tls()" );
					return get_result { op_ret::UNEXPECT_ERR, 0U };   // fail to get
				}
			} else {
				// already initialized
			}
			return get_result { op_ret::SUCCESS, tls_data_ };
#else
			switch ( cur_stat ) {
				case tls_data_and_stat::stat::UNINITIALIZED: {
					// if before initialized
					auto p_func = key->tls_allocator_.load( std::memory_order_acquire );
					if ( p_func != nullptr ) {
						tls_data_ = ( *p_func )( key->tls_p_data_ );
					}
					tls_stat_.store( tls_data_and_stat::stat::USED, std::memory_order_release );
				} break;
				case tls_data_and_stat::stat::USED: {
					// already initialized
				} break;

				default:
				case tls_data_and_stat::stat::DESTRUCTING: {
					// Should not happen this condition
					internal::LogOutput( log_type::ERR, "into the unexpected condition for dynamic_tls_content_array::get_tls()" );
					return get_result { op_ret::UNEXPECT_ERR, 0U };   // fail to get
				} break;
			}
			return get_result { op_ret::SUCCESS, tls_data_ };
#endif
		}

		op_ret set_tls( dynamic_tls_key_t key, uintptr_t p_data_arg )
		{
			tls_data_and_stat::stat cur_stat = tls_stat_.load( std::memory_order_acquire );
#if 1
			if ( ( cur_stat == tls_data_and_stat::stat::UNINITIALIZED ) || ( cur_stat == tls_data_and_stat::stat::USED ) ) {
				tls_stat_.store( tls_data_and_stat::stat::USED, std::memory_order_release );
				tls_data_ = p_data_arg;
				return op_ret::SUCCESS;
			} else {
				// Should not happen this condition
				internal::LogOutput( log_type::ERR, "into the unexpected condition for dynamic_tls_content_array::set_tls()" );
				return op_ret::UNEXPECT_ERR;   // fail to get
			}
#else
			switch ( cur_stat ) {
				case tls_data_and_stat::stat::UNINITIALIZED: {
					// if before initialized, ...TODO: what should we do ?
				} break;
				case tls_data_and_stat::stat::USED: {
					// already initialized
				} break;

				default:
				case tls_data_and_stat::stat::DESTRUCTING: {
					// Should not happen this condition
					internal::LogOutput( log_type::ERR, "into the unexpected condition for dynamic_tls_content_array::set_tls()" );
					return op_ret::UNEXPECT_ERR;   // fail to get
				} break;
			}
			tls_data_ = p_data_arg;
			tls_stat_.store( tls_data_and_stat::stat::USED, std::memory_order_release );

			return op_ret::SUCCESS;
#endif
		}

		op_ret destruct_tls_by_thread_exit( dynamic_tls_key_t key )
		{
			if ( key->is_used_.load( std::memory_order_acquire ) != dynamic_tls_key::alloc_stat::USED ) {
				// dynamic_tls_key::alloc_stat::NOT_USEDの場合、デストラクト処理は不要なので、デストラクト処理完了済みと判定し、trueを返す。
				// dynamic_tls_key::alloc_stat::RELEASINGの場合、近い将来デストラクト処理はキー解放処理の中でデストラクト処理が実行されるので、デストラクト処理完了済みと判定し、trueを返す。
				return op_ret::SUCCESS;
			}

			// keyは有効。
			tls_data_and_stat::stat expect_stat = tls_data_and_stat::stat::USED;
			if ( !( tls_stat_.compare_exchange_strong( expect_stat, tls_data_and_stat::stat::DESTRUCTING ) ) ) {
				// UNINITIALIZED(=初期化前)か、DESTRUCTING(デストラクト処理中)なので、デストラクト処理完了済みと判定し、trueを返す。
				return op_ret::SUCCESS;
			}

			// デストラクト処理の実行権を取得したので、デストラクト処理を開始する
			auto p_func = key->tls_deallocator_.load( std::memory_order_acquire );
			if ( p_func != nullptr ) {
				( *p_func )( tls_data_, key->tls_p_data_ );
			}
			tls_stat_.store( tls_data_and_stat::stat::UNINITIALIZED, std::memory_order_release );
			return op_ret::SUCCESS;
		}

		op_ret destruct_tls_by_key_release( dynamic_tls_key_t key )
		{
			dynamic_tls_key::alloc_stat cur_key_stat = key->is_used_.load( std::memory_order_acquire );
			switch ( cur_key_stat ) {
				case dynamic_tls_key::alloc_stat::NOT_USED: {
					// dynamic_tls_key::alloc_stat::NOT_USEDの場合、デストラクト処理は不要なので、デストラクト処理完了済みと判定し、trueを返す。
					return op_ret::SUCCESS;
				} break;

				default:
				case dynamic_tls_key::alloc_stat::USED: {
					// dynamic_tls_key::alloc_stat::USEDの場合、本来起きてはならない条件のため、デストラクト処理失敗と判定し、falseを返す。
					return op_ret::UNEXPECT_ERR;
				} break;

				case dynamic_tls_key::alloc_stat::RELEASING: {
					// この関数は、dynamic_tls_keyの解放処理関数から呼び出される想定のため、ここを通過するのが期待値。
				} break;
			}

			// keyは解放処理中が確認された状態。
			tls_data_and_stat::stat expect_stat = tls_data_and_stat::stat::USED;
			if ( !( tls_stat_.compare_exchange_strong( expect_stat, tls_data_and_stat::stat::DESTRUCTING ) ) ) {
				// UNINITIALIZED(=初期化前)か、DESTRUCTING(デストラクト処理中)なので、デストラクト処理完了済みと判定し、trueを返す。
				return op_ret::SUCCESS;
			}

			// デストラクト処理の実行権を取得したので、デストラクト処理を開始する
			auto p_func = key->tls_deallocator_.load( std::memory_order_acquire );
			if ( p_func != nullptr ) {
				( *p_func )( tls_data_, key->tls_p_data_ );
			}
			tls_stat_.store( tls_data_and_stat::stat::UNINITIALIZED, std::memory_order_release );
			return op_ret::SUCCESS;
		}

	private:
		std::atomic<stat> tls_stat_;
		uintptr_t         tls_data_;
	};

	struct search_result {
		op_ret             stat_;     //!< 取得処理に成功した場合にop_ret::SUCCESSとなる。それ以外の場合は、何らかのエラーによる失敗を示す。
		tls_data_and_stat* p_data_;   //!< 取得処理に成功した場合に、取得した値が保持される。取得に失敗した場合は、不定値となる。
	};

	dynamic_tls_content_array( size_t base_idx_arg )
	  : p_next_( nullptr )
	  , base_idx_( base_idx_arg )
	{
	}

	search_result search_tls_unchk_key( dynamic_tls_key_t key )
	{
		iterator cnt_it = search( key );
		return search_result { cnt_it != end() ? op_ret::SUCCESS : op_ret::OUT_OF_RANGE, cnt_it };   // 範囲外の場合は、OUT_OF_RANGEを返す。
	}
#if 0
	get_result get_tls_unchk_key( dynamic_tls_key_t key )
	{
		search_result s_ret = search_tls_unchk_key( key );
		if ( s_ret.stat_ == op_ret::SUCCESS ) {
			return s_ret.p_data_->get_tls( key );
		} else {
			return get_result { s_ret.stat_, 0UL };
		}
	}

	op_ret set_tls_unchk_key( dynamic_tls_key_t key, uintptr_t p_data_arg )
	{
		search_result s_ret = search_tls_unchk_key( key );
		if ( s_ret.stat_ == op_ret::SUCCESS ) {
			return s_ret.p_data_->set_tls( key, p_data_arg );
		} else {
			return s_ret.stat_;
		}
	}
#endif
	op_ret destruct_tls_by_key_release( dynamic_tls_key_t key )
	{
		iterator cnt_it = search( key );
		if ( cnt_it == end() ) {
			return op_ret::OUT_OF_RANGE;   // 範囲外なので、デストラクト処理失敗と判定し、OUT_OF_RANGEを返す。
		}

		return cnt_it->destruct_tls_by_key_release( key );
	}

	void* operator new( std::size_t n );             // usual new...(1)
	void  operator delete( void* p_mem ) noexcept;   // usual delete...(2)	dynamic_tls_content_arrayは、破棄しないクラスなので、メモリ開放しない

	void* operator new[]( std::size_t n );             // usual new...(1)
	void  operator delete[]( void* p_mem ) noexcept;   // usual delete...(2)	dynamic_tls_content_arrayは、破棄しないクラスなので、メモリ開放しない

	void* operator new( std::size_t n, void* p );          // placement new
	void  operator delete( void* p, void* p2 ) noexcept;   // placement delete...(3)

	dynamic_tls_content_array* p_next_;   //!< thread local storage方向で、次のarrayへのポインタ
	const size_t               base_idx_;

private:
	using iterator = tls_data_and_stat*;

	inline iterator begin( void )
	{
		return &( content_array_[0] );
	}

	inline iterator end( void )
	{
		return &( content_array_[ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE] );
	}

	iterator search( dynamic_tls_key_t key )
	{
		if ( key->idx_ < base_idx_ ) {
			// 範囲外なので、取得処理失敗と判定し、OUT_OF_RANGEを返す。
			return end();
		}
		if ( ( key->idx_ - base_idx_ ) >= ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE ) {
			// 範囲外なので、取得処理失敗と判定し、OUT_OF_RANGEを返す。
			return end();
		}
		return &( content_array_[key->idx_ - base_idx_] );
	}

	tls_data_and_stat content_array_[ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE];

	friend void call_destructor_for_array_and_clear_data( dynamic_tls_key_array* p_key_array_arg, dynamic_tls_content_array* p_content_array_arg );
};

class dynamic_tls_content_head {
public:
	enum class cnt_arry_state {
		// NOT_ALLOC,   //!< dynamic_tls_content_arrayが割り当てられていない
		NOT_USED,   //!< dynamic_tls_content_arrayは割り当てられているが、スレッド終了などにより未使用状態となっている
		USED        //!< dynamic_tls_content_arrayは使用中
	};

	constexpr dynamic_tls_content_head( void )
	  : p_next_( nullptr )
	  , ownership_state_( cnt_arry_state::USED )
	  , p_head_content_( nullptr )
	{
	}

	dynamic_tls_content_array::search_result search_tls_unchk_key( dynamic_tls_key_t key )
	{
		if ( ownership_state_.load( std::memory_order_acquire ) != cnt_arry_state::USED ) {
			return dynamic_tls_content_array::search_result { op_ret::INVALID, nullptr };
		}

		dynamic_tls_content_array* p_cur_tls_ca = p_head_content_.load( std::memory_order_acquire );
		while ( p_cur_tls_ca != nullptr ) {
			auto ret = p_cur_tls_ca->search_tls_unchk_key( key );
#if 1
			if ( ret.stat_ != op_ret::OUT_OF_RANGE ) {
				// case op_ret::INVALID:
				// case op_ret::INVALID_KEY:
				// case op_ret::UNEXPECT_ERR: {
				// 	// 引数のkeyに問題があるため、取得処理失敗で終了する。
				// case op_ret::SUCCESS: {
				//  // 処理成功のため、終了する
				return ret;
			}
#else
			switch ( ret.stat_ ) {
				default:
				case op_ret::INVALID:
				case op_ret::INVALID_KEY:
				case op_ret::UNEXPECT_ERR: {
					// 引数のkeyに問題があるため、取得処理失敗で終了する。
					return ret;
				} break;

				case op_ret::SUCCESS: {
					return ret;
				} break;
				case op_ret::OUT_OF_RANGE: {
					// continue to next dynamic_tls_content_array
				} break;
			}
#endif
			p_cur_tls_ca = p_cur_tls_ca->p_next_;
		}

		// keyに対応するdynamic_tls_content_arrayが見つからなかったので、dynamic_tls_content_arrayが不足している。よって、追加
		dynamic_tls_content_array* p_new = push_new_tls_array_for( key );

		return p_new->search_tls_unchk_key( key );
	}

	get_result get_tls_unchk_key( dynamic_tls_key_t key )
	{
		auto s_ret = search_tls_unchk_key( key );
		if ( s_ret.stat_ == op_ret::SUCCESS ) {
			return s_ret.p_data_->get_tls( key );
		} else {
			return get_result { s_ret.stat_, 0UL };
		}
	}

	op_ret set_tls_unchk_key( dynamic_tls_key_t key, uintptr_t p_data_arg )
	{
		auto s_ret = search_tls_unchk_key( key );
		if ( s_ret.stat_ == op_ret::SUCCESS ) {
			return s_ret.p_data_->set_tls( key, p_data_arg );
		} else {
			return s_ret.stat_;
		}
	}

	bool try_get_ownership( void )
	{
		cnt_arry_state expect = cnt_arry_state::NOT_USED;
		return ownership_state_.compare_exchange_strong( expect, cnt_arry_state::USED );
	}

	op_ret destruct_tls_by_key_release( dynamic_tls_key_t key )
	{
		if ( ownership_state_.load( std::memory_order_acquire ) != cnt_arry_state::USED ) return op_ret::INVALID;

		dynamic_tls_content_array* p_cur_tls_ca = p_head_content_.load( std::memory_order_acquire );
		while ( p_cur_tls_ca != nullptr ) {
			auto ret = p_cur_tls_ca->destruct_tls_by_key_release( key );
#if 1
			if ( ret != op_ret::OUT_OF_RANGE ) {
				return ret;
			}
#else
			switch ( ret ) {
				default:
				case op_ret::INVALID:
				case op_ret::INVALID_KEY:
				case op_ret::UNEXPECT_ERR: {
					// 引数のkeyに問題があるため、取得処理失敗で終了する。
					return ret;
				} break;

				case op_ret::SUCCESS: {
					return ret;
				} break;
				case op_ret::OUT_OF_RANGE: {
					// continue to next dynamic_tls_content_array
				} break;
			}
#endif
			p_cur_tls_ca = p_cur_tls_ca->p_next_;
		}

		return op_ret::OUT_OF_RANGE;
	}

	void call_destructor_and_release_ownership( void );

	void* operator new( std::size_t n );             // usual new...(1)
	void  operator delete( void* p_mem ) noexcept;   // usual delete...(2)	dynamic_tls_content_headは、破棄しないクラスなので、メモリ開放しない

	void* operator new[]( std::size_t n );             // usual new...(1)
	void  operator delete[]( void* p_mem ) noexcept;   // usual delete...(2)	dynamic_tls_content_headは、破棄しないクラスなので、メモリ開放しない

	void* operator new( std::size_t n, void* p );          // placement new
	void  operator delete( void* p, void* p2 ) noexcept;   // placement delete...(3)

	std::atomic<dynamic_tls_content_head*> p_next_;   //!< thread方向で、次のarrayへのポインタ

private:
	dynamic_tls_content_array* push_new_tls_array_for( dynamic_tls_key_t key )
	{
		size_t                     base_idx = ( key->idx_ / ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE ) * ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE;
		dynamic_tls_content_array* p_new    = new dynamic_tls_content_array( base_idx );
		p_new->p_next_                      = p_head_content_.load( std::memory_order_acquire );
		p_head_content_.store( p_new, std::memory_order_release );

		return p_new;
	}

	std::atomic<cnt_arry_state>             ownership_state_;   //!< 所有者の有無の状態
	std::atomic<dynamic_tls_content_array*> p_head_content_;
};

/**
 * @brief dynamic thread local のキー配列を管理するクラス
 *
 * @note
 * 一度確保したら解放することはないクラスなので、allocate_only()からメモリ領域を確保する。
 *
 */
class dynamic_tls_key_array {
public:
	dynamic_tls_key_array( unsigned int base_idx_arg )
	  : p_next_( nullptr )
	  , base_idx_( base_idx_arg )
	  , num_of_free_( ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE )
	  , hint_to_alloc_( 0 )
	{
		for ( size_t i = 0; i < ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE; i++ ) {
			key_array_[i].idx_ = i + base_idx_;
		}
	}

	dynamic_tls_key* allocate_key( void* p_param, uintptr_t ( *allocator )( void* p_param ), void ( *deallocator )( uintptr_t p_obj, void* p_param ) )
	{
		if ( num_of_free_.load( std::memory_order_acquire ) <= 0 ) {
			return nullptr;
		}

		size_t new_hint = hint_to_alloc_.load( std::memory_order_acquire );
		for ( size_t i = 0; i < ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE; i++ ) {
			dynamic_tls_key::alloc_stat expected_alloc_stat = dynamic_tls_key::alloc_stat::NOT_USED;
			size_t                      cur_hint            = new_hint;
			new_hint++;
			if ( new_hint >= ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE ) {
				new_hint = 0;
			}
			if ( key_array_[cur_hint].is_used_.compare_exchange_strong( expected_alloc_stat, dynamic_tls_key::alloc_stat::USED ) ) {
				// 割り当て成功
				num_of_free_.fetch_sub( 1, std::memory_order_acq_rel );
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
				RECORD_BACKTRACE_GET_BACKTRACE( key_array_[cur_hint].bt_when_allocate_ );
#endif
				key_array_[cur_hint].tls_p_data_.store( p_param, std::memory_order_release );
				key_array_[cur_hint].tls_allocator_.store( allocator, std::memory_order_release );
				key_array_[cur_hint].tls_deallocator_.store( deallocator, std::memory_order_release );
				hint_to_alloc_.store( new_hint, std::memory_order_release );
				return &( key_array_[cur_hint] );
			}
		}

		return nullptr;
	}

	bool release_key( dynamic_tls_key* p_key_arg );

	void* operator new( std::size_t n );             // usual new...(1)
	void  operator delete( void* p_mem ) noexcept;   // usual delete...(2)	dynamic_tls_key_arrayは、破棄しないクラスなので、メモリ開放しない

	void* operator new[]( std::size_t n );             // usual new...(1)
	void  operator delete[]( void* p_mem ) noexcept;   // usual delete...(2)	dynamic_tls_key_arrayは、破棄しないクラスなので、メモリ開放しない

	void* operator new( std::size_t n, void* p );          // placement new
	void  operator delete( void* p, void* p2 ) noexcept;   // placement delete...(3)

	std::atomic<dynamic_tls_key_array*> p_next_;
	const size_t                        base_idx_;

private:
	using iterator = dynamic_tls_key*;
	iterator begin( void )
	{
		return &( key_array_[0] );
	}
	iterator end( void )
	{
		return &( key_array_[ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE] );
	}

	std::atomic<size_t> num_of_free_;
	std::atomic<size_t> hint_to_alloc_;
	dynamic_tls_key     key_array_[ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE];

	friend void call_destructor_for_array_and_clear_data( dynamic_tls_key_array* p_key_array_arg, dynamic_tls_content_array* p_content_array_arg );
};

class dynamic_tls_mgr {
public:
	static inline dynamic_tls_mgr& get_instance( void );

	dynamic_tls_key* allocate_key( void* p_param, uintptr_t ( *allocator )( void* p_param ), void ( *deallocator )( uintptr_t p_obj, void* p_param ) )
	{
		dynamic_tls_key* p_ans = nullptr;
		do {
			p_ans = search_key( p_param, allocator, deallocator );
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

	dynamic_tls_key_array* get_dynamic_tls_key_array( size_t base_idx_arg )
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
		dynamic_tls_content_head* p_ans = tl_cnt_head_.get_p_content_head();
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
			dtls_content_head_cnt_.fetch_add( 1, std::memory_order_acq_rel );
		}

		tl_cnt_head_.set_p_content_head( p_ans );

		return p_ans;
	}

	dynamic_tls_status_info get_status( void ) const
	{
		dynamic_tls_status_info ans { dtls_key_array_cnt_.load( std::memory_order_acquire ), dtls_content_head_cnt_.load( std::memory_order_acquire ), next_base_idx_.load( std::memory_order_acquire ) };
		return ans;
	}

	static void destructor( void* p_data );

private:
#ifdef ALCONCURRENT_CONF_USE_THREAD_LOCAL
	class tl_content_head {
	public:
		constexpr tl_content_head( void )
		  : p_tl_cnt_head_( nullptr )
		{
		}

		~tl_content_head()
		{
			if ( p_tl_cnt_head_ == nullptr ) return;

			dynamic_tls_mgr::destructor( p_tl_cnt_head_ );
		}

		inline dynamic_tls_content_head* get_p_content_head( void )
		{
			return p_tl_cnt_head_;
		}

		inline void set_p_content_head( dynamic_tls_content_head* p_tl_cnt_head_arg )
		{
			p_tl_cnt_head_ = p_tl_cnt_head_arg;
		}

	private:
		dynamic_tls_content_head* p_tl_cnt_head_;
	};
#else
	class tl_content_head {
	public:
		tl_content_head( void )
		{
			int status = pthread_key_create( &key_, dynamic_tls_mgr::destructor );
			if ( status != 0 ) {
				error_log_output( status, "pthread_key_create()" );
				internal::LogOutput( log_type::ERR, "PTHREAD_KEYS_MAX: %d", PTHREAD_KEYS_MAX );
				std::abort();   // because of the critical error, let's exit. TODO: should throw std::runtime_error ?
			}
			return;
		}

		~tl_content_head()
		{
			int status = pthread_key_delete( key_ );
			if ( status != 0 ) {
				error_log_output( status, "pthread_key_delete()" );
				internal::LogOutput( log_type::ERR, "PTHREAD_KEYS_MAX: %d", PTHREAD_KEYS_MAX );
				std::abort();   // because of the critical error, let's exit. TODO: should throw std::runtime_error ?
			}
		}

		inline dynamic_tls_content_head* get_p_content_head( void )
		{
			return reinterpret_cast<dynamic_tls_content_head*>( pthread_getspecific( key_ ) );
		}

		inline void set_p_content_head( dynamic_tls_content_head* p_tl_cnt_head_arg )
		{
			pthread_setspecific( key_, reinterpret_cast<void*>( p_tl_cnt_head_arg ) );
		}

	private:
		pthread_key_t key_;
	};
#endif

#ifdef ALCONCURRENT_CONF_USE_THREAD_LOCAL
	constexpr
#else
#endif
		dynamic_tls_mgr( void )
	  : next_base_idx_( 0 )
	  , p_top_dtls_key_array_( nullptr )
	  , p_top_dtls_content_head_( nullptr )
	  , dtls_content_head_cnt_( 0 )
	  , dtls_key_array_cnt_( 0 )
#ifdef ALCONCURRENT_CONF_USE_THREAD_LOCAL
#else
	  , tl_cnt_head_()
#endif
	{
	}

	~dynamic_tls_mgr()
	{
		is_live_.store( false, std::memory_order_release );

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
		unsigned int           cur_base_idx     = next_base_idx_.fetch_add( ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE, std::memory_order_acq_rel );
		dynamic_tls_key_array* p_new_dtls_ka    = new dynamic_tls_key_array( cur_base_idx );
		dynamic_tls_key_array* p_expect_dtls_ka = p_top_dtls_key_array_.load( std::memory_order_acquire );
		do {   // 置き換えに成功するまでビジーループ
			p_new_dtls_ka->p_next_.store( p_expect_dtls_ka, std::memory_order_release );
		} while ( !p_top_dtls_key_array_.compare_exchange_strong( p_expect_dtls_ka, p_new_dtls_ka ) );
		dtls_key_array_cnt_++;
	}

	dynamic_tls_key* search_key( void* p_param, uintptr_t ( *allocator )( void* p_param ), void ( *deallocator )( uintptr_t p_obj, void* p_param ) )
	{
		dynamic_tls_key*       p_ans         = nullptr;
		dynamic_tls_key_array* p_cur_dtls_ka = p_top_dtls_key_array_.load( std::memory_order_acquire );
		while ( p_cur_dtls_ka != nullptr ) {
			p_ans = p_cur_dtls_ka->allocate_key( p_param, allocator, deallocator );
			if ( p_ans != nullptr ) {
				return p_ans;
			}
			dynamic_tls_key_array* p_nxt_dtls_ka = p_cur_dtls_ka->p_next_.load( std::memory_order_acquire );
			p_cur_dtls_ka                        = p_nxt_dtls_ka;
		}
		return nullptr;
	}

	std::atomic<unsigned int>              next_base_idx_;
	std::atomic<dynamic_tls_key_array*>    p_top_dtls_key_array_;
	std::atomic<dynamic_tls_content_head*> p_top_dtls_content_head_;
	std::atomic<unsigned int>              dtls_content_head_cnt_;
	std::atomic<unsigned int>              dtls_key_array_cnt_;

#ifdef ALCONCURRENT_CONF_USE_THREAD_LOCAL
	static thread_local tl_content_head tl_cnt_head_;
#else
	tl_content_head tl_cnt_head_;
#endif

	static std::atomic<bool> is_live_;
#ifdef ALCONCURRENT_CONF_USE_THREAD_LOCAL
	static dynamic_tls_mgr singleton;
#else
#endif
};

#ifdef ALCONCURRENT_CONF_USE_THREAD_LOCAL
thread_local dynamic_tls_mgr::tl_content_head dynamic_tls_mgr::tl_cnt_head_;
dynamic_tls_mgr                               dynamic_tls_mgr::singleton;
#else
#endif
std::atomic<bool> dynamic_tls_mgr::is_live_( true );

void dynamic_tls_mgr::destructor( void* p_data )
{
	if ( !is_live_.load( std::memory_order_acquire ) ) return;

	std::lock_guard<std::recursive_mutex> lg( dynamic_tls_global_exclusive_control_for_destructions );

	dynamic_tls_content_head* p_destruct_dtls_head = reinterpret_cast<dynamic_tls_content_head*>( p_data );
	p_destruct_dtls_head->call_destructor_and_release_ownership();

	return;
}

inline dynamic_tls_mgr& dynamic_tls_mgr::get_instance( void )
{
#ifdef ALCONCURRENT_CONF_USE_THREAD_LOCAL
#else
	static dynamic_tls_mgr singleton;
#endif
	return singleton;
}

void call_destructor_for_array_and_clear_data( dynamic_tls_key_array* p_key_array_arg, dynamic_tls_content_array* p_content_array_arg )
{
	for ( unsigned int j = 0; j < ALCONCURRENT_CONF_DYNAMIC_TLS_DESTUCT_ITERATE_MAX; j++ ) {
		bool is_finish = true;
		for ( auto& cur_key : ( *p_key_array_arg ) ) {
#ifdef ALCONCURRENT_CONF_ENABLE_INDIVIDUAL_KEY_EXCLUSIVE_ACCESS
			scoped_inout_counter_atomic_int cl( cur_key.acc_cnt_ );
#endif
			if ( cur_key.is_used_.load( std::memory_order_acquire ) != dynamic_tls_key::alloc_stat::USED ) continue;

			auto   it_tls_        = p_content_array_arg->search( &cur_key );
			op_ret destuct_result = it_tls_->destruct_tls_by_thread_exit( &cur_key );
			if ( destuct_result != op_ret::SUCCESS ) {
				is_finish = false;
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
				static std::atomic<int> ec( 0 );
				int                     cc = ec.fetch_add( 1, std::memory_order_acq_rel );
				internal::LogOutput( log_type::WARN, "dynamic_tls_key(%p): backtrace when allocated", &( cur_key ) );
				cur_key.bt_when_allocate_.dump_to_log( log_type::WARN, 'a', cc );
#else
				internal::LogOutput( log_type::WARN, "dynamic_tls_key(%p): if you would like to get previous released backtrace, please compile libalconcurrent with ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE", &( cur_key ) );
#endif
			}
		}
		if ( is_finish ) {
			break;
		}

		std::this_thread::yield();
	}

	return;
}

void dynamic_tls_content_head::call_destructor_and_release_ownership( void )
{
	dynamic_tls_content_array* p_cur_tls_ca = p_head_content_.load( std::memory_order_acquire );
	while ( p_cur_tls_ca != nullptr ) {
		dynamic_tls_key_array* p_pare = dynamic_tls_mgr::get_instance().get_dynamic_tls_key_array( p_cur_tls_ca->base_idx_ );
		if ( p_pare != nullptr ) {
			call_destructor_for_array_and_clear_data( p_pare, p_cur_tls_ca );
		}
		p_cur_tls_ca = p_cur_tls_ca->p_next_;
	}

	ownership_state_.store( cnt_arry_state::NOT_USED, std::memory_order_release );

	return;
}

void* dynamic_tls_content_head::operator new( std::size_t n )   // usual new...(1)
{
	// 	dynamic_tls_content_headは、破棄しないクラスなので、メモリ開放を行わないメモリアロケータを使用する
	void* p_ans = dynamic_tls_key_allocating_only( n );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}

	return p_ans;
}
void dynamic_tls_content_head::operator delete( void* p_mem ) noexcept   // usual delete...(2)
{
	// 	dynamic_tls_content_headは、破棄しないクラスなので、メモリ開放を行わないメモリアロケータを使用してるため、何もしない。
}

void* dynamic_tls_content_head::operator new[]( std::size_t n )   // usual new...(1)
{
	// dynamic_tls_content_headは、破棄しないクラスなので、メモリ開放を行わないメモリアロケータを使用する
	// が、このクラスが配列形式で使用する想定はされていない。
	void* p_ans = dynamic_tls_key_allocating_only( n );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}

	return p_ans;
}
void dynamic_tls_content_head::operator delete[]( void* p_mem ) noexcept   // usual delete...(2)
{
	// 	dynamic_tls_content_headは、破棄しないクラスなので、メモリ開放を行わないメモリアロケータを使用してるため、何もしない。
}

void* dynamic_tls_content_head::operator new( std::size_t n, void* p )   // placement new
{
	// dynamic_tls_content_headのクラスがplacement new形式で使用する想定はされていない。
	return p;
}
void dynamic_tls_content_head::operator delete( void* p, void* p2 ) noexcept   // placement delete...(3)
{
	// 	dynamic_tls_content_headは、破棄しないクラスなので、メモリ開放を行わないメモリアロケータを使用してるため、何もしない。
}

/////////////////////////////////////////////////////////////////////////

void* dynamic_tls_content_array::operator new( std::size_t n )   // usual new...(1)
{
	// 	dynamic_tls_content_arrayは、破棄しないクラスなので、メモリ開放を行わないメモリアロケータを使用する
	void* p_ans = dynamic_tls_key_allocating_only( n );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}

	return p_ans;
}
void dynamic_tls_content_array::operator delete( void* p_mem ) noexcept   // usual delete...(2)
{
	// 	dynamic_tls_content_arrayは、破棄しないクラスなので、メモリ開放を行わないメモリアロケータを使用してるため、何もしない。
}

void* dynamic_tls_content_array::operator new[]( std::size_t n )   // usual new...(1)
{
	// dynamic_tls_content_arrayは、破棄しないクラスなので、メモリ開放を行わないメモリアロケータを使用する
	// が、このクラスが配列形式で使用する想定はされていない。
	void* p_ans = dynamic_tls_key_allocating_only( n );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}

	return p_ans;
}
void dynamic_tls_content_array::operator delete[]( void* p_mem ) noexcept   // usual delete...(2)
{
	// 	dynamic_tls_content_arrayは、破棄しないクラスなので、メモリ開放を行わないメモリアロケータを使用してるため、何もしない。
}

void* dynamic_tls_content_array::operator new( std::size_t n, void* p )   // placement new
{
	// dynamic_tls_content_arrayのクラスがplacement new形式で使用する想定はされていない。
	return p;
}
void dynamic_tls_content_array::operator delete( void* p, void* p2 ) noexcept   // placement delete...(3)
{
	// 	dynamic_tls_content_arrayは、破棄しないクラスなので、メモリ開放を行わないメモリアロケータを使用してるため、何もしない。
}

/////////////////////////////////////////////////////////////////////////
bool dynamic_tls_key_array::release_key( dynamic_tls_key* p_key_arg )
{
	if ( p_key_arg < key_array_ ) return false;
	if ( &( key_array_[ALCONCURRENT_CONF_DYNAMIC_TLS_ARRAY_SIZE] ) <= p_key_arg ) return false;

	std::lock_guard<std::recursive_mutex> lg( dynamic_tls_global_exclusive_control_for_destructions );

	// releaseの作業権の確保
	dynamic_tls_key::alloc_stat expected_alloc_stat = dynamic_tls_key::alloc_stat::USED;
	if ( !p_key_arg->is_used_.compare_exchange_strong( expected_alloc_stat, dynamic_tls_key::alloc_stat::RELEASING ) ) {
		static std::atomic<int> ec( 0 );
		int                     cc = ec.fetch_add( 1, std::memory_order_acq_rel );
		internal::LogOutput( log_type::ERR, "dynamic_tls_key(%p) is fail to release. Current back trace is;", p_key_arg );
		bt_info cur_bt;
		RECORD_BACKTRACE_GET_BACKTRACE( cur_bt );
		cur_bt.dump_to_log( log_type::ERR, 'c', cc );
		if ( expected_alloc_stat == dynamic_tls_key::alloc_stat::NOT_USED ) {
			internal::LogOutput( log_type::ERR, "dynamic_tls_key(%p) is double-released", p_key_arg );
		} else if ( expected_alloc_stat == dynamic_tls_key::alloc_stat::RELEASING ) {
			internal::LogOutput( log_type::ERR, "dynamic_tls_key(%p) is now release key race condition by double-releasing", p_key_arg );
		} else {
			internal::LogOutput( log_type::ERR, "dynamic_tls_key(%p) is now unknown status", p_key_arg );
		}
#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
		internal::LogOutput( log_type::ERR, "dynamic_tls_key(%p): backtrace where this key is allocated", p_key_arg );
		p_key_arg->bt_when_allocate_.dump_to_log( log_type::ERR, 'p', cc );
#else
		internal::LogOutput( log_type::ERR, "dynamic_tls_key(%p): if you would like to get backtrace where this key is allocated, please compile libalconcurrent with ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE", p_key_arg );
#endif
		return false;
	}

	// releaseの作業権が確保できた
#ifdef ALCONCURRENT_CONF_ENABLE_INDIVIDUAL_KEY_EXCLUSIVE_ACCESS
	// アクセススレッドが自分以外に存在しなくなるのを待つ
	while ( p_key_arg->acc_cnt_.load( std::memory_order_acquire ) != 1 ) {
		// 1以外の場合は、ほかのスレッドがアクセス中のため、いったん実行権を解放して、ほかのスレッドの実行を完了させる。
		std::this_thread::yield();
	}
#endif

	// すべての該当するスレッドローカルストレージをクリアする
	dynamic_tls_content_head* p_cur_dtls_c = dynamic_tls_mgr::get_instance().get_top_dynamic_tls_content_head();
	while ( p_cur_dtls_c != nullptr ) {
		op_ret tmp_ret = p_cur_dtls_c->destruct_tls_by_key_release( p_key_arg );
		switch ( tmp_ret ) {
			case op_ret::SUCCESS: {
				// expected result
			} break;
			case op_ret::OUT_OF_RANGE: {
				// expected result. In this case no tls allocate by no access to a tls
				internal::LogOutput( log_type::DEBUG, "dynamic_tls_content_head(%p) has not allocate tls array", p_cur_dtls_c );
			} break;
			case op_ret::INVALID: {
				internal::LogOutput( log_type::DEBUG, "dynamic_tls_content_head(%p) may be NOT_USED", p_cur_dtls_c );
			} break;

			default:
			case op_ret::INVALID_KEY: {
				internal::LogOutput( log_type::ERR, "dynamic_tls_key(%p) is invalid", p_key_arg );
			} break;
			case op_ret::UNEXPECT_ERR: {
				internal::LogOutput( log_type::ERR, "dynamic_tls_key(%p) releasing happens UNEXPECTED ERR", p_key_arg );
			} break;
		}
		p_cur_dtls_c = p_cur_dtls_c->p_next_.load( std::memory_order_acquire );
	}

#ifdef ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
	RECORD_BACKTRACE_INVALIDATE_BACKTRACE( p_key_arg->bt_when_allocate_ );
#endif

	num_of_free_.fetch_add( 1, std::memory_order_acq_rel );

	// release作業が完了したので、未使用状態に変更する。
	p_key_arg->is_used_.store( dynamic_tls_key::alloc_stat::NOT_USED, std::memory_order_release );

	return true;
}

void* dynamic_tls_key_array::operator new( std::size_t n )   // usual new...(1)
{
	// 	dynamic_tls_key_arrayは、破棄しないクラスなので、メモリ開放を行わないメモリアロケータを使用する
	void* p_ans = dynamic_tls_key_allocating_only( n );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}

	return p_ans;
}
void dynamic_tls_key_array::operator delete( void* p_mem ) noexcept   // usual delete...(2)
{
	// 	dynamic_tls_key_arrayは、破棄しないクラスなので、メモリ開放を行わないメモリアロケータを使用してるため、何もしない。
}

void* dynamic_tls_key_array::operator new[]( std::size_t n )   // usual new...(1)
{
	// dynamic_tls_key_arrayは、破棄しないクラスなので、メモリ開放を行わないメモリアロケータを使用する
	// が、このクラスが配列形式で使用する想定はされていない。
	void* p_ans = dynamic_tls_key_allocating_only( n );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}

	return p_ans;
}
void dynamic_tls_key_array::operator delete[]( void* p_mem ) noexcept   // usual delete...(2)
{
	// 	dynamic_tls_key_arrayは、破棄しないクラスなので、メモリ開放を行わないメモリアロケータを使用してるため、何もしない。
}

void* dynamic_tls_key_array::operator new( std::size_t n, void* p )   // placement new
{
	// dynamic_tls_key_arrayのクラスがplacement new形式で使用する想定はされていない。
	return p;
}
void dynamic_tls_key_array::operator delete( void* p, void* p2 ) noexcept   // placement delete...(3)
{
	// 	dynamic_tls_key_arrayは、破棄しないクラスなので、メモリ開放を行わないメモリアロケータを使用してるため、何もしない。
}

////////////////////////////////////////////////////////////////////////////////////////////////////

dynamic_tls_key_scoped_accessor::dynamic_tls_key_scoped_accessor( dynamic_tls_key_t key, op_ret stat, void* p )
  : stat_( stat )
  , key_( key )
  , p_( p )
{
#ifdef ALCONCURRENT_CONF_ENABLE_INDIVIDUAL_KEY_EXCLUSIVE_ACCESS
	if ( stat_ == op_ret::SUCESS ) {
		key->acc_cnt_.fetch_add( 1, std::memory_order_acq_rel );
	}
#endif
}

#ifdef ALCONCURRENT_CONF_ENABLE_INDIVIDUAL_KEY_EXCLUSIVE_ACCESS
dynamic_tls_key_scoped_accessor::~dynamic_tls_key_scoped_accessor()
{
	if ( stat_ == op_ret::SUCESS ) {
		key->acc_cnt_.fetch_sub( 1, std::memory_order_acq_rel );
	}
}
#endif

op_ret dynamic_tls_key_scoped_accessor::set_value( uintptr_t data )
{
	dynamic_tls_content_array::tls_data_and_stat* p_tls = reinterpret_cast<dynamic_tls_content_array::tls_data_and_stat*>( p_ );
	return p_tls->set_tls( key_, data );
}

get_result dynamic_tls_key_scoped_accessor::get_value( void )
{
	dynamic_tls_content_array::tls_data_and_stat* p_tls = reinterpret_cast<dynamic_tls_content_array::tls_data_and_stat*>( p_ );
	return p_tls->get_tls( key_ );
}

////////////////////////////////////////////////////////////////////////////////////////////////////

dynamic_tls_key_t dynamic_tls_key_create( void* p_param, uintptr_t ( *allocator )( void* p_param ), void ( *deallocator )( uintptr_t tls_data, void* p_param ) )
{
	dynamic_tls_key_t p_ans = dynamic_tls_mgr::get_instance().allocate_key( p_param, allocator, deallocator );

	cur_count_of_tls_keys++;

	int cur_max = max_count_of_tls_keys.load( std::memory_order_acquire );
	if ( cur_max < cur_count_of_tls_keys.load( std::memory_order_acquire ) ) {
		max_count_of_tls_keys.compare_exchange_strong( cur_max, cur_count_of_tls_keys.load( std::memory_order_acquire ) );
	}
	return p_ans;
}

void dynamic_tls_key_release( dynamic_tls_key_t key )
{
	if ( key == nullptr ) {
		internal::LogOutput( log_type::WARN, "dynamic_tls_key_release was called with nullptr" );
		return;
	}

#ifdef ALCONCURRENT_CONF_ENABLE_INDIVIDUAL_KEY_EXCLUSIVE_ACCESS
	scoped_inout_counter_atomic_int cl( key->acc_cnt_ );
#endif

	if ( dynamic_tls_mgr::get_instance().release_key( key ) ) {
		cur_count_of_tls_keys--;
	}
	return;
}

op_ret dynamic_tls_setspecific( dynamic_tls_key_t key, uintptr_t tls_data )
{
	if ( key == nullptr ) {
		internal::LogOutput( log_type::ERR, "dynamic_tls_setspecific was called with nullptr" );
		return op_ret::INVALID_KEY;
	}
	if ( key->is_used_.load( std::memory_order_acquire ) != dynamic_tls_key::alloc_stat::USED ) {
		internal::LogOutput( log_type::ERR, "dynamic_tls_key(%p) is not used, why do you call dynamic_tls_setspecific() with %p", key, key );
		return op_ret::INVALID_KEY;
	}

#ifdef ALCONCURRENT_CONF_ENABLE_INDIVIDUAL_KEY_EXCLUSIVE_ACCESS
	scoped_inout_counter_atomic_int cl( key->acc_cnt_ );
#endif

	dynamic_tls_content_head* p_cur_dtls = dynamic_tls_mgr::get_instance().get_current_thread_dynamic_tls_content_head();
	return p_cur_dtls->set_tls_unchk_key( key, tls_data );
}

get_result dynamic_tls_getspecific( dynamic_tls_key_t key )
{
	if ( key == nullptr ) {
		internal::LogOutput( log_type::ERR, "dynamic_tls_getspecific was called with nullptr" );
		return get_result { op_ret::INVALID_KEY, 0 };
	}
	if ( key->is_used_.load( std::memory_order_acquire ) != dynamic_tls_key::alloc_stat::USED ) {
		internal::LogOutput( log_type::ERR, "dynamic_tls_key(%p) is not used, why do you call dynamic_tls_getspecific() with %p", key, key );
		return get_result { op_ret::INVALID_KEY, 0 };
	}

#ifdef ALCONCURRENT_CONF_ENABLE_INDIVIDUAL_KEY_EXCLUSIVE_ACCESS
	scoped_inout_counter_atomic_int cl( key->acc_cnt_ );
#endif

	dynamic_tls_content_head* p_cur_dtls = dynamic_tls_mgr::get_instance().get_current_thread_dynamic_tls_content_head();
	return p_cur_dtls->get_tls_unchk_key( key );
}

dynamic_tls_key_scoped_accessor dynamic_tls_getspecific_accessor( dynamic_tls_key_t key )
{
	if ( key == nullptr ) {
		internal::LogOutput( log_type::ERR, "dynamic_tls_getspecific was called with nullptr" );
		return dynamic_tls_key_scoped_accessor( key, op_ret::INVALID_KEY, 0UL );
	}
	if ( key->is_used_.load( std::memory_order_acquire ) != dynamic_tls_key::alloc_stat::USED ) {
		internal::LogOutput( log_type::ERR, "dynamic_tls_key(%p) is not used, why do you call dynamic_tls_getspecific() with %p", key, key );
		return dynamic_tls_key_scoped_accessor( key, op_ret::INVALID_KEY, 0UL );
	}

#ifdef ALCONCURRENT_CONF_ENABLE_INDIVIDUAL_KEY_EXCLUSIVE_ACCESS
	scoped_inout_counter_atomic_int cl( key->acc_cnt_ );
#endif

	dynamic_tls_content_head* p_cur_dtls = dynamic_tls_mgr::get_instance().get_current_thread_dynamic_tls_content_head();
	auto                      s_ret      = p_cur_dtls->search_tls_unchk_key( key );
	return dynamic_tls_key_scoped_accessor( key, s_ret.stat_, reinterpret_cast<void*>( s_ret.p_data_ ) );
}

dynamic_tls_status_info dynamic_tls_get_status( void )
{
	return dynamic_tls_mgr::get_instance().get_status();
}

/*!
 * @brief	get the number of dynamic thread local memory
 *
 *　不具合解析や、パラメータの最適化作業などで使用する。
 */
int get_num_of_tls_key( void )
{
	return cur_count_of_tls_keys.load( std::memory_order_acquire );
}

/*!
 * @brief	get the max number of dynamic thread local memory
 *
 *　不具合解析や、パラメータの最適化作業などで使用する。
 */
int get_max_num_of_tls_key( void )
{
	return max_count_of_tls_keys.load( std::memory_order_acquire );
}

}   // namespace internal

}   // namespace concurrent
}   // namespace alpha
