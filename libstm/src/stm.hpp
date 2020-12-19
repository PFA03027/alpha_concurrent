/*
 * stm.hpp
 *
 *  Created on: 2020/12/17
 *      Author: alpha
 */

#ifndef SRC_STM_HPP_
#define SRC_STM_HPP_

#include <atomic>
#include <memory>

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
	  : sp_tobj_( std::make_shared<transactional_obj>( args... ) )
	{
	}

	/*!
	 * @breif	read transaction
	 *
	 * @return	shared pointer to value
	 */
	std::shared_ptr<const T> read_value( void ) const
	{
		std::shared_ptr<transactional_obj> sp_old_tobj = std::atomic_load( &sp_tobj_ );
		return sp_old_tobj->read_value();
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

			std::shared_ptr<transactional_obj> sp_old_tobj = std::atomic_load( &sp_tobj_ );

			while ( true ) {
				auto sp_read_value = sp_old_tobj->read_value();

				std::shared_ptr<transactional_obj> sp_new_tobj = std::make_shared<transactional_obj>( std::move( sp_read_value ), modify_func( *sp_read_value ), sp_atomic_state );

				if ( std::atomic_compare_exchange_weak( &sp_tobj_, &sp_old_tobj, std::move( sp_new_tobj ) ) ) {
					break;
				}
			}
		} while ( !sp_atomic_state->compare_exchange_weak( expected, state::COMMITED ) );
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

		transactional_obj( std::shared_ptr<const T> sp_old_value_arg, const T& w_value, std::shared_ptr<std::atomic<state>> sp_owner_arg )
		  : sp_old_value_( sp_old_value_arg )
		  , sp_new_value_( std::make_shared<T>( w_value ) )
		  , sp_owner_( sp_owner_arg )
		{
		}

		transactional_obj( std::shared_ptr<const T> sp_old_value_arg, const T&& w_value, std::shared_ptr<std::atomic<state>> sp_owner_arg )
		  : sp_old_value_( sp_old_value_arg )
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
					exit( 1 );
				}
			}
		}

		std::shared_ptr<const T> const            sp_old_value_;
		std::shared_ptr<const T> const            sp_new_value_;
		std::shared_ptr<std::atomic<state>> const sp_owner_;
	};

	std::shared_ptr<transactional_obj> sp_tobj_;   // need to be accessed by atomic style.
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_STM_HPP_ */
