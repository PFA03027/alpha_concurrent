/*
 * stm.hpp
 *
 *  Created on: 2020/12/17
 *      Author: alpha
 */

#ifndef SRC_STM_HPP_
#define SRC_STM_HPP_

#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <thread>
using namespace std;

namespace alpha {
namespace concurrent {

/*!
 * @breif	software transactional memory
 */
template <typename T>
class stm {
public:
	template <typename... Args>
	stm( Args... args )
	  : atomic_p_tobj_( new transactional_obj( args... ) )
	  , sp_tobj_( atomic_p_tobj_.load() )
	{
	}

	/*!
	 * @breif	read transaction
	 *
	 * @return	shared pointer to value
	 */
	std::shared_ptr<const T> read_value( void )
	{
		std::shared_ptr<const T> sp_ans = nullptr;

		while ( true ) {
			transactional_obj*                 p_old_tobj         = nullptr;
			std::shared_ptr<transactional_obj> sp_local_keep_tobj = nullptr;

			sp_local_keep_tobj = sp_tobj_;                   // 一時的に、保護。
			p_old_tobj         = sp_local_keep_tobj.get();   // 生ポインタを取得
			sp_ans             = p_old_tobj->read_value();   // 戻り値の準備
			if ( atomic_p_tobj_.compare_exchange_weak( p_old_tobj, sp_local_keep_tobj.get() ) ) {
				// 書き変わっていなければ、完了。
				break;
			}
		}
		return sp_ans;
	}

	/*!
	 * @breif	write transaction
	 *
	 * @return	result of transaction
	 * @retval	true	success to write transaction
	 * @retval	false	fail to write transaction
	 */
	template <typename TFunc>
	void read_modify_write(
		TFunc modify_func   //!< [in]	this functor is the procedure of modificatio. This will be happened multi-call in case of collision.
	)
	{
		state                               expected = state::ACTIVE;
		std::shared_ptr<std::atomic<state>> sp_atomic_state;

		do {
			expected        = state::ACTIVE;
			sp_atomic_state = std::make_shared<std::atomic<state>>( expected );

			std::shared_ptr<transactional_obj> sp_local_keep_tobj;   // 一時的に、保護。
			transactional_obj*                 p_old_tobj;

			while ( true ) {
				sp_local_keep_tobj = sp_tobj_;   // 一時的に、保護。

				p_old_tobj = atomic_p_tobj_.load();

				if ( p_old_tobj == sp_local_keep_tobj.get() ) {
					break;
				}
			}

			std::shared_ptr<const T> sp_read_value = p_old_tobj->read_value();   // 戻り値の準備

			T new_value = modify_func( *sp_read_value );

			transactional_obj* p_new_tobj = new transactional_obj( std::move( sp_read_value ), new_value, sp_atomic_state );

			std::shared_ptr<transactional_obj> sp_new_tobj( p_new_tobj );

			sp_tobj_.reset( p_new_tobj );
			if ( atomic_p_tobj_.compare_exchange_weak( p_old_tobj, p_new_tobj ) ) {
				// 新しい値に更新出来たら終了
				break;
			}

		} while ( !sp_atomic_state->compare_exchange_weak( expected, state::COMMITED ) );
	}

#if 0
	bool is_lock_free(void)
	{
		return std::atomic_is_lock_free(&sp_tobj_);
	}
#endif

private:
	enum class state {
		COMMITED,
		ACTIVE,
		ABORT
	};

	class transactional_obj {
	public:
		template <typename... Args>
		transactional_obj( Args... args )
		  : sp_old_value_( std::make_shared<T>( args... ) )
		  , sp_new_value_( sp_old_value_ )
		  , sp_owner_( std::make_shared<std::atomic<state>>( state::COMMITED ) )
		{
		}

		transactional_obj( std::shared_ptr<const T>&& sp_old_value_arg, const T& w_value, std::shared_ptr<std::atomic<state>> sp_owner_arg )
		  : sp_old_value_( std::move( sp_old_value_arg ) )
		  , sp_new_value_( std::make_shared<T>( w_value ) )
		  , sp_owner_( sp_owner_arg )
		{
		}

		transactional_obj( std::shared_ptr<const T>&& sp_old_value_arg, const T&& w_value, std::shared_ptr<std::atomic<state>> sp_owner_arg )
		  : sp_old_value_( std::move( sp_old_value_arg ) )
		  , sp_new_value_( std::make_shared<T>( std::move( w_value ) ) )
		  , sp_owner_( sp_owner_arg )
		{
		}

		std::shared_ptr<const T> read_value( void )
		{
			while ( true ) {
				state owners_status = sp_owner_->load();
				if ( owners_status == state::COMMITED ) {
					return sp_new_value_;
				} else if ( owners_status == state::ABORT ) {
					return sp_old_value_;
				} else if ( owners_status == state::ACTIVE ) {
					// 何かやっているようだから、中断を仕掛けて、踏み越えた後、もう一度やり直す。
					sp_owner_->compare_exchange_weak( owners_status, state::ABORT );
					continue;
				} else {
					// メモリ破壊等による回復不可能なエラー。
					assert( owners_status == state::ACTIVE );
				}
			}
		}

		std::shared_ptr<const T> const            sp_old_value_;
		std::shared_ptr<const T> const            sp_new_value_;
		std::shared_ptr<std::atomic<state>> const sp_owner_;
	};

	std::atomic<transactional_obj*>    atomic_p_tobj_;   // need to be accessed by atomic style.
	std::shared_ptr<transactional_obj> sp_tobj_;         // need to be accessed by atomic style.
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_STM_HPP_ */
