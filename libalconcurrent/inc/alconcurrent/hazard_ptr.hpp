/*!
 * @file	hazard_ptr.hpp
 * @brief	hazard pointer
 * @author	Teruaki Ata
 * @date	Created on 2020/12/24
 * @details
 *
 *  US Patent of Hazard pointer algrithm: US20040107227A1 is now abandoned.
 *  https://patents.google.com/patent/US20040107227
 *
 * Copyright (C) 2020 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef ALCONCCURRENT_INC_HAZARD_PTR_HPP_
#define ALCONCCURRENT_INC_HAZARD_PTR_HPP_

#include <pthread.h>

#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>
#include <type_traits>

#include "conf_logger.hpp"
#include "internal/hazard_ptr_internal.hpp"

namespace alpha {
namespace concurrent {

template <typename T>
class hazard_ptr_handler;

template <typename T>
class hazard_ptr_w_mark_handler;

template <typename T>
class hazard_ptr {
public:
	using element_type = T;
	using pointer      = T*;

	hazard_ptr( pointer p_arg = nullptr )
	  : p_( p_arg )
	  , os_( internal::hazard_ptr_mgr::AssignHazardPtrSlot( p_arg ) )
	{
	}
	~hazard_ptr() = default;
	ALCC_INTERNAL_CONSTEXPR_CONSTRUCTOR_BODY hazard_ptr( const hazard_ptr& src )
	  : p_( src.p_ )
	  , os_( internal::hazard_ptr_mgr::AssignHazardPtrSlot( src.p_ ) )
	{
	}
	ALCC_INTERNAL_CONSTEXPR_CONSTRUCTOR_BODY hazard_ptr( hazard_ptr&& src )
	  : p_( src.p_ )
	  , os_( std::move( src.os_ ) )
	{
		src.p_ = nullptr;
	}
	hazard_ptr& operator=( const hazard_ptr& src )
	{
		if ( this == &src ) return *this;

		if ( os_ == nullptr ) {
			os_ = internal::hazard_ptr_mgr::AssignHazardPtrSlot( nullptr );
		}

		p_ = src.p_;
		os_->store( src.os_->load( std::memory_order_acquire ), internal::hzrd_slot_memory_order_for_store );
		// nullptrが特別扱いされるため、src側がnullptrだった場合に自然に対応できるようにload()を使う。
		// p == nullptrで判定しても良いが、どちら効率的かは不明。。。

		return *this;
	}
	hazard_ptr& operator=( hazard_ptr&& src )
	{
		if ( this == &src ) return *this;

		swap( src );

		return *this;
	}

	void swap( hazard_ptr& src )
	{
		auto p_tmp = p_;
		p_         = src.p_;
		src.p_     = p_tmp;
		os_.swap( src.os_ );
	}

	template <bool IsVoid = std::is_same<T, void>::value, typename std::enable_if<!IsVoid>::type* = nullptr>
	auto operator->() const noexcept -> typename std::add_pointer<typename std::enable_if<!IsVoid, T>::type>::type
	{
		return p_;
	}

	template <bool IsVoid = std::is_same<T, void>::value, typename std::enable_if<!IsVoid>::type* = nullptr>
	auto operator*() const noexcept -> typename std::add_lvalue_reference<typename std::enable_if<!IsVoid, T>::type>::type
	{
		return *p_;
	}

	T* reset( pointer p_arg = nullptr ) noexcept
	{
		T* p_ans = p_;
		store( p_arg );
		return p_ans;
	}

	T* get() const noexcept
	{
		return p_;
	}

	template <typename CAST_T>
	auto get_pointer_by_static_cast( void ) const -> typename std::conditional<std::is_const<T>::value, typename std::add_const<CAST_T>::type*, CAST_T*>::type
	{
#ifdef ALCONCURRENT_CONF_REPLACE_STATIC_CAST_OF_ZDPTR_TO_DYNAMIC_CAST
		return dynamic_cast<typename std::conditional<std::is_const<T>, typename std::remove_const<CAST_T>::type*, CAST_T*>::type>( p_ );
#else
		return static_cast<typename std::conditional<std::is_const<T>::value, typename std::remove_const<CAST_T>::type*, CAST_T*>::type>( p_ );
#endif
	}

	template <typename CAST_T>
	auto get_pointer_by_dynamic_cast( void ) const -> typename std::conditional<std::is_const<T>::value, typename std::add_const<CAST_T>::type*, CAST_T*>::type
	{
		return dynamic_cast<typename std::conditional<std::is_const<T>::value, typename std::remove_const<CAST_T>::type*, CAST_T*>::type>( p_ );
	}

	void store( pointer p_arg ) noexcept
	{
#if defined( ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR ) || defined( ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION )
		if ( os_ == nullptr ) {
			internal::LogOutput( log_type::ERR, "slot of hazard pointer in hazard_ptr is nullptr, p_=%p", p_ );
			bt_info cur_bt;
			RECORD_BACKTRACE_GET_BACKTRACE( cur_bt );
			cur_bt.dump_to_log( log_type::ERR, 'd', 1 );
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION
			throw std::logic_error( "slot of hazard pointer in hazard_ptr is nullptr" );
#else
#endif
		}
#endif

		p_ = p_arg;
		reflect_from_p();
	}

private:
	void reflect_from_p( void ) noexcept
	{
#if defined( ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR ) || defined( ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION )
		if ( os_ == nullptr ) {
			internal::LogOutput( log_type::ERR, "slot of hazard pointer in hazard_ptr is nullptr, p_=%p", p_ );
			bt_info cur_bt;
			RECORD_BACKTRACE_GET_BACKTRACE( cur_bt );
			cur_bt.dump_to_log( log_type::ERR, 'd', 1 );
#ifdef ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION
			throw std::logic_error( "slot of hazard pointer in hazard_ptr is nullptr" );
#else
#endif
		}
#endif

		os_->store( ( p_ == nullptr ) ? reinterpret_cast<pointer>( static_cast<std::uintptr_t>( 1U ) ) : p_, internal::hzrd_slot_memory_order_for_store );
	}

	pointer                         p_;
	internal::hzrd_slot_ownership_t os_;

	friend class hazard_ptr_handler<T>;
	friend class hazard_ptr_w_mark_handler<T>;

	template <class T1, class T2>
	friend constexpr bool operator==( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator!=( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator<( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator<=( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator>( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator>=( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept;

	template <class T1>
	friend constexpr bool operator==( const hazard_ptr<T1>& a, std::nullptr_t ) noexcept;
	template <class T1>
	friend constexpr bool operator!=( const hazard_ptr<T1>& a, std::nullptr_t ) noexcept;
	template <class T1>
	friend constexpr bool operator==( std::nullptr_t, const hazard_ptr<T1>& a ) noexcept;
	template <class T1>
	friend constexpr bool operator!=( std::nullptr_t, const hazard_ptr<T1>& a ) noexcept;

	template <class T1, class T2>
	friend constexpr bool operator==( const hazard_ptr<T1>& a, const T2* b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator!=( const hazard_ptr<T1>& a, const T2* b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator<( const hazard_ptr<T1>& a, const T2* b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator<=( const hazard_ptr<T1>& a, const T2* b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator>( const hazard_ptr<T1>& a, const T2* b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator>=( const hazard_ptr<T1>& a, const T2* b ) noexcept;

	template <class T1, class T2>
	friend constexpr bool operator==( const T1* a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator!=( const T1* a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator<( const T1* a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator<=( const T1* a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator>( const T1* a, const hazard_ptr<T2>& b ) noexcept;
	template <class T1, class T2>
	friend constexpr bool operator>=( const T1* a, const hazard_ptr<T2>& b ) noexcept;
};

template <class T1, class T2>
constexpr bool operator==( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept
{
	return reinterpret_cast<const void*>( a.p_ ) == reinterpret_cast<const void*>( b.p_ );
}

template <class T1, class T2>
constexpr bool operator!=( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept
{
	return reinterpret_cast<const void*>( a.p_ ) != reinterpret_cast<const void*>( b.p_ );
}

template <class T1, class T2>
constexpr bool operator<( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept
{
	return reinterpret_cast<const void*>( a.p_ ) < reinterpret_cast<const void*>( b.p_ );
}

template <class T1, class T2>
constexpr bool operator<=( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept
{
	return reinterpret_cast<const void*>( a.p_ ) <= reinterpret_cast<const void*>( b.p_ );
}

template <class T1, class T2>
constexpr bool operator>( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept
{
	return reinterpret_cast<const void*>( a.p_ ) > reinterpret_cast<const void*>( b.p_ );
}

template <class T1, class T2>
constexpr bool operator>=( const hazard_ptr<T1>& a, const hazard_ptr<T2>& b ) noexcept
{
	return reinterpret_cast<const void*>( a.p_ ) >= reinterpret_cast<const void*>( b.p_ );
}

template <class T1>
constexpr bool operator==( const hazard_ptr<T1>& a, std::nullptr_t ) noexcept
{
	return a.p_ == nullptr;
}

template <class T1>
constexpr bool operator!=( const hazard_ptr<T1>& a, std::nullptr_t ) noexcept
{
	return a.p_ != nullptr;
}

template <class T1>
constexpr bool operator==( std::nullptr_t, const hazard_ptr<T1>& a ) noexcept
{
	return a.p_ == nullptr;
}

template <class T1>
constexpr bool operator!=( std::nullptr_t, const hazard_ptr<T1>& a ) noexcept
{
	return a.p_ != nullptr;
}

template <class T1, class T2>
constexpr bool operator==( const hazard_ptr<T1>& a, const T2* b ) noexcept
{
	return a.p_ == b;
}
template <class T1, class T2>
constexpr bool operator!=( const hazard_ptr<T1>& a, const T2* b ) noexcept
{
	return a.p_ != b;
}
template <class T1, class T2>
constexpr bool operator<( const hazard_ptr<T1>& a, const T2* b ) noexcept
{
	return a.p_ < b;
}
template <class T1, class T2>
constexpr bool operator<=( const hazard_ptr<T1>& a, const T2* b ) noexcept
{
	return a.p_ <= b;
}
template <class T1, class T2>
constexpr bool operator>( const hazard_ptr<T1>& a, const T2* b ) noexcept
{
	return a.p_ > b;
}
template <class T1, class T2>
constexpr bool operator>=( const hazard_ptr<T1>& a, const T2* b ) noexcept
{
	return a.p_ >= b;
}

template <class T1, class T2>
constexpr bool operator==( const T1* a, const hazard_ptr<T2>& b ) noexcept
{
	return a == b.p_;
}
template <class T1, class T2>
constexpr bool operator!=( const T1* a, const hazard_ptr<T2>& b ) noexcept
{
	return a != b.p_;
}
template <class T1, class T2>
constexpr bool operator<( const T1* a, const hazard_ptr<T2>& b ) noexcept
{
	return a < b.p_;
}
template <class T1, class T2>
constexpr bool operator<=( const T1* a, const hazard_ptr<T2>& b ) noexcept
{
	return a <= b.p_;
}
template <class T1, class T2>
constexpr bool operator>( const T1* a, const hazard_ptr<T2>& b ) noexcept
{
	return a > b.p_;
}
template <class T1, class T2>
constexpr bool operator>=( const T1* a, const hazard_ptr<T2>& b ) noexcept
{
	return a >= b.p_;
}

template <typename T>
class hazard_ptr_handler {
public:
	using element_type         = T;
	using pointer              = T*;
	using hazard_pointer       = hazard_ptr<T>;
	using hazard_const_pointer = hazard_ptr<typename std::add_const<T>::type>;

	constexpr hazard_ptr_handler( void ) noexcept
	  : ap_target_p_( nullptr )
	{
	}
	explicit constexpr hazard_ptr_handler( T* p_desired ) noexcept
	  : ap_target_p_( p_desired )
	{
	}
	constexpr hazard_ptr_handler( const hazard_ptr_handler& src ) noexcept
	  : ap_target_p_( src.ap_target_p_.load( std::memory_order_acquire ) )
	{
	}
	hazard_ptr_handler( hazard_ptr_handler&& src ) noexcept
	  : ap_target_p_()
	{
		pointer p_expect = src.ap_target_p_.load( std::memory_order_acquire );
		do {
			ap_target_p_.store( p_expect, std::memory_order_release );
		} while ( !src.ap_target_p_.compare_exchange_weak( p_expect, nullptr, std::memory_order_acq_rel, std::memory_order_acquire ) );
	}
	hazard_ptr_handler& operator=( const hazard_ptr_handler& src ) noexcept
	{
		if ( this == &src ) return *this;

		ap_target_p_.store( src.ap_target_p_.load( std::memory_order_acquire ), std::memory_order_release );

		return *this;
	}
	hazard_ptr_handler& operator=( hazard_ptr_handler&& src ) noexcept
	{
		if ( this == &src ) return *this;

		pointer p_expect = src.ap_target_p_.load( std::memory_order_acquire );
		do {
			ap_target_p_.store( p_expect, std::memory_order_release );
		} while ( !src.ap_target_p_.compare_exchange_weak( p_expect, nullptr, std::memory_order_acq_rel, std::memory_order_acquire ) );

		return *this;
	}

	/**
	 * @brief hpと自身のポインタの一致をチェックする
	 *
	 * hpと自身が保持するポインタが一致した場合、真を返す。
	 * hpと自身が保持するポインタが不一致の場合、偽を返すとともに、自身が保持するポインタでhpを更新する。
	 *
	 * @param hp 検証対象のハザードポインタ
	 * @return true hpと自身が保持するポインタが一致
	 * @return false hpと自身が保持するポインタが不一致。かつ、自身が保持するポインタでhpを更新した。
	 */
	bool verify_exchange( hazard_pointer& hp ) const noexcept
	{
		pointer p_expect = ap_target_p_.load( std::memory_order_acquire );
		bool    ret      = ( p_expect == hp );
		if ( !ret ) {
			hp.store( p_expect );
		}
		return ret;
	}

	hazard_pointer get_to_verify_exchange( void ) const noexcept
	{
		return hazard_pointer( ap_target_p_.load( std::memory_order_acquire ) );
	}

	void reuse_to_verify_exchange( hazard_pointer& hp_reuse ) const noexcept
	{
		pointer p_expect = ap_target_p_.load( std::memory_order_acquire );
		hp_reuse.store( p_expect );

		return;
	}

	pointer load( std::memory_order order = std::memory_order_acquire ) const noexcept
	{
		return ap_target_p_.load( order );
	}

	void store( pointer p_desired, std::memory_order order = std::memory_order_release ) noexcept
	{
		ap_target_p_.store( p_desired, order );
	}

	inline bool compare_exchange_weak( pointer&          expected,
	                                   pointer           desired,
	                                   std::memory_order success = std::memory_order_acq_rel,
	                                   std::memory_order failure = std::memory_order_acquire ) noexcept
	{
		return ap_target_p_.compare_exchange_weak( expected, desired, success, failure );
	}

	inline bool compare_exchange_strong( pointer&          expected,
	                                     pointer           desired,
	                                     std::memory_order success = std::memory_order_acq_rel,
	                                     std::memory_order failure = std::memory_order_acquire ) noexcept
	{
		return ap_target_p_.compare_exchange_strong( expected, desired, success, failure );
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_weak を使用する。
	 *
	 * 現在の値と expected_hzd_ptr が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、
	 * そうでなければ failure メモリオーダーで expected_hzd_ptr を現在の値で置き換える。
	 * また、failureケースの置き換え時に自身の現在の値との一致を再確認し、再確認成功後、関数が完了する。
	 * そのため、compare_exchange_strong_to_verify_exchangeよりも処理が重い。
	 *
	 * @param expected_hzd_ptr 自身が保持していると期待されるハザードポインタへの参照
	 * @param desired CAS操作で置き換える新たなポインタ値
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @param failure CAS操作失敗時に使用するメモリオーダー
	 * @return true CAS操作に成功。また、expected_hzd_ptrは呼び出し前の状態を維持している。
	 * @return false CAS操作に失敗。自身が保持するあらなたポインタでexpected_hzd_ptrを置き換えられている。
	 */
	inline bool compare_exchange_weak( hazard_pointer&   expected_hzd_ptr,
	                                   pointer           desired,
	                                   std::memory_order success = std::memory_order_acq_rel,
	                                   std::memory_order failure = std::memory_order_acquire ) noexcept
	{
		bool ret = ap_target_p_.compare_exchange_weak( expected_hzd_ptr.p_, desired, success, failure );
		if ( !ret ) {
			// 置き換えに失敗した場合、新たな値が、expected_hzd_ptr.p_に設定されている。
			// しかし、まだハザードポインタ用のスロットには反映されていないため、更新を行う。
			do {
				expected_hzd_ptr.reflect_from_p();
			} while ( !ap_target_p_.compare_exchange_weak( expected_hzd_ptr.p_, expected_hzd_ptr.p_, success, failure ) );
		}
		return ret;
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_strong を使用する。
	 *
	 * 現在の値と expected_hzd_ptr が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、
	 * そうでなければ failure メモリオーダーで expected_hzd_ptr を現在の値で置き換える。
	 * また、failureケースの置き換え時に自身の現在の値との一致を再確認し、再確認成功後、関数が完了する。
	 * そのため、compare_exchange_strong_to_verify_exchangeよりも処理が重い。
	 *
	 * @param expected_hzd_ptr 自身が保持していると期待されるハザードポインタへの参照
	 * @param desired CAS操作で置き換える新たなポインタ値
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @param failure CAS操作失敗時に使用するメモリオーダー
	 * @return true CAS操作に成功。また、expected_hzd_ptrは呼び出し前の状態を維持している。
	 * @return false CAS操作に失敗。自身が保持するあらなたポインタでexpected_hzd_ptrを置き換えられている。
	 */
	inline bool compare_exchange_strong( hazard_pointer&   expected_hzd_ptr,
	                                     pointer           desired,
	                                     std::memory_order success = std::memory_order_acq_rel,
	                                     std::memory_order failure = std::memory_order_acquire ) noexcept
	{
		bool ret = ap_target_p_.compare_exchange_strong( expected_hzd_ptr.p_, desired, success, failure );
		if ( !ret ) {
			// 置き換えに失敗した場合、新たな値が、expected_hzd_ptr.p_に設定されている。
			// しかし、まだハザードポインタ用のスロットには反映されていないため、更新を行う。
			do {
				expected_hzd_ptr.reflect_from_p();
			} while ( !ap_target_p_.compare_exchange_strong( expected_hzd_ptr.p_, expected_hzd_ptr.p_, success, failure ) );
		}
		return ret;
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_weak を使用する。
	 *
	 * 現在の値と expected_hzd_ptr が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、
	 *
	 * なお、expected_hzd_ptr 使用できない状態となるため、expected_hzd_ptr を使い捨てにする場合に使用するI/F。
	 *
	 * @param expected_hzd_ptr 自身が保持していると期待されるハザードポインタへの参照。呼び出し完了後、hazard_ptrとして使用できない。
	 * @param desired CAS操作で置き換える新たなポインタ値
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @return true CAS操作に成功。
	 * @return false CAS操作に失敗。
	 */
	inline bool compare_exchange_weak( hazard_pointer&&  expected_hzd_ptr,
	                                   pointer           desired,
	                                   std::memory_order success = std::memory_order_acq_rel ) noexcept
	{
		bool ret = ap_target_p_.compare_exchange_weak( expected_hzd_ptr.p_, desired, success, std::memory_order_relaxed );
		return ret;
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_strong を使用する。
	 *
	 * 現在の値と expected_hzd_ptr が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、
	 *
	 * なお、expected_hzd_ptr 使用できない状態となるため、expected_hzd_ptr を使い捨てにする場合に使用するI/F。
	 *
	 * @param expected_hzd_ptr 自身が保持していると期待されるハザードポインタへの参照。呼び出し完了後、hazard_ptrとして使用できない。
	 * @param desired CAS操作で置き換える新たなポインタ値
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @return true CAS操作に成功。
	 * @return false CAS操作に失敗。
	 */
	inline bool compare_exchange_strong( hazard_pointer&&  expected_hzd_ptr,
	                                     pointer           desired,
	                                     std::memory_order success = std::memory_order_acq_rel ) noexcept
	{
		bool ret = ap_target_p_.compare_exchange_strong( expected_hzd_ptr.p_, desired, success, std::memory_order_relaxed );
		return ret;
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_weak を使用する。
	 *
	 * 現在の値と expected_hzd_ptr が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、expected_hzd_ptr に、desired を反映する。
	 * そうでなければ failure メモリオーダーで expected_hzd_ptr を現在の値で置き換える。
	 *
	 * @param expected_hzd_ptr 自身が保持していると期待されるハザードポインタへの参照
	 * @param desired CAS操作で置き換える新たなポインタ値
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @param failure CAS操作失敗時に使用するメモリオーダー
	 * @return true CAS操作に成功。expected_hzd_ptr は、desired で置き換えられている。
	 * @return false CAS操作に失敗。自身が保持する現在のポインタでexpected_hzd_ptrを置き換えられている。
	 */
	inline bool compare_exchange_weak_to_verify_exchange1( hazard_pointer&   expected_hzd_ptr,
	                                                       pointer           desired,
	                                                       std::memory_order success = std::memory_order_acq_rel,
	                                                       std::memory_order failure = std::memory_order_acquire ) noexcept

	{
		bool ret = ap_target_p_.compare_exchange_weak( expected_hzd_ptr.p_, desired, success, failure );
		if ( ret ) {
			// 置き換えに成功した場合、desiredがまだハザードポインタ用のスロットには反映されていないため、更新を行う。
			expected_hzd_ptr.store( desired );
		} else {
			// 置き換えに失敗した場合、新たな値が、expected_hzd_ptr.p_に設定されている。
			// しかし、まだハザードポインタ用のスロットには反映されていないため、更新を行う。
			expected_hzd_ptr.reflect_from_p();
		}
		return ret;
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_weak を使用する。
	 *
	 * 現在の値と expected_hzd_ptr が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、
	 * そうでなければ failure メモリオーダーで expected_hzd_ptr を現在の値で置き換える
	 *
	 * @param expected_hzd_ptr 自身が保持していると期待されるハザードポインタへの参照
	 * @param desired CAS操作で置き換える新たなポインタ値
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @param failure CAS操作失敗時に使用するメモリオーダー
	 * @return true CAS操作に成功。また、expected_hzd_ptrは呼び出し前の状態を維持している。
	 * @return false CAS操作に失敗。自身が保持するあらなたポインタでexpected_hzd_ptrを置き換えられている。
	 */
	inline bool compare_exchange_weak_to_verify_exchange2( hazard_pointer&   expected_hzd_ptr,
	                                                       pointer           desired,
	                                                       std::memory_order success = std::memory_order_acq_rel,
	                                                       std::memory_order failure = std::memory_order_acquire ) noexcept
	{
		bool ret = ap_target_p_.compare_exchange_weak( expected_hzd_ptr.p_, desired, success, failure );
		if ( !ret ) {
			// 置き換えに失敗した場合、新たな値が、expected_hzd_ptr.p_に設定されている。
			// しかし、まだハザードポインタ用のスロットには反映されていないため、更新を行う。
			expected_hzd_ptr.reflect_from_p();
		}
		return ret;
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_strong を使用する。
	 *
	 * 現在の値と expected_hzd_ptr が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、expected_hzd_ptr に、desired を反映する。
	 * そうでなければ failure メモリオーダーで expected_hzd_ptr を現在の値で置き換える。
	 *
	 * @param expected_hzd_ptr 自身が保持していると期待されるハザードポインタへの参照
	 * @param desired CAS操作で置き換える新たなポインタ値
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @param failure CAS操作失敗時に使用するメモリオーダー
	 * @return true CAS操作に成功。expected_hzd_ptr は、desired で置き換えられている。
	 * @return false CAS操作に失敗。自身が保持する現在のポインタでexpected_hzd_ptrを置き換えられている。
	 */
	inline bool compare_exchange_strong_to_verify_exchange1( hazard_pointer&   expected_hzd_ptr,
	                                                         pointer           desired,
	                                                         std::memory_order success = std::memory_order_acq_rel,
	                                                         std::memory_order failure = std::memory_order_acquire ) noexcept
	{
		bool ret = ap_target_p_.compare_exchange_strong( expected_hzd_ptr.p_, desired, success, failure );
		if ( ret ) {
			// 置き換えに成功した場合、desiredがまだハザードポインタ用のスロットには反映されていないため、更新を行う。
			expected_hzd_ptr.store( desired );
		} else {
			// 置き換えに失敗した場合、新たな値が、expected_hzd_ptr.p_に設定されている。
			// しかし、まだハザードポインタ用のスロットには反映されていないため、更新を行う。
			expected_hzd_ptr.reflect_from_p();
		}
		return ret;
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_strong を使用する。
	 *
	 * 現在の値と expected_hzd_ptr が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、
	 * そうでなければ failure メモリオーダーで expected_hzd_ptr を現在の値で置き換える
	 *
	 * @param expected_hzd_ptr 自身が保持していると期待されるハザードポインタへの参照
	 * @param desired CAS操作で置き換える新たなポインタ値
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @param failure CAS操作失敗時に使用するメモリオーダー
	 * @return true CAS操作に成功。また、expected_hzd_ptrは呼び出し前の状態を維持している。
	 * @return false CAS操作に失敗。自身が保持するあらなたポインタでexpected_hzd_ptrを置き換えられている。
	 */
	inline bool compare_exchange_strong_to_verify_exchange2( hazard_pointer&   expected_hzd_ptr,
	                                                         pointer           desired,
	                                                         std::memory_order success = std::memory_order_acq_rel,
	                                                         std::memory_order failure = std::memory_order_acquire ) noexcept
	{
		bool ret = ap_target_p_.compare_exchange_strong( expected_hzd_ptr.p_, desired, success, failure );
		if ( !ret ) {
			// 置き換えに失敗した場合、新たな値が、expected_hzd_ptr.p_に設定されている。
			// しかし、まだハザードポインタ用のスロットには反映されていないため、更新を行う。
			expected_hzd_ptr.reflect_from_p();
		}
		return ret;
	}

private:
	std::atomic<pointer> ap_target_p_;
};

/**
 * @brief 1bitのマーク付きハザードポインタハンドラ
 *
 * lock-free listのアルゴリズム上、1bitのマークのフラグが立った場合、ポインタのCASは行われないか、失敗判定とする必要がある。
 * （CAS操作はマークが立っていないことが前提で、マークが立っている場合は必ず失敗させる必要がある）
 * そのため、CAS系のI/Fのexpectのパラメータに1bitのマーク情報は必ずマークが立っていない。よって、1bitのマーク情報をCAS系のI/Fに用意する必要はない。
 *
 * 現時点のCAS系I/Fは、lock-free listの実装に必要なI/Fのみを用意する。
 *
 * @tparam T
 */
template <typename T>
class hazard_ptr_w_mark_handler {
public:
	using element_type         = T;
	using pointer              = T*;
	using const_pointer        = typename std::add_pointer<typename std::add_const<T>::type>::type;
	using hazard_pointer       = hazard_ptr<T>;
	using hazard_const_pointer = hazard_ptr<typename std::add_const<T>::type>;

	struct pointer_w_mark {
		bool    mark_ = false;
		pointer p_    = nullptr;
	};
	struct const_pointer_w_mark {
		bool          mark_ = false;
		const_pointer p_    = nullptr;
	};

	template <typename HZD_PTR_T, typename PTR_T>
	struct hazard_pointer_w_mark_impl {
		bool      mark_;
		HZD_PTR_T hp_;   // std::memory_order_releaseを扱いやすくするために、mark_の後にアトミック変数関連の操作を持つhazard_pointerの変数を宣言する。

		constexpr hazard_pointer_w_mark_impl( void )
		  : mark_( false )
		  , hp_()
		{
		}
		explicit constexpr hazard_pointer_w_mark_impl( PTR_T p_arg )
		  : mark_( false )
		  , hp_( p_arg )
		{
		}
		hazard_pointer_w_mark_impl( const hazard_pointer_w_mark_impl& )            = default;
		hazard_pointer_w_mark_impl( hazard_pointer_w_mark_impl&& )                 = default;
		hazard_pointer_w_mark_impl& operator=( const hazard_pointer_w_mark_impl& ) = default;
		hazard_pointer_w_mark_impl& operator=( hazard_pointer_w_mark_impl&& )      = default;
		~hazard_pointer_w_mark_impl()                                              = default;

		explicit hazard_pointer_w_mark_impl( const pointer_w_mark& src )
		  : mark_( src.mark_ )
		  , hp_( src.p_ )
		{
		}
		hazard_pointer_w_mark_impl& operator=( const pointer_w_mark& src )
		{
			mark_ = src.mark_;
			hp_.store( src.p_ );
			return *this;
		}
		void swap( hazard_pointer_w_mark_impl& src ) noexcept
		{
			std::swap( mark_, src.mark_ );
			hp_.swap( src.hp_ );
		}
	};

	using hazard_pointer_w_mark       = hazard_pointer_w_mark_impl<hazard_pointer, pointer>;
	using hazard_const_pointer_w_mark = hazard_pointer_w_mark_impl<hazard_const_pointer, const_pointer>;

	constexpr hazard_ptr_w_mark_handler( void ) noexcept
	  : a_target_addr_( static_cast<addr_markable>( 0U ) )
	{
	}
	explicit constexpr hazard_ptr_w_mark_handler( T* p_desired ) noexcept
	  : a_target_addr_( reinterpret_cast<addr_markable>( p_desired ) )
	{
	}
	constexpr hazard_ptr_w_mark_handler( const hazard_ptr_w_mark_handler& src ) noexcept
	  : a_target_addr_( src.a_target_addr_.load( std::memory_order_acquire ) )
	{
	}
	hazard_ptr_w_mark_handler( hazard_ptr_w_mark_handler&& src ) noexcept
	  : a_target_addr_()
	{
		addr_markable addr_expect = src.a_target_addr_.load( std::memory_order_acquire );
		do {
			a_target_addr_.store( addr_expect, std::memory_order_release );
		} while ( !src.a_target_addr_.compare_exchange_weak( addr_expect, static_cast<addr_markable>( 0U ), std::memory_order_acq_rel, std::memory_order_acquire ) );
	}
	hazard_ptr_w_mark_handler& operator=( const hazard_ptr_w_mark_handler& src ) noexcept
	{
		if ( this == &src ) return *this;

		a_target_addr_.store( src.a_target_addr_.load( std::memory_order_acquire ), std::memory_order_release );

		return *this;
	}
	hazard_ptr_w_mark_handler& operator=( hazard_ptr_w_mark_handler&& src ) noexcept
	{
		if ( this == &src ) return *this;

		constexpr addr_markable clear_val   = 0;
		addr_markable           addr_expect = src.a_target_addr_.load( std::memory_order_acquire );
		do {
			a_target_addr_.store( addr_expect, std::memory_order_release );
		} while ( !src.a_target_addr_.compare_exchange_weak( addr_expect, clear_val, std::memory_order_acq_rel, std::memory_order_acquire ) );

		return *this;
	}

	pointer_w_mark load( std::memory_order order = std::memory_order_acquire ) const noexcept
	{
		return unzip_addr_markable_to_tuple( a_target_addr_.load( order ) );
	}

	void store( const pointer_w_mark& desired, std::memory_order order = std::memory_order_release ) noexcept
	{
		a_target_addr_.store( zip_tuple_to_addr_markable( desired ), order );
	}
	void store( pointer p_desired, bool desired_mark, std::memory_order order = std::memory_order_release ) noexcept
	{
		a_target_addr_.store( zip_tuple_to_addr_markable( p_desired, desired_mark ), order );
	}

	bool is_marked( std::memory_order order = std::memory_order_acquire ) const noexcept
	{
		return load( order ).mark_;
	}

	template <typename HAZARD_PTR_T,
	          typename std::enable_if<std::is_same<HAZARD_PTR_T, hazard_pointer_w_mark>::value ||
	                                  std::is_same<HAZARD_PTR_T, hazard_const_pointer_w_mark>::value>::type* = nullptr>
	bool verify_exchange( HAZARD_PTR_T& hp_w_mark ) const noexcept
	{
		addr_markable addr_desired = zip_tuple_to_addr_markable( hp_w_mark );
		addr_markable addr_expect  = a_target_addr_.load( std::memory_order_acquire );

		bool ret = ( addr_expect == addr_desired );
		if ( !ret ) {
			hp_w_mark = unzip_addr_markable_to_tuple( addr_expect );
		}
		return ret;
	}

	template <typename HAZARD_PTR_T = hazard_pointer_w_mark,
	          typename PTR_T        = typename std::conditional<std::is_same<HAZARD_PTR_T, hazard_pointer_w_mark>::value, pointer_w_mark, const_pointer_w_mark>::type>
	HAZARD_PTR_T get_to_verify_exchange( void ) const noexcept
	{
		addr_markable addr_expect = a_target_addr_.load( std::memory_order_acquire );
		return HAZARD_PTR_T { unzip_addr_markable_to_tuple<PTR_T>( addr_expect ) };
	}

	void reuse_to_verify_exchange( hazard_pointer_w_mark& hp_w_mark_reuse ) const noexcept
	{
		addr_markable addr_expect = a_target_addr_.load( std::memory_order_acquire );
		hp_w_mark_reuse           = unzip_addr_markable_to_tuple( addr_expect );
		return;
	}
	void reuse_to_verify_exchange( hazard_const_pointer_w_mark& hp_w_mark_reuse ) const noexcept
	{
		addr_markable addr_expect = a_target_addr_.load( std::memory_order_acquire );
		hp_w_mark_reuse           = unzip_addr_markable_to_tuple( addr_expect );
		return;
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_weak を使用する。
	 *
	 * 現在の値と expected が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、expected に、desired を反映する。
	 * そうでなければ failure メモリオーダーで expected を現在の値で置き換える。
	 *
	 * @param expect 自身が保持していると期待されるポインタへの参照。CAS操作ではフラグ情報は参照されない(falseとして扱う)。
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @param failure CAS操作失敗時に使用するメモリオーダー
	 * @return true CAS操作に成功。
	 * @return false CAS操作に失敗。expected のポインタと、マーク情報は、自身が保持する現在の情報で置き換えられている。
	 */
	bool compare_exchange_weak_set_mark( pointer_w_mark&   expect,
	                                     std::memory_order success = std::memory_order_acq_rel,
	                                     std::memory_order failure = std::memory_order_acquire ) noexcept
	{
		addr_markable addr_expected = zip_tuple_to_addr_markable( expect.p_, false );
		addr_markable addr_desired  = zip_tuple_to_addr_markable( expect.p_, true );

		bool ret = a_target_addr_.compare_exchange_weak( addr_expected, addr_desired, success, failure );
		if ( !ret ) {
			expect = unzip_addr_markable_to_tuple( addr_expected );
		}
		return ret;
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_strong を使用する。
	 *
	 * 現在の値と expected が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、expected に、desired を反映する。
	 * そうでなければ failure メモリオーダーで expected を現在の値で置き換える。
	 *
	 * @param expect 自身が保持していると期待されるポインタへの参照。CAS操作ではフラグ情報は参照されない(falseとして扱う)。
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @param failure CAS操作失敗時に使用するメモリオーダー
	 * @return true CAS操作に成功し、マークの付与に成功。
	 * @return false CAS操作に失敗、マークの付与に失敗。expected のポインタと、マーク情報は、自身が保持する現在の情報で置き換えられている。
	 */
	bool compare_exchange_strong_set_mark( pointer_w_mark&   expected,
	                                       std::memory_order success = std::memory_order_acq_rel,
	                                       std::memory_order failure = std::memory_order_acquire ) noexcept
	{
		addr_markable addr_expected = zip_tuple_to_addr_markable( expected.p_, false );
		addr_markable addr_desired  = zip_tuple_to_addr_markable( expected.p_, true );

		bool ret = a_target_addr_.compare_exchange_strong( addr_expected, addr_desired, success, failure );
		if ( !ret ) {
			expected = unzip_addr_markable_to_tuple( addr_expected );
		}
		return ret;
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_strong を使用する。
	 *
	 * 現在の値と expected が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、expected に、desired を反映する。
	 * そうでなければ failure メモリオーダーで expected を現在の値で置き換える。
	 *
	 * @param expected 自身が保持していると期待されるハザードポインタへの参照。CAS操作ではフラグ情報は参照されない(falseとして扱う)。
	 * @param desired CAS操作で置き換える新たなポインタ値
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @param failure CAS操作失敗時に使用するメモリオーダー
	 * @return true CAS操作に成功。 expected は、desired で置き換えられている。boolは、falseがセットされる。
	 * @return false CAS操作に失敗。expected のポインタと、マーク情報は、自身が保持する現在の情報で置き換えられている。
	 */
	bool compare_exchange_strong_to_verify_exchange1( hazard_pointer_w_mark& expected,
	                                                  pointer                desired,
	                                                  std::memory_order      success = std::memory_order_acq_rel,
	                                                  std::memory_order      failure = std::memory_order_acquire ) noexcept
	{
		addr_markable addr_expected = zip_tuple_to_addr_markable( expected.hp_.get(), false );
		addr_markable addr_desired  = zip_tuple_to_addr_markable( desired, false );
		bool          ret           = a_target_addr_.compare_exchange_strong( addr_expected, addr_desired, success, failure );
		if ( ret ) {
			expected.mark_ = false;
			expected.hp_.store( desired );
		} else {
			expected = unzip_addr_markable_to_tuple( addr_expected );
		}
		return ret;
	}

	/**
	 * @brief CAS操作を実施する。CAS操作には、compare_exchange_strong を使用する。
	 *
	 * 現在の値と expected が等値である場合に、 success メモリオーダーで現在の値を desired で置き換え、
	 *
	 * @param expected_hzd_ptr 自身が保持していると期待されるハザードポインタへの参照。CAS操作ではフラグ情報は参照されない(falseとして扱う)。
	 * @param desired CAS操作で置き換える新たなポインタ値
	 * @param success CAS操作成功時に使用するメモリオーダー
	 * @return true CAS操作に成功。また、 expected は呼び出し前の状態を維持している。
	 * @return false CAS操作に失敗。expected のポインタと、マーク情報は、自身が保持する現在の情報で置き換えられている。
	 */
	bool compare_exchange_strong_to_verify_exchange2( hazard_pointer_w_mark& expected,
	                                                  pointer                desired,
	                                                  std::memory_order      success = std::memory_order_acq_rel ) noexcept
	{
		addr_markable addr_expected = zip_tuple_to_addr_markable( expected.hp_.get(), false );
		addr_markable addr_desired  = zip_tuple_to_addr_markable( desired, false );
		bool          ret           = a_target_addr_.compare_exchange_strong( addr_expected, addr_desired, success, std::memory_order_acquire );
		if ( !ret ) {
			expected = unzip_addr_markable_to_tuple( addr_expected );
		}
		return ret;
	}

private:
	using addr_markable = std::uintptr_t;

	static constexpr addr_markable zip_tuple_to_addr_markable( const pointer_w_mark& tp )
	{
		// reinterpret_castはconstexprの中では使えない。
		addr_markable ans = reinterpret_cast<addr_markable>( tp.p_ );
		if ( tp.mark_ ) {
			ans |= static_cast<addr_markable>( 1U );
		} /*
		else {
		    ans &= ~(static_cast<addr_markable>(1U));
		} */
		return ans;
	}
	static constexpr addr_markable zip_tuple_to_addr_markable( const hazard_pointer_w_mark& tp )
	{
		// reinterpret_castはconstexprの中では使えない。
		addr_markable ans = reinterpret_cast<addr_markable>( tp.hp_.get() );
		if ( tp.mark_ ) {
			ans |= static_cast<addr_markable>( 1U );
		} /*
		else {
		    ans &= ~(static_cast<addr_markable>(1U));
		} */
		return ans;
	}
	static constexpr addr_markable zip_tuple_to_addr_markable( const_pointer_w_mark& tp )
	{
		// reinterpret_castはconstexprの中では使えない。
		addr_markable ans = reinterpret_cast<addr_markable>( tp.p_ );
		if ( tp.mark_ ) {
			ans |= static_cast<addr_markable>( 1U );
		} /*
		else {
		    ans &= ~(static_cast<addr_markable>(1U));
		} */
		return ans;
	}
	static constexpr addr_markable zip_tuple_to_addr_markable( hazard_const_pointer_w_mark& tp )
	{
		// reinterpret_castはconstexprの中では使えない。
		addr_markable ans = reinterpret_cast<addr_markable>( tp.hp_.get() );
		if ( tp.mark_ ) {
			ans |= static_cast<addr_markable>( 1U );
		} /*
		else {
		    ans &= ~(static_cast<addr_markable>(1U));
		} */
		return ans;
	}
	static constexpr addr_markable zip_tuple_to_addr_markable( pointer p, bool mark )
	{
		// reinterpret_castはconstexprの中では使えない。
		addr_markable ans = reinterpret_cast<addr_markable>( p );
		if ( mark ) {
			ans |= static_cast<addr_markable>( 1U );
		} /*
		else {
		    ans &= ~(static_cast<addr_markable>(1U));
		} */
		return ans;
	}

	template <typename TUPLE_T = pointer_w_mark>
	static constexpr TUPLE_T unzip_addr_markable_to_tuple( addr_markable addr ) noexcept
	{
		// reinterpret_castはconstexprの中では使えない。
		pointer p_ans = reinterpret_cast<pointer>( addr & ( ~( static_cast<addr_markable>( 1U ) ) ) );
		bool    b_ans = ( ( addr & static_cast<addr_markable>( 1U ) ) == 0 ) ? false : true;
		return TUPLE_T { b_ans, p_ans };
	}

	static constexpr bool is_marked( addr_markable tut ) noexcept
	{
		return ( tut & static_cast<addr_markable>( 1U ) ) != 0;
	}

	static constexpr addr_markable set_mark( addr_markable tut ) noexcept
	{
		return tut | static_cast<addr_markable>( 1U );
	}
	static constexpr addr_markable unset_mark( addr_markable tut ) noexcept
	{
		return tut & ( ~static_cast<addr_markable>( 1U ) );
	}

	std::atomic<addr_markable> a_target_addr_;
};

}   // namespace concurrent
}   // namespace alpha

#endif /* SRC_HAZARD_PTR_HPP_ */
