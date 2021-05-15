/*!
 * @file	lf_mem_alloc.hpp
 * @brief	semi lock-free memory allocator
 * @author	alpha
 * @date	Created on 2021/05/12
 * @details
 *
 * Copyright (C) 2021 by alpha <e-mail address>
 */

#ifndef INC_ALCONCURRENT_LF_MEM_ALLOC_HPP_
#define INC_ALCONCURRENT_LF_MEM_ALLOC_HPP_

#include <atomic>
#include <cstdlib>
#include <memory>

#include "alconcurrent/free_node_storage.hpp"
#include "alconcurrent/lf_stack.hpp"

namespace alpha {
namespace concurrent {

/*!
 * @breif	configuration paramters when allocate chunk
 */
struct param_chunk_allocation {
	std::size_t size_of_one_piece_ = 0;   //!< size of one piece in a chunk
	std::size_t num_of_pieces_     = 0;   //!< number of pieces in a chunk
};

/*!
 * @breif	chunk control status
 */
enum class chunk_control_status {
	EMPTY,                 //!< chunk header has no allocated chunk memory.
	RESERVED_ALLOCATION,   //!< chunk header has no allocated chunk memory. But some one start to allocation
	NORMAL,                //!< allow to allocate the memory from this chunk
	RESERVED_DELETION,     //!< does not allow to allocate the memory from this chunk. But if needed to reuse this chunk, allow to change NORMAL
	DELETION,              //!< does not allow to access any more except GC. After shift to this state, chunk memory will be free after confirmed accesser is zero.
};

class slot_release_handler {
public:
	using pointer = void*;
	void operator()( pointer& x )
	{
		x = nullptr;
	}
};

/*!
 * @breif	management information of a chunk
 */
class chunk_header_multi_slot {
public:
	std::atomic<chunk_header_multi_slot*> p_next_chunk_;      //!< pointer to next chunk header. chunk header does not free. therefore we don't need to consider ABA.
	std::atomic<chunk_control_status>     status_;            //!< chunk status for GC
	std::atomic<int>                      num_of_accesser_;   //!< number of accesser

	/*!
	 * @breif	constructor
	 */
	chunk_header_multi_slot(
		const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
	);

	/*!
	 * @breif	destructor
	 */
	~chunk_header_multi_slot();

	/*!
	 * @breif	allocate new memory slot
	 *
	 * @return	pointer to an allocated memory slot
	 * @retval	non-nullptr	success to allocate and it is a pointer to an allocated memory
	 * @retval	nullptr		fail to allocate
	 */
	void* allocate_mem_slot( void );

	/*!
	 * @breif	recycle memory slot
	 *
	 * @retval	true	success to recycle.
	 * @retval	false	fail to recycle. Normally p_recycle_slot does not belong to this chunk
	 */
	bool recycle_mem_slot(
		void* p_recycle_slot   //!< [in] pointer to the memory slot to recycle.
	);

	/*!
	 * @breif	parameter of this chunk
	 */
	const param_chunk_allocation& get_param( void ) const;

	bool set_delete_reservation( void );
	bool unset_delete_reservation( void );
	bool exec_deletion( void );
	bool exec_allocation( void );

	/*!
	 * @breif	allocate new chunk
	 */
	bool alloc_new_chunk( void );

private:
	param_chunk_allocation                        alloc_conf_;          //!< allocation configuration paramter
	std::size_t                                   size_of_chunk_ = 0;   //!< size of a chunk
	stack_list<void*, true, slot_release_handler> free_slot_stack_;     //!< free slot stack

	void* p_chunk_ = nullptr;   //!< pointer to an allocated memory as a chunk

	/*!
	 * @breif	allocate new chunk
	 */
	bool alloc_new_chunk(
		const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
	);

	std::size_t get_size_of_one_slot( void ) const;
};

/*!
 * @breif	chunk list that has same allocation parameter
 */
class chunk_list {
public:
	/*!
	 * @breif	constructor
	 */
	chunk_list(
		const param_chunk_allocation& ch_param_arg   //!< [in] chunk allocation paramter
	);

	/*!
	 * @breif	destructor
	 */
	~chunk_list();

	/*!
	 * @breif	allocate new memory slot
	 *
	 * @return	pointer to an allocated memory slot
	 * @retval	non-nullptr	success to allocate and it is a pointer to an allocated memory
	 * @retval	nullptr		fail to allocate
	 */
	void* allocate_mem_slot( void );

	/*!
	 * @breif	recycle memory slot
	 *
	 * @retval	true	success to recycle.
	 * @retval	false	fail to recycle. Normally p_recycle_slot does not belong to this chunk
	 */
	bool recycle_mem_slot(
		void* p_recycle_slot   //!< [in] pointer to the memory slot to recycle.
	);

	/*!
	 * @breif	parameter of this chunk
	 */
	const param_chunk_allocation& get_param( void ) const;

private:
	param_chunk_allocation                alloc_conf_;    //!< allocation configuration paramter
	std::atomic<chunk_header_multi_slot*> p_top_chunk_;   //!< pointer to chunk_header that is top of list.
};

/*!
 * @breif	semi lock-free memory allocator based on multi chunk size list
 *
 * If requested size is over max size that parameter has, just call malloc()
 */
class general_mem_allocator {
public:
	/*!
	 * @breif	constructor
	 */
	general_mem_allocator(
		const param_chunk_allocation* p_param_array,   //!< [in] pointer to parameter array
		int                           num              //!< [in] array size
	);

	/*!
	 * @breif	constructor
	 */
	template <int N>
	general_mem_allocator(
		const param_chunk_allocation param_array[N]   //!< [in] param array
		)
	  : general_mem_allocator( param_array, N )
	{
	}

	/*!
	 * @breif	allocate memory
	 */
	void* allocate( std::size_t n );

	/*!
	 * @breif	deallocate memory
	 */
	void deallocate( void* p_mem );

private:
	struct param_chunk_comb {
		param_chunk_allocation      param_;          //!< parametor for chunk
		std::unique_ptr<chunk_list> up_chunk_lst_;   //!< unique pointer to chunk list
	};

	int                                 pr_ch_size_;          // array size of chunk and param array
	std::unique_ptr<param_chunk_comb[]> up_param_ch_array_;   //!< unique pointer to chunk and param array
};

}   // namespace concurrent
}   // namespace alpha

#endif /* INC_ALCONCURRENT_LF_MEM_ALLOC_HPP_ */
