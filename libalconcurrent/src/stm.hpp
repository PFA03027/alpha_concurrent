/*
 * stm.hpp
 *
 *  Created on: 2020/12/17
 *      Author: Teruaki Ata
 *
 * Copyright (C) 2020 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef SRC_STM_HPP_
#define SRC_STM_HPP_

#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>

#include "hazard_ptr.hpp"

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
	{
	}

	/*!
	 * @breif	read transaction
	 *
	 * @return	shared pointer to value
	 */
	std::shared_ptr<const T> read_value( void ) const
	{
		transactional_obj* p_old_tobj = atomic_p_tobj_.load();
		return p_old_tobj->read_value();
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

			transactional_obj*                       p_old_tobj = atomic_p_tobj_.load();
			hazard_ptr_scoped_ref<transactional_obj> hzrd_scp( tobj_hazard_ptr );

			while ( true ) {

				while ( true ) {
					tobj_hazard_ptr.regist_ptr_as_hazard_ptr( p_old_tobj );
					if ( atomic_p_tobj_.compare_exchange_weak( p_old_tobj, p_old_tobj ) ) {
						break;
					}
				}
				// ここまでで、ハザードポインターに登録したポインターが有効であることであることの確認が取れた。
				// あわせて、ハザードポインターに登録できたことで、p_old_tobjが指し示す先のメモリオブジェクトは、削除されていないことが保障できた。

				// 以降、安心してアクセス開始
				auto sp_read_value = p_old_tobj->read_value();

				transactional_obj* p_new_tobj = new transactional_obj( std::move( sp_read_value ), modify_func( *sp_read_value ), sp_atomic_state );

				if ( atomic_p_tobj_.compare_exchange_weak( p_old_tobj, p_new_tobj ) ) {
					// p_old_tobjに対し、置き換えが成功＝削除権を獲得
					tobj_hazard_ptr.move_hazard_ptr_to_del_list();
					break;
				} else {
					// 更新失敗。準備していた新しい値の為のオブジェクトは削除する。
					delete p_new_tobj;
				}
			}
		} while ( !sp_atomic_state->compare_exchange_weak( expected, state::COMMITED ) );
	}

	static std::tuple<int, int> debug_get_glist_size( void )
	{
		return hazard_ptr<transactional_obj>::debug_get_glist_size();
	}

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

	std::atomic<transactional_obj*> atomic_p_tobj_;   // need to be accessed by atomic style.
													  //	static thread_local hazard_ptr<transactional_obj> tobj_hazard_ptr;
	hazard_ptr<transactional_obj> tobj_hazard_ptr;
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_STM_HPP_ */
