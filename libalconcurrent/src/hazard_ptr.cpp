/**
 * @file hazard_ptr.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-01
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/internal/alloc_only_allocator.hpp"
#include "alconcurrent/internal/cpp_std_configure.hpp"
#include "alconcurrent/internal/hazard_ptr_internal.hpp"

#include "hazard_ptr_impl.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

////////////////////////////////////////////////////////////////////////////////////////////////////////
// configuration value
constexpr size_t conf_pre_mmap_size = 16 * 1024;

static ALCC_INTERNAL_CONSTINIT alloc_only_chamber g_alloc_only_inst_for_hzrd_ptr_module( false, conf_pre_mmap_size );   // グローバルインスタンスは、プロセス終了までメモリ領域を維持するために、デストラクタが呼ばれてもmmapした領域を解放しない。

////////////////////////////////////////////////////////////////////////////////////////////////////////
ALCC_INTERNAL_CONSTINIT global_scope_hazard_ptr_chain     g_scope_hzrd_chain_;
thread_local ALCC_INTERNAL_CONSTINIT bind_hazard_ptr_list tl_bhpl;

///////////////////////////////////////////////////////////////////////
#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
std::atomic<size_t> hazard_ptr_group::call_count_try_assign_( 0 );
std::atomic<size_t> hazard_ptr_group::loop_count_in_try_assign_( 0 );

std::atomic<size_t> call_count_hazard_ptr_get_( 0 );
std::atomic<size_t> loop_count_in_hazard_ptr_get_( 0 );
#endif

hazard_ptr_group::~hazard_ptr_group()
{
	for ( auto& e : *this ) {
		if ( e.load( std::memory_order_acquire ) != nullptr ) {
			LogOutput( log_type::ERR, "Called the destructor of hazard pointer slots. but, hazard pointer is still exist." );
		}
	}
}

hzrd_slot_ownership_t hazard_ptr_group::try_assign( void* p )
{
#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
	call_count_try_assign_++;
#endif

	hzrd_slot_ownership_t ans( nullptr );   // expect RVO

	if ( p == nullptr ) {
		return ans;
	}

	{
		if ( next_assign_hint_it_->load( std::memory_order_acquire ) == nullptr ) {
			next_assign_hint_it_->store( p, std::memory_order_release );
			ans = hzrd_slot_ownership_t( &( *next_assign_hint_it_ ) );
			++next_assign_hint_it_;
			if ( next_assign_hint_it_ == end() ) {
				next_assign_hint_it_ = begin();
			}
			return ans;
		}
	}

	for ( auto it = next_assign_hint_it_; it != end(); it++ ) {
#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
		loop_count_in_try_assign_++;
#endif
		if ( it->load( std::memory_order_acquire ) == nullptr ) {
			it->store( p, std::memory_order_release );
			ans = hzrd_slot_ownership_t( &( *it ) );
			++it;
			if ( it == end() ) {
				it = begin();
			}
			next_assign_hint_it_ = it;
			return ans;
		}
	}
	auto wraparound_end = next_assign_hint_it_;
	for ( auto it = begin(); it != wraparound_end; it++ ) {
#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
		loop_count_in_try_assign_++;
#endif
		if ( it->load( std::memory_order_acquire ) == nullptr ) {
			it->store( p, std::memory_order_release );
			ans = hzrd_slot_ownership_t( &( *it ) );
			++it;
			if ( it == end() ) {
				it = begin();
			}
			next_assign_hint_it_ = it;
			return ans;
		}
	}

	return ans;
}

hazard_ptr_group::ownership_t hazard_ptr_group::try_ocupy( void )
{
	bool expected_b = false;
	if ( is_used_.compare_exchange_strong( expected_b, true, std::memory_order_release, std::memory_order_relaxed ) ) {
		return ownership_t( this );
	}

	return ownership_t( nullptr );
}

void hazard_ptr_group::force_clear( void ) noexcept
{
	for ( auto& e : *this ) {
		if ( e.load( std::memory_order_acquire ) != nullptr ) {
			LogOutput( log_type::ERR, "hazard pointer is still exist." );
		}
		e.store( nullptr, std::memory_order_release );
	}
}

bool hazard_ptr_group::check_pointer_is_hazard_pointer( void* p ) noexcept
{
	if ( p == nullptr ) return false;

	for ( auto& e : *this ) {
		// if hazard_ptr_group has a pointer that is same to p, p is hazard pointer
		if ( e.load( std::memory_order_acquire ) == p ) return true;
	}

	return false;
}

void hazard_ptr_group::scan_hazard_pointers( std::function<void( void* )>& pred )
{
	for ( auto& e : *this ) {
		// if hazard_ptr_group has a pointer that is same to p, p is hazard pointer
		void* p = e.load( std::memory_order_acquire );
		if ( p != nullptr ) {
			pred( p );
		}
	}
}

inline void log_and_throw( const char* p )
{
	internal::LogOutput( log_type::ERR, p );
	throw std::logic_error( p );
}

void hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( hazard_ptr_group* const p_hpg_arg, std::atomic<std::uintptr_t>* const p_addr_top_valid_hpg_chain )
{
	if ( p_hpg_arg == nullptr ) {
		throw std::logic_error( "p_hpg_arg should not be nullptr. this is logic error" );
	}
	if ( p_addr_top_valid_hpg_chain == nullptr ) {
		throw std::logic_error( "p_addr_top_valid_hpg_chain should not be nullptr. this is logic error" );
	}

	if ( p_hpg_arg->is_any_deleting_accesser() ) {
		// log_and_throw( "shold be no deleting accessor" );
	}

	std::uintptr_t addr_new_top = get_addr_from_pointer( p_hpg_arg );

	std::uintptr_t addr_nxt = p_addr_top_valid_hpg_chain->load( std::memory_order_acquire );
	p_hpg_arg->get_valid_chain_next_writer_accesser().store_address( addr_nxt );

	while ( !( p_addr_top_valid_hpg_chain->compare_exchange_weak( addr_nxt, addr_new_top, std::memory_order_release, std::memory_order_acquire ) ) ) {
		// p_addr_top_valid_hpg_chainの中の値には削除マークはつかないので、clr処理は不要
		p_hpg_arg->get_valid_chain_next_writer_accesser().store_address( addr_nxt );
	}
}
void hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( hazard_ptr_group* const p_hpg_arg, std::atomic<std::uintptr_t>* const p_addr_top_valid_hpg_chain )
{
	if ( p_hpg_arg == nullptr ) {
		throw std::logic_error( "p_hpg_arg should not be nullptr. this is logic error" );
	}
	if ( p_addr_top_valid_hpg_chain == nullptr ) {
		throw std::logic_error( "p_addr_top_valid_hpg_chain should not be nullptr. this is logic error" );
	}

	if ( !is_hazard_ptr_group_in_valid_chain( p_hpg_arg, p_addr_top_valid_hpg_chain ) ) {
		// すでに削除されている
		throw std::logic_error( "unexpected deletion" );
		// return;
	}

	{
		// 削除マークを付与する。
		p_hpg_arg->get_valid_chain_next_writer_accesser().set_del_mark();

		// 削除を試みる
		// Note: Lock-freeリストの削除処理の過程おいては、削除マーク付きのアドレス情報は削除が完了するまで書き換えてはならないという制約を満たす必要がある。
		// Note: ABA問題に関して、これらのAPI群は下記の制約を前提としているため、このAPIによる削除完了後、hazard_ptr_group::is_used_フラグがfalseになるまで再使用対象とならない。
		// Note: ・削除権は、hazard_ptr_groupのスレッドオーナーだけが持っている。
		// Note: ・削除処理自体は、srch_pre_hazard_ptr_group_on_valid_chain()を呼び出すスレッドも行うが、このAPIを呼び出すhazard_ptr_groupのスレッドオーナーによって、削除完了まで監視される。
		while ( is_hazard_ptr_group_in_valid_chain( p_hpg_arg, p_addr_top_valid_hpg_chain ) ) {
		}
		// ここに来た時点で、削除に成功
		if ( !is_del_marked( p_hpg_arg->delmarkable_valid_chain_next_.get_reader_accesser().load_address() ) ) {
			log_and_throw( "this is TEST3 logic error" );
		}
	}

	// 削除アクセス中のスレッドがいなくなるまでビジーループ
	while ( p_hpg_arg->is_any_deleting_accesser() ) {
#ifdef ALCONCURRENT_CONF_ENABLE_YIELD_IN_HAZARD_POINTER_THREAD_DESTRUCTION
		// ビジーループでも成立するが、スレッドの実行権を一回放棄した方が、システム負荷は下がる。
		// ただし、lock-freeとは言えなくなる。
		// しかしながら、このAPIが使用されるのは、スレッド終了時のみであるため、すでにlock-free要請はなくなっていると考えてよい。
		std::this_thread::yield();
#endif
	}

	return;
}
bool hazard_ptr_group::is_hazard_ptr_group_in_valid_chain( hazard_ptr_group* const p_hpg_arg, std::atomic<std::uintptr_t>* const p_addr_top_valid_hpg_chain )
{
	// Note: Lock-freeリストの削除処理の過程おいては、削除マーク付きのアドレス情報は削除が完了するまで書き換えてはならないという制約を満たす必要がある。
	// Note: さらに、このhazard_ptr_groupのvalid_chainのリストについては、削除完了後も削除マーク付きのアドレス情報を書き換えてはならない追加の制約がある。
	// Note: これは、hazard_ptr_groupのハザードポインタチェック処理をリストの最後まで走り切らせる必要があるため、
	// Note: 削除後であってもリンクのつながりを切ってはならないという制約条件があるため。

	if ( p_hpg_arg == nullptr ) {
		return false;
	}
	if ( p_addr_top_valid_hpg_chain == nullptr ) {
		return false;
	}

	while ( true ) {
		// 最初から検索をやり直す。
		del_markable_pointer::writer_twin_accessor<hazard_ptr_group> acr_pre2cur( p_addr_top_valid_hpg_chain );
		hazard_ptr_group*                                            p_cur_hpg = acr_pre2cur.get_pointer_to_cur();
		if ( p_cur_hpg == nullptr ) {
			// valid chaingが空なので、ループ終了
			break;
		}

		while ( p_cur_hpg != nullptr ) {
			std::uintptr_t addr_nxt_hpg = acr_pre2cur.load_nxt_address_of_cur();
			if ( is_del_marked( addr_nxt_hpg ) ) {
				// curに削除マークがついているので、削除を試みる
				hazard_ptr_group* p_nxt_hpg    = get_pointer_from_addr_clr_marker<hazard_ptr_group>( addr_nxt_hpg );   // 削除マークフラグをクリアした次へのアドレス情報を得る
				std::uintptr_t    addr_cur_hpg = get_addr_from_pointer( p_cur_hpg );
				if ( acr_pre2cur.pre_address_compare_exchange_weak( addr_cur_hpg, p_nxt_hpg ) ) {
					// 削除に成功
					acr_pre2cur.re_setup();
					p_cur_hpg = acr_pre2cur.get_pointer_to_cur();
					continue;
				} else {
					// 削除に失敗(＝誰かが削除した or p_addr_curに削除マークがついた or 削除後さらに別の場所に挿入された)
					// よって、ループの先頭からやり直す
					break;
				}
			} else {
				// 探しているものかどうかを確認する。
				if ( p_cur_hpg == p_hpg_arg ) {
					return true;
				}

				// 削除マークがついていないので、次に進める
				acr_pre2cur.shift();
				p_cur_hpg = acr_pre2cur.get_pointer_to_cur();
			}
		}
		if ( p_cur_hpg == nullptr ) {
			// 見つからなかったのでループ終了
			break;
		}
	}

	return false;
}

#ifdef ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
#else
#if __cpp_aligned_new
ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new( std::size_t size, std::align_val_t alignment )   // possible throw std::bad_alloc, from C++17
{
	void* p_ans = g_alloc_only_inst_for_hzrd_ptr_module.allocate( size, static_cast<size_t>( alignment ) );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}
	return p_ans;
}
ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new( std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++17
{
	return g_alloc_only_inst_for_hzrd_ptr_module.allocate( size, static_cast<size_t>( alignment ) );
}
ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new[]( std::size_t size, std::align_val_t alignment )   // possible throw std::bad_alloc, from C++17
{
	void* p_ans = g_alloc_only_inst_for_hzrd_ptr_module.allocate( size, static_cast<size_t>( alignment ) );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}
	return p_ans;
}
ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new[]( std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++17
{
	return g_alloc_only_inst_for_hzrd_ptr_module.allocate( size, static_cast<size_t>( alignment ) );
}
#endif
ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new( std::size_t size )   // possible throw std::bad_alloc, from C++11
{
	void* p_ans = g_alloc_only_inst_for_hzrd_ptr_module.allocate( size );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}
	return p_ans;
}
ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new( std::size_t size, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++11
{
	return g_alloc_only_inst_for_hzrd_ptr_module.allocate( size );
}
ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new[]( std::size_t size )   // possible throw std::bad_alloc, from C++11
{
	void* p_ans = g_alloc_only_inst_for_hzrd_ptr_module.allocate( size );
	if ( p_ans == nullptr ) {
		throw std::bad_alloc();
	}
	return p_ans;
}
ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new[]( std::size_t size, const std::nothrow_t& ) noexcept   // possible return nullptr, instead of throwing exception, from C++11
{
	return g_alloc_only_inst_for_hzrd_ptr_module.allocate( size );
}

ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new( std::size_t size, void* ptr ) noexcept   // placement new, from C++11
{
	return ptr;
}
ALCC_INTERNAL_NODISCARD_ATTR void* hazard_ptr_group::operator new[]( std::size_t size, void* ptr ) noexcept   // placement new for array, from C++11
{
	return ptr;
}

#if __cpp_aligned_new
void hazard_ptr_group::operator delete( void* ptr, std::align_val_t alignment ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete( void* ptr, std::size_t size, std::align_val_t alignment ) noexcept   // from C++17
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete[]( void* ptr, std::align_val_t alignment ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete[]( void* ptr, std::size_t size, std::align_val_t alignment ) noexcept   // from C++17
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete( void* ptr, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete( void* ptr, std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete[]( void* ptr, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17, and no sized deallocation support(in case of using clang without -fsized-deallocation)
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete[]( void* ptr, std::size_t size, std::align_val_t alignment, const std::nothrow_t& ) noexcept   // from C++17
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
#endif
void hazard_ptr_group::operator delete( void* ptr ) noexcept   // from C++11
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete[]( void* ptr ) noexcept   // from C++11
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}

void hazard_ptr_group::operator delete( void* ptr, std::size_t size ) noexcept   // from C++14
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete[]( void* ptr, std::size_t size ) noexcept   // from C++14
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}

void hazard_ptr_group::operator delete( void* ptr, const std::nothrow_t& ) noexcept   // from C++11
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete[]( void* ptr, const std::nothrow_t& ) noexcept   // from C++11
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}

void hazard_ptr_group::operator delete( void* ptr, std::size_t size, const std::nothrow_t& ) noexcept   // from C++14
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}
void hazard_ptr_group::operator delete[]( void* ptr, std::size_t size, const std::nothrow_t& ) noexcept   // from C++14
{
	g_alloc_only_inst_for_hzrd_ptr_module.deallocate( ptr );
}

void hazard_ptr_group::operator delete( void* ptr, void* ) noexcept   // delete for area that is initialized by placement new.
{
	// nothing to do
}
void hazard_ptr_group::operator delete[]( void* ptr, void* ) noexcept   // delete for area that is initialized by placement new.
{
	// nothing to do
}
#endif

//////////////////////////////////////////////////////////////////////////////
bind_hazard_ptr_list::~bind_hazard_ptr_list()
{
	if ( hazard_ptr_mgr::IsDestoryed() ) return;

	hazard_ptr_group* p_cur_list = ownership_ticket_.get();
	if ( p_cur_list == nullptr ) return;

	do {
		p_cur_list->force_clear();

		hazard_ptr_group* p_next_list = p_cur_list->ap_list_next_.load( std::memory_order_acquire );
		p_cur_list                    = p_next_list;
	} while ( p_cur_list != nullptr );

	global_scope_hazard_ptr_chain::ReleaseOwnership( std::move( ownership_ticket_ ) );
}

hzrd_slot_ownership_t bind_hazard_ptr_list::slot_assign( void* p )
{
	if ( p == nullptr ) {
		return nullptr;
	}

	hazard_ptr_group* p_pre_list = nullptr;
	hazard_ptr_group* p_cur_list = ownership_ticket_.get();
	while ( p_cur_list != nullptr ) {
		hzrd_slot_ownership_t ans = p_cur_list->try_assign( p );
		if ( ans != nullptr ) {
			return ans;   // success to assign the slot of hazard pointer
		}

		hazard_ptr_group* p_next_list = p_cur_list->ap_list_next_.load( std::memory_order_acquire );
		p_pre_list                    = p_cur_list;
		p_cur_list                    = p_next_list;
	}

	hazard_ptr_group* p_new_hpg = nullptr;
	if ( p_pre_list != nullptr ) {
		// 追加割り当てが必要になった状態
		p_new_hpg = new hazard_ptr_group;   // ここから例外がスローされる可能性がある。
		p_pre_list->ap_list_next_.store( p_new_hpg, std::memory_order_release );
	} else {
		// まだなにも割り当てられていない状態。
		// グローバルからの再利用を行う。
		ownership_ticket_ = global_scope_hazard_ptr_chain::GetOwnership();
		p_new_hpg         = ownership_ticket_.get();
	}

	hzrd_slot_ownership_t ans = p_new_hpg->try_assign( p );
	if ( ans == nullptr ) {
		throw std::logic_error( "Fail to assign hazard pointer slot. this is logic error" );
	}
	return ans;
}

//////////////////////////////////////////////////////////////////////////////
hazard_ptr_group::ownership_t global_scope_hazard_ptr_chain::GetOwnership( void )
{
	return g_scope_hzrd_chain_.get_ownership();
}

void global_scope_hazard_ptr_chain::ReleaseOwnership( hazard_ptr_group::ownership_t os )
{
	if ( os == nullptr ) {
		throw std::logic_error( "ownership is nullptr. and required to release it. this is logic error" );
	}

	g_scope_hzrd_chain_.release_ownership( std::move( os ) );
}

hazard_ptr_group::ownership_t global_scope_hazard_ptr_chain::try_get_ownership( void )
{
	hazard_ptr_group* p_cur_hpg = ap_top_hzrd_ptr_chain_.load( std::memory_order_acquire );
	while ( p_cur_hpg != nullptr ) {
		hazard_ptr_group::ownership_t ans = p_cur_hpg->try_ocupy();
		if ( ans != nullptr ) return ans;   // success to get ownership

		hazard_ptr_group* p_next_hpg = p_cur_hpg->ap_chain_next_.load( std::memory_order_acquire );
		p_cur_hpg                    = p_next_hpg;
	}

	return nullptr;
}

void global_scope_hazard_ptr_chain::release_ownership( hazard_ptr_group::ownership_t up_o )
{
	hazard_ptr_group::remove_hazard_ptr_group_from_valid_chain( up_o.get(), &aaddr_top_hzrd_ptr_valid_chain_ );
}

void global_scope_hazard_ptr_chain::register_new_hazard_ptr_group( hazard_ptr_group* p_hpg_arg )
{
	// push p_phg_arg to top of chain like stack(LILO, even if not allow out)
	hazard_ptr_group* p_cur_top_hpg = ap_top_hzrd_ptr_chain_.load( std::memory_order_acquire );
	p_hpg_arg->ap_chain_next_.store( p_cur_top_hpg, std::memory_order_release );
	while ( !ap_top_hzrd_ptr_chain_.compare_exchange_weak( p_cur_top_hpg, p_hpg_arg, std::memory_order_release, std::memory_order_release ) ) {
		p_hpg_arg->ap_chain_next_.store( p_cur_top_hpg, std::memory_order_release );
	}
}

void global_scope_hazard_ptr_chain::register_hazard_ptr_group_to_valid_list( hazard_ptr_group* p_hpg_arg )
{
	if ( p_hpg_arg == nullptr ) {
		throw std::logic_error( "p_hpg_arg is nullptr. this is logic error" );
	}

	hazard_ptr_group::push_front_hazard_ptr_group_to_valid_chain( p_hpg_arg, &aaddr_top_hzrd_ptr_valid_chain_ );
}

hazard_ptr_group::ownership_t global_scope_hazard_ptr_chain::get_ownership( void )
{
	hazard_ptr_group::ownership_t ans = try_get_ownership();
	if ( ans == nullptr ) {
		hazard_ptr_group* p_new_hpg = new hazard_ptr_group;
		ans                         = p_new_hpg->try_ocupy();
		if ( ans == nullptr ) {
			delete p_new_hpg;
			throw std::logic_error( "Fail to get ownership. this is logic error" );
		}
		register_new_hazard_ptr_group( p_new_hpg );
	}

	// valid chainに追加する
	register_hazard_ptr_group_to_valid_list( ans.get() );

	return ans;
}

bool global_scope_hazard_ptr_chain::check_pointer_is_hazard_pointer( void* p ) noexcept
{
	if ( p == nullptr ) return false;

	hazard_ptr_group* p_cur_chain = get_pointer_from_addr_clr_marker<hazard_ptr_group>( aaddr_top_hzrd_ptr_valid_chain_.load( std::memory_order_acquire ) );

	while ( p_cur_chain != nullptr ) {
		if ( p_cur_chain->is_used() ) {
			hazard_ptr_group* p_cur_list = p_cur_chain;
			while ( p_cur_list != nullptr ) {
				if ( p_cur_list->check_pointer_is_hazard_pointer( p ) ) {
					return true;
				}
				hazard_ptr_group* p_next_list = p_cur_list->ap_list_next_.load( std::memory_order_acquire );
				p_cur_list                    = p_next_list;
			}
		}
		hazard_ptr_group* p_next_chain = p_cur_chain->get_valid_chain_next_reader_accesser().load_pointer<hazard_ptr_group>();
		p_cur_chain                    = p_next_chain;
	}

	return false;
}

void global_scope_hazard_ptr_chain::scan_hazard_pointers( std::function<void( void* )>& pred )
{
	hazard_ptr_group* p_cur_chain = get_pointer_from_addr_clr_marker<hazard_ptr_group>( aaddr_top_hzrd_ptr_valid_chain_.load( std::memory_order_acquire ) );

	while ( p_cur_chain != nullptr ) {
		if ( p_cur_chain->is_used() ) {
			hazard_ptr_group* p_cur_list = p_cur_chain;
			while ( p_cur_list != nullptr ) {
				p_cur_list->scan_hazard_pointers( pred );

				hazard_ptr_group* p_next_list = p_cur_list->ap_list_next_.load( std::memory_order_acquire );
				p_cur_list                    = p_next_list;
			}
		}
		hazard_ptr_group* p_next_chain = p_cur_chain->get_valid_chain_next_reader_accesser().load_pointer<hazard_ptr_group>();
		p_cur_chain                    = p_next_chain;
	}
}

void global_scope_hazard_ptr_chain::remove_all( void )
{
	tl_bhpl = bind_hazard_ptr_list();

	hazard_ptr_group* p_cur_chain = ap_top_hzrd_ptr_chain_.load( std::memory_order_acquire );
	ap_top_hzrd_ptr_chain_.store( nullptr, std::memory_order_release );

	while ( p_cur_chain != nullptr ) {
		hazard_ptr_group* p_next_chain = p_cur_chain->ap_chain_next_.load( std::memory_order_acquire );
		hazard_ptr_group* p_cur_list   = p_cur_chain;
		while ( p_cur_list != nullptr ) {
			hazard_ptr_group* p_next_list = p_cur_list->ap_list_next_.load( std::memory_order_acquire );
			delete p_cur_list;
			p_cur_list = p_next_list;
		}
		p_cur_chain = p_next_chain;
	}

#ifdef ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
#else
	alloc_chamber_statistics chk_ret = g_alloc_only_inst_for_hzrd_ptr_module.get_statistics();
	log_type                 lg      = log_type::INFO;
	if ( chk_ret.num_of_using_allocated_ > 0 ) {
		lg = log_type::ERR;
	}
	LogOutput( lg, chk_ret.print().c_str() );
#endif
#ifdef ALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE
	LogOutput( log_type::DUMP, "Profile of hazard_ptr_group:" );
	LogOutput( log_type::DUMP, "\tcall count of hazard_ptr_group::try_assign() -> %zu", hazard_ptr_group::call_count_try_assign_.load() );
	LogOutput( log_type::DUMP, "\tloop count in hazard_ptr_group::try_assign() -> %zu", hazard_ptr_group::loop_count_in_try_assign_.load() );
	LogOutput( log_type::DUMP, "\tcall count of hazard_ptr<T>::get() -> %zu", call_count_hazard_ptr_get_.load() );
	LogOutput( log_type::DUMP, "\tloop count in hazard_ptr<T>::get() -> %zu", loop_count_in_hazard_ptr_get_.load() );
#endif
}

//////////////////////////////////////////////////////////////////////////////

hzrd_slot_ownership_t hazard_ptr_mgr::AssignHazardPtrSlot( void* p )
{
	return internal::tl_bhpl.slot_assign( p );
}

bool hazard_ptr_mgr::CheckPtrIsHazardPtr( void* p ) noexcept
{
	return g_scope_hzrd_chain_.check_pointer_is_hazard_pointer( p );
}

void hazard_ptr_mgr::ScanHazardPtrs( std::function<void( void* )> pred )
{
	g_scope_hzrd_chain_.scan_hazard_pointers( pred );
}

void hazard_ptr_mgr::DestoryAll( void )
{
	return g_scope_hzrd_chain_.remove_all();
}

bool hazard_ptr_mgr::IsDestoryed( void )
{
	return g_scope_hzrd_chain_.is_empty();
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
