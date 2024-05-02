/*!
 * @file	lf_mem_alloc_idx_mgr.hpp
 * @brief	internal component of semi lock-free memory allocator
 * @author	Teruaki Ata
 * @date	Created on 2021/05/18
 * @details
 *
 * Copyright (C) 2021 by Teruaki Ata <PFA03027@nifty.com>
 */

#ifndef ALCONCURRENT_INTERNAL_LF_MEM_ALLOC_IDX_MGR_HPP_
#define ALCONCURRENT_INTERNAL_LF_MEM_ALLOC_IDX_MGR_HPP_

#include <atomic>
#include <cstdlib>
#include <list>
#include <memory>

#include "alconcurrent/conf_logger.hpp"
#include "alconcurrent/dynamic_tls.hpp"
#include "alconcurrent/internal/alloc_only_allocator.hpp"

#include "alconcurrent/lf_mem_alloc_type.hpp"

namespace alpha {
namespace concurrent {

namespace internal {

constexpr size_t MAX_ALLOC_SIZE_LIMIT = (size_t)( 2UL * 1024UL * 1024UL * 1024UL );
constexpr size_t NON_OWNERED_TL_ID    = 0;

struct slot_array_mgr;

constexpr unsigned int RecycleGroupStatusMask  = 0x10;
constexpr unsigned int TryAllocGroupStatusMask = 0x20;

/*!
 * @brief	chunk control status
 */
enum class chunk_control_status : unsigned int {
	EMPTY                 = 0,                                                      //!< chunk header has no allocated chunk memory.
	RESERVED_ALLOCATION   = 1,                                                      //!< chunk header has no allocated chunk memory. But some one start to allocation
	NORMAL                = 2 | RecycleGroupStatusMask | TryAllocGroupStatusMask,   //!< allow to allocate the memory from this chunk
	RESERVED_DELETION     = 3 | RecycleGroupStatusMask | TryAllocGroupStatusMask,   //!< does not allow to allocate the memory from this chunk. But if needed to reuse this chunk, allow to change NORMAL
	ANNOUNCEMENT_DELETION = 4 | RecycleGroupStatusMask,                             //!< does not allow to allocate the memory from this chunk. And some one start to deletion trail.
	DELETION              = 5,                                                      //!< does not allow to access any more except GC. After shift to this state, chunk memory will be free after confirmed accesser is zero.
};

/*!
 * @brief statistics information of alloc/dealloc
 *
 */
struct chunk_list_statistics {
	constexpr chunk_list_statistics( void )
	  : chunk_num_( 0 )
	  , valid_chunk_num_( 0 )
	  , total_slot_cnt_( 0 )
	  , free_slot_cnt_( 0 )
	  , consum_cnt_( 0 )
	  , max_consum_cnt_( 0 )
	  , alloc_req_cnt_( 0 )
	  , alloc_req_err_cnt_( 0 )
	  , dealloc_req_cnt_( 0 )
	  , dealloc_req_err_cnt_( 0 )
	  , alloc_collision_cnt_( 0 )
	  , dealloc_collision_cnt_( 0 )
	{
	}
	constexpr chunk_list_statistics( const chunk_list_statistics& ) = default;
	constexpr chunk_list_statistics( chunk_list_statistics&& )      = default;
#if ( __cpp_constexpr >= 201304 )
	constexpr chunk_list_statistics& operator=( const chunk_list_statistics& ) = default;
	constexpr chunk_list_statistics& operator=( chunk_list_statistics&& )      = default;
#else
	chunk_list_statistics& operator=( const chunk_list_statistics& ) = default;
	chunk_list_statistics& operator=( chunk_list_statistics&& )      = default;
#endif

	chunk_statistics get_statistics( void ) const;

	std::atomic<unsigned int> chunk_num_;               //!< number of chunks
	std::atomic<unsigned int> valid_chunk_num_;         //!< number of valid chunks
	std::atomic<size_t>       total_slot_cnt_;          //!< total slot count
	std::atomic<size_t>       free_slot_cnt_;           //!< free slot count
	std::atomic<unsigned int> consum_cnt_;              //!< current count of allocated slots
	std::atomic<unsigned int> max_consum_cnt_;          //!< max count of allocated slots
	std::atomic<unsigned int> alloc_req_cnt_;           //!< allocation request count
	std::atomic<unsigned int> alloc_req_err_cnt_;       //!< fail count for allocation request
	std::atomic<unsigned int> dealloc_req_cnt_;         //!< deallocation request count
	std::atomic<unsigned int> dealloc_req_err_cnt_;     //!< fail count for deallocation request
	std::atomic<unsigned int> alloc_collision_cnt_;     //!< count of a collision of allocation in lock-free algorithm
	std::atomic<unsigned int> dealloc_collision_cnt_;   //!< count of a collision of deallocation in lock-free algorithm
};

class chunk_header_multi_slot;
class slot_header_of_array;
struct slot_chk_result {
	bool                     correct_;   //!< slot header check result. true: check result is valid.
	chunk_header_multi_slot* p_chms_;    //!< pointer to chunk_header_multi_slot. if correct_ is true and this pointer value is nullptr, it is allocated by malloc with slot_header
	slot_array_mgr*          p_sam_;     //!< pointer to slot array manager
	slot_header_of_array*    p_sha_;     //!< pointer to slot_header_of_array
};

/*!
 * @brief	management information of a chunk
 */
class chunk_header_multi_slot {
public:
	std::atomic<chunk_header_multi_slot*> p_next_chunk_;      //!< pointer to next chunk header. chunk header does not free. therefore we don't need to consider ABA.
	std::atomic<chunk_control_status>     status_;            //!< chunk status for GC
	std::atomic<unsigned int>             owner_tl_id_;       //!< owner tl_id_
	mutable std::atomic<int>              num_of_accesser_;   //!< number of accesser to slot buffer

	/*!
	 * @brief	constructor
	 */
	chunk_header_multi_slot(
		const param_chunk_allocation& ch_param_arg,      //!< [in] chunk allocation paramter
		const unsigned int            owner_tl_id_arg,   //!< [in] owner tl_id_
		chunk_list_statistics*        p_chms_stat_arg    //!< [in] pointer to statistics inforation to store activity
	);

	/*!
	 * @brief	destructor
	 */
	~chunk_header_multi_slot();

	/*!
	 * @brief	allocate new memory slot
	 *
	 * @pre status_ must be chunk_control_status::NORMAL
	 *
	 * @return	pointer to an allocated memory slot
	 * @retval	non-nullptr	success to allocate and it is a pointer to an allocated memory
	 * @retval	nullptr		fail to allocate
	 */
	inline void* allocate_mem_slot( size_t req_size, size_t req_align )
	{
		void* p_ans = allocate_mem_slot_impl( req_size, req_align );
		if ( p_ans != nullptr ) {
			p_statistics_->free_slot_cnt_--;
			auto cur     = p_statistics_->consum_cnt_.fetch_add( 1, std::memory_order_acq_rel ) + 1;
			auto cur_max = p_statistics_->max_consum_cnt_.load( std::memory_order_acquire );
			if ( cur > cur_max ) {
				p_statistics_->max_consum_cnt_.compare_exchange_strong( cur_max, cur, std::memory_order_acq_rel );
			}
		}
		return p_ans;
	}

	/*!
	 * @brief	recycle memory slot
	 *
	 * @retval	true	success to recycle.
	 * @retval	false	fail to recycle. Normally p_recycle_slot does not belong to this chunk
	 */
	inline bool recycle_mem_slot(
		void* p_recycle_mem   //!< [in] pointer to the memory slot to recycle.
	)
	{
		if ( p_recycle_mem == nullptr ) {
			return false;
		}

		return recycle_mem_slot_impl( p_recycle_mem );
	}

	/*!
	 * @brief	recycle memory slot
	 *
	 * @retval	true	success to recycle.
	 * @retval	false	fail to recycle. Normally p_recycle_slot does not belong to this chunk
	 */
	inline bool unchk_recycle_mem_slot(
		slot_array_mgr*       p_rcv_sam,
		slot_header_of_array* p_recycle_slot   //!< [in] pointer to the memory slot to recycle.
	)
	{
		if ( p_recycle_slot == nullptr ) {
			return false;
		}

		return unchk_recycle_mem_slot_impl( p_rcv_sam, p_recycle_slot );
	}

	/*!
	 * @brief	allocate new chunk
	 *
	 * @pre status_ must be chunk_control_status::EMPTY
	 *
	 * @retval true success to allocate a chunk memory
	 * @retval false fail to allocate a chunk memory
	 *
	 * @post owner_tl_id_ is owner_tl_id_arg, and status_ is chunk_control_status::NORMAL
	 */
	bool alloc_new_chunk(
		const param_chunk_allocation& ch_param_arg,     //!< [in] chunk allocation paramter
		const unsigned int            owner_tl_id_arg   //!< [in] owner tl_id_
	);

	/*!
	 * @brief	try to get ownership of this chunk
	 *
	 * @pre owner_tl_id_ is owner_tl_id_arg, and status_ is chunk_control_status::NORMAL or chunk_control_status::RESERVED_DELETION
	 *
	 * @retval non-nullptr success to get ownership and allocate a memory slot
	 * @retval false fail to get ownership or fail to allocate a memory slot
	 *
	 * @post status_ is chunk_control_status::NORMAL
	 */
	inline void* try_allocate_mem_slot_from_reserved_deletion(
		const unsigned int owner_tl_id_arg,   //!< [in] owner tl_id_
		const size_t       req_size,          //!< [in] requested size
		const size_t       req_align          //!< [in] requested align size
	)
	{
		unsigned int cur_tl_id = owner_tl_id_.load( std::memory_order_acquire );
		return try_allocate_mem_slot_impl( cur_tl_id, owner_tl_id_arg, req_size, req_align );
	}

	/*!
	 * @brief	try to get ownership of this chunk
	 *
	 * @pre owner_tl_id_ is NON_OWNERED_TL_ID, and status_ is chunk_control_status::NORMAL or chunk_control_status::RESERVED_DELETION
	 *
	 * @retval non-nullptr success to get ownership and allocate a memory slot
	 * @retval false fail to get ownership or fail to allocate a memory slot
	 *
	 * @post owner_tl_id_ is owner_tl_id_arg, and status_ is chunk_control_status::NORMAL
	 */
	inline void* try_get_ownership_allocate_mem_slot(
		const unsigned int owner_tl_id_arg,   //!< [in] owner tl_id_
		const size_t       req_size,          //!< [in] requested size
		const size_t       req_align          //!< [in] requested align size
	)
	{
		return try_allocate_mem_slot_impl( NON_OWNERED_TL_ID, owner_tl_id_arg, req_size, req_align );
	}

	bool set_delete_reservation( void );
	bool unset_delete_reservation( void );
	bool exec_deletion( void );

	const param_chunk_allocation& get_param_chunk_allocation( void ) const
	{
		return slot_conf_;
	}

	/*!
	 * @brief	get chunk_header_multi_slot address from void* address that allocate_mem_slot() returns.
	 *
	 * @return	check result and pointer to chunk
	 */
	static slot_chk_result get_chunk(
		void* p_addr   //!< [in] void* address that allocate_mem_slot() returns
	);

	/*!
	 * @brief	get statistics
	 */
	chunk_statistics get_statistics( void ) const;

	/*!
	 * @brief	dump for debug
	 */
	void dump( void );

	void* operator new( std::size_t n ) = delete;       // usual new...(1)
	void  operator delete( void* p_mem ) noexcept {};   // usual delete...(2)	dynamic_tls_content_arrayは、破棄しないクラスなので、メモリ開放しない

	void* operator new[]( std::size_t n )           = delete;   // usual new...(1)
	void  operator delete[]( void* p_mem ) noexcept = delete;   // usual delete...(2)	dynamic_tls_content_arrayは、破棄しないクラスなので、メモリ開放しない

	void* operator new( std::size_t n, internal::alloc_only_chamber& allocator_arg )   // placement new
	{
		return allocator_arg.allocate( n, sizeof( uintptr_t ) );
	}
	void operator delete( void* p, internal::alloc_only_chamber& allocator_arg ) noexcept   // placement delete...(3)
	{
	}

private:
	void set_slot_allocation_conf(
		const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
	);

	/*!
	 * @brief	allocate new memory slot
	 *
	 * @return	pointer to an allocated memory slot
	 * @retval	non-nullptr	success to allocate and it is a pointer to an allocated memory
	 * @retval	nullptr		fail to allocate
	 */
	void* allocate_mem_slot_impl( size_t req_size, size_t req_align );

	/*!
	 * @brief	recycle memory slot
	 *
	 * @retval	true	success to recycle.
	 * @retval	false	fail to recycle. Normally p_recycle_slot does not belong to this chunk
	 */
	bool recycle_mem_slot_impl(
		void* p_recycle_slot   //!< [in] pointer to the memory slot to recycle.
	);

	/*!
	 * @brief	recycle memory slot without checking if p_recycle_slot belongs to this chunk_header_multi_slot.
	 *
	 * @retval	true	success to recycle.
	 * @retval	false	fail to recycle. Normally p_recycle_slot does not belong to this chunk
	 */
	bool unchk_recycle_mem_slot_impl(
		slot_array_mgr*       p_rcv_sam,
		slot_header_of_array* p_recycle_slot   //!< [in] pointer to the memory slot to recycle.
	);

	/*!
	 * @brief	try to get ownership of this chunk
	 *
	 * @pre owner_tl_id_ is expect_tl_id_arg, and status_ is chunk_control_status::NORMAL or chunk_control_status::RESERVED_DELETION
	 *
	 * @retval non-nullptr success to get ownership and allocate a memory slot
	 * @retval false fail to get ownership or fail to allocate a memory slot
	 *
	 * @post owner_tl_id_ is owner_tl_id_arg, and status_ is chunk_control_status::NORMAL
	 */
	void* try_allocate_mem_slot_impl(
		const unsigned int expect_tl_id_arg,   //!< [in] owner tl_id_
		const unsigned int owner_tl_id_arg,    //!< [in] owner tl_id_
		const size_t       req_size,           //!< [in] requested size
		const size_t       req_align           //!< [in] requested align size
	);

	chunk_list_statistics* p_statistics_;   //!< statistics

	param_chunk_allocation slot_conf_;          //!< allocation configuration paramter. value is corrected internally.
	slot_array_mgr*        p_slot_array_mgr_;   //!< pointer to slot_array_mgr
};

/*!
 * @brief	chunk list that has same allocation parameter
 */
class chunk_list {
public:
	/*!
	 * @brief	constructor
	 */
	constexpr chunk_list(
		const param_chunk_allocation& ch_param_arg,     //!< [in] chunk allocation paramter
		internal::alloc_only_chamber* p_allocator_arg   //!< [in] 割り当て専用アロケータへのポインタ
		)
	  : chunk_param_( ch_param_arg )
	  , p_allocator_( p_allocator_arg )
	  , p_top_chunk_()
	  , tls_hint_( tl_chunk_param_destructor( this ) )
	  , statistics_()
	{
	}

	/*!
	 * @brief	destructor
	 */
	~chunk_list() = default;

	/*!
	 * @brief	allocate new memory slot
	 *
	 * @return	pointer to an allocated memory slot
	 * @retval	non-nullptr	success to allocate and it is a pointer to an allocated memory
	 * @retval	nullptr		fail to allocate
	 */
	void* allocate_mem_slot( size_t req_size, size_t req_align );

	/*!
	 * @brief	recycle memory slot
	 *
	 * @retval	true	success to recycle.
	 * @retval	false	fail to recycle. Normally p_recycle_slot does not belong to this chunk
	 */
	bool recycle_mem_slot(
		void* p_recycle_slot   //!< [in] pointer to the memory slot to recycle.
	);

	/*!
	 * @brief	free the deletable buffers
	 */
	void prune( void );

	/*!
	 * @brief	get statistics
	 */
	chunk_statistics get_statistics( void ) const;

	const param_chunk_allocation chunk_param_;

private:
	/**
	 * @brief スレッド毎のチャンク操作のためのヒント情報、および所有者となるchunk_listへの情報を保持する構造体
	 */
	struct tl_chunk_param {
		chunk_list* const        p_owner_chunk_list_;   //!< 所有者となるchunk_listへのポインタ
		const unsigned int       tl_id_;                //!< スレッド毎に構造体を区別するためおID
		size_t                   num_of_pieces_;        //!< 現在のメモリ領域割り当てに使用したスロット数
		chunk_header_multi_slot* tls_p_hint_chunk;      //!< 所有者のスレッドが最初に参照するべきchunk_header_multi_slotへのポインタ。

		tl_chunk_param(
			chunk_list* p_owner_chunk_list_arg,   //!< [in] pointer to onwer chunk_list
			size_t      init_num_of_pieces_arg    //!< [in] initiall number of slots for allocation
		);

	private:
		static unsigned int              get_new_tl_id( void );
		static std::atomic<unsigned int> tl_id_counter_;   //!< 各スレッド毎に生成されるIDをIDジェネレータ
	};
	/**
	 * @brief それぞれのスレッド終了時に実行する処理を担うfunctor
	 */
	struct tl_chunk_param_destructor {
		constexpr explicit tl_chunk_param_destructor( chunk_list* p_chlst_arg )
		  : p_chlst_( p_chlst_arg )
		{
		}

		uintptr_t allocate( void )
		{
			return reinterpret_cast<uintptr_t>( new tl_chunk_param( p_chlst_, p_chlst_->chunk_param_.num_of_pieces_ ) );
		}

		/*!
		 * @brief スレッド終了時等に呼び出されるI/F
		 */
		void deallocate( uintptr_t p_destructing_tls )
		{
			tl_chunk_param* p_tmp = reinterpret_cast<tl_chunk_param*>( p_destructing_tls );

			p_tmp->p_owner_chunk_list_->release_all_of_ownership( p_tmp->tl_id_, nullptr );

			delete p_tmp;
		}

		chunk_list* p_chlst_;
	};

	// TODO 仮実装
	class atomic_push_list {
	public:
		class forward_iterator {
		public:
			constexpr forward_iterator( chunk_header_multi_slot* p_arg )
			  : p_( p_arg )
			{
			}

			chunk_header_multi_slot& operator*( void )
			{
				return *p_;
			}
			chunk_header_multi_slot& operator->( void )
			{
				return *p_;
			}
			forward_iterator& operator++( void )
			{
				p_ = p_->p_next_chunk_.load( std::memory_order_acquire );
				return *this;
			}
			bool operator!=( const forward_iterator& v )
			{
				return p_ != v.p_;
			}

		private:
			chunk_header_multi_slot* p_;
		};

		constexpr atomic_push_list( void )
		  : p_top_( nullptr )
		{
		}
		~atomic_push_list()
		{
			chunk_header_multi_slot* p_chms = p_top_.load( std::memory_order_acquire );
			while ( p_chms != nullptr ) {
				chunk_header_multi_slot* p_next_chms = p_chms->p_next_chunk_.load( std::memory_order_acquire );
				delete p_chms;   // 実際には削除されない。割り当て専用アロケータのdestructorによって、まとめて破棄される。
				p_chms = p_next_chms;
			}
		}

		chunk_header_multi_slot* load( std::memory_order order )
		{
			return p_top_.load( order );
		}

		void push( chunk_header_multi_slot* p_new_chms )
		{
			chunk_header_multi_slot* p_cur_top = p_top_.load( std::memory_order_acquire );
			do {
				p_new_chms->p_next_chunk_.store( p_cur_top, std::memory_order_release );
			} while ( !p_top_.compare_exchange_weak( p_cur_top, p_new_chms ) );
		}

		inline forward_iterator begin( void )
		{
			return forward_iterator( p_top_.load( std::memory_order_acquire ) );
		}
		inline forward_iterator end( void )
		{
			return forward_iterator( nullptr );
		}

	private:
		std::atomic<chunk_header_multi_slot*> p_top_;
	};

	void mark_as_reserved_deletion(
		unsigned int             target_tl_id_arg,    //!< [in] 削除予約対象に変更するtl_id_
		chunk_header_multi_slot* p_non_deletion_arg   //!< [in] 削除予約対象としないchunk_header_multi_slotへのポインタ
	);

	void release_all_of_ownership(
		unsigned int                   target_tl_id_arg,         //!< [in] オーナー権を開放する対象のtl_id_
		const chunk_header_multi_slot* p_non_release_chunk_arg   //!< [in] オーナー権解放の対象外とするchunk_header_multi_slotへのポインタ。指定しない場合は、nullptrを指定すること。
	);

	size_t get_cur_max_slot_size(
		unsigned int target_tl_id_arg   //!< [in] オーナー権を開放する対象のtl_id_
	);

	internal::alloc_only_chamber*                          p_allocator_;   //!< 割り当て専用アロケータへのポインタ
	atomic_push_list                                       p_top_chunk_;   //!< pointer to chunk_header that is top of list.
	dynamic_tls<tl_chunk_param, tl_chunk_param_destructor> tls_hint_;      //!< thread local pointer to chunk_header that is success to allocate recently for a thread.
	                                                                       //!< tls_hint_は、p_top_chunk_を参照しているため、この2つのメンバ変数の宣言順(p_top_chunk_の次にtls_hint_)を入れ替えてはならない。
	                                                                       //!< 入れ替えてしまった場合、デストラクタでの解放処理で、解放済みのメモリ領域にアクセスしてしまう。

	chunk_list_statistics statistics_;   //!< statistics
};

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha

#endif /* INC_ALCONCURRENT_LF_MEM_ALLOC_IDX_MGR_HPP_ */
