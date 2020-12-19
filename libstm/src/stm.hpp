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
#include <thread>
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
		transactional_obj*	p_old_tobj = nullptr;
		while ( true ) {
			p_old_tobj = atomic_p_tobj_.load();
			if(p_old_tobj == nullptr) {
				std::this_thread::yield();
				continue;
			}
			if ( atomic_p_tobj_.compare_exchange_weak( p_old_tobj, nullptr ) ) {
				break;
			}
			std::this_thread::yield();
		}
		// ここに来た時点で、p_old_tobjは、このインスタンスのみが所有している。
		std::shared_ptr<const T>	sp_ans = p_old_tobj->read_value();
		atomic_p_tobj_.store( p_old_tobj );
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
#if	0
		// 単体でのリードモディファイライトの場合は、下記でも動くが、最初から、COMMITEDでよい。
		state                               expected = state::ACTIVE;
		std::shared_ptr<std::atomic<state>> sp_atomic_state;

		do {
			expected        = state::ACTIVE;
			sp_atomic_state = std::make_shared<std::atomic<state>>( expected );

			transactional_obj*	p_old_tobj = nullptr;

			while ( true ) {
				p_old_tobj = atomic_p_tobj_.load();
				if(p_old_tobj == nullptr) {
					std::this_thread::yield();
					continue;
				}
				if ( atomic_p_tobj_.compare_exchange_weak( p_old_tobj, nullptr ) ) {
					break;
				}
				std::this_thread::yield();
			}
			// ここに来た時点で、p_old_tobjは、このインスタンスのみが所有している。
			auto sp_read_value = p_old_tobj->read_value();

			transactional_obj*	p_new_tobj = new transactional_obj( std::move( sp_read_value ), modify_func( *sp_read_value ), sp_atomic_state );

			atomic_p_tobj_.store( p_new_tobj );
			delete p_old_tobj;
		} while ( !sp_atomic_state->compare_exchange_weak( expected, state::COMMITED ) );
#else
		std::shared_ptr<std::atomic<state>> sp_atomic_state = std::make_shared<std::atomic<state>>( state::COMMITED );

		transactional_obj*	p_old_tobj = nullptr;

		// これで、メモリリークは発生しないが、yield()している時点でリアルタイム処理には適用できない。
		// なぜならば、優先度が高い処理が低い処理を踏み越えていけないため、いくら実行権を放棄しても優先度の低い処理に実行権が回らないので（シングルCPUの場合）、意味がない
		// よって、TSSにしか適用できないが、TSSなら、わざわざロックフリーアルゴリズムを適用するまでもなく、ロック方式でよい。。。
		while ( true ) {
			p_old_tobj = atomic_p_tobj_.load();
			if(p_old_tobj == nullptr) {
				std::this_thread::yield();
				continue;
			}
			if ( atomic_p_tobj_.compare_exchange_weak( p_old_tobj, nullptr ) ) {
				break;
			}
			std::this_thread::yield();
		}
		// ここに来た時点で、p_old_tobjは、このインスタンスのみが所有している。
		auto sp_read_value = p_old_tobj->read_value();

		transactional_obj*	p_new_tobj = new transactional_obj( std::move( sp_read_value ), modify_func( *sp_read_value ), sp_atomic_state );

		atomic_p_tobj_.store( p_new_tobj );
		delete p_old_tobj;
#endif
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

	mutable std::atomic<transactional_obj*> atomic_p_tobj_;   // need to be accessed by atomic style.
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_STM_HPP_ */
