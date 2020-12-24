/*
 * stm.hpp
 *
 *  Created on: 2020/12/17
 *      Author: alpha
 */

#ifndef SRC_STM_HPP_
#define SRC_STM_HPP_

#include <cassert>
#include <atomic>
#include <memory>
#include <iostream>
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
	{
	}

	/*!
	 * @breif	read transaction
	 *
	 * @return	shared pointer to value
	 */
	std::shared_ptr<const T> read_value( void ) const
	{
		transactional_obj*	p_old_tobj = atomic_p_tobj_.load();
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

			transactional_obj*	p_old_tobj = nullptr;

			while ( true ) {
				p_old_tobj = atomic_p_tobj_.load();
				auto sp_read_value = p_old_tobj->read_value();

				transactional_obj*	p_new_tobj = new transactional_obj( std::move( sp_read_value ), modify_func( *sp_read_value ), sp_atomic_state );

				if ( atomic_p_tobj_.compare_exchange_weak( p_old_tobj, p_new_tobj ) ) {
					// How to avoid memory leak of old tobj pointer.
					break;
				} else {
					delete p_new_tobj;
				}
			}
		} while ( !sp_atomic_state->compare_exchange_weak( expected, state::COMMITED ) );
	}

#if	0
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
		  : sp_old_value_( std::move(sp_old_value_arg) )
		  , sp_new_value_( std::make_shared<T>( w_value ) )
		  , sp_owner_( sp_owner_arg )
		{
		}

		transactional_obj( std::shared_ptr<const T>&& sp_old_value_arg, const T&& w_value, std::shared_ptr<std::atomic<state>> sp_owner_arg )
		  : sp_old_value_( std::move(sp_old_value_arg) )
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
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_STM_HPP_ */
