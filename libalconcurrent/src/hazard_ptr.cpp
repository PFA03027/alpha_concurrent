/*
 * hazard_ptr.cpp
 *
 *  Created on: 2020/12/29
 *      Author: alpha
 */

#include <pthread.h>
#include <semaphore.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include "hazard_ptr.hpp"

namespace alpha {
namespace concurrent {

namespace hazard_ptr_internal {

node_for_delete_ptr<hazard_node_glist_base> hazard_node_glist_base::head_node_glist_( nullptr );

class Garbage_collector {
public:
	static Garbage_collector& get_instance( void );

	std::mutex glist_access_mtx_;

	bool do_continue( void )
	{
		std::atomic_thread_fence( std::memory_order_acquire );
		return loop_flag;
	}

	~Garbage_collector()
	{
		loop_flag = false;
		std::atomic_thread_fence( std::memory_order_release );

		post_trigger_gc();

		thd_garbage_collector.join();
	}

	void post_trigger_gc( void )
	{
		int ret = sem_post( &sem_t_garbage_collection_trigger );

		//		printf( "sem_post has done\n" );
		fflush( NULL );

		if ( ret != 0 ) {
			printf( "Error: fail to semaphore post. errno=%d\n", errno );
		}
	}

	static void thread_Garbage_collector( void );

private:
	Garbage_collector( void )
	{
		int ret = sem_init( &sem_t_garbage_collection_trigger, 0, 0 );
		if ( ret != 0 ) {
			printf( "Error: fail to semaphore init. errno=%d\n", errno );
			exit( 1 );
		}

		thd_garbage_collector = std::move( std::thread( thread_Garbage_collector ) );
	}

	sem_t sem_t_garbage_collection_trigger;

	std::thread thd_garbage_collector;

	bool loop_flag = true;
};

Garbage_collector& Garbage_collector::get_instance( void )
{
	static Garbage_collector singleton;

	return singleton;
}

void Garbage_collector::thread_Garbage_collector( void )
{
	//	printf( "GC thread has started\n" );
	while ( get_instance().do_continue() ) {
		sem_wait( &get_instance().sem_t_garbage_collection_trigger );

		//		printf( "GC starts now\n" );

		{
			std::lock_guard<std::mutex> lock( get_instance().glist_access_mtx_ );

			//			printf( "get glist lock now\n" );

			node_for_delete_ptr<hazard_node_glist_base>* p_cur = hazard_node_glist_base::head_node_glist_.get_next();
			while ( p_cur != nullptr ) {
				if ( !( p_cur->is_emptry() ) ) {
					hazard_node_glist_base* p_glist = p_cur->get_delete_ptr();
					p_glist->try_clean_up_delete_ptr();
				}
				p_cur = p_cur->get_next();
			}
		}

		//		printf( "GC done now\n" );
	}

	return;
}

/*!
 * @breif	新しいノードを用意し、削除リストに追加する。
 *
 * 空ノードを追加する場合は、引数にnullptrを指定する。
 */
void hazard_node_glist_base::add_one_new_glist_node( void )
{
	node_for_delete_ptr<hazard_node_glist_base>* p_ans = new node_for_delete_ptr<hazard_node_glist_base>( this );
	node_for_delete_ptr<hazard_node_glist_base>* p_next_check;
	p_next_check = head_node_glist_.get_next();
	while ( true ) {
		p_ans->set_next( p_next_check );
		if ( head_node_glist_.next_CAS( &p_next_check, p_ans ) ) {
			break;
		}
	}

	return;
}

void hazard_node_glist_base::regist_self_to_list( void )
{
	// 空きノードを探す。
	node_for_delete_ptr<hazard_node_glist_base>* p_ans = head_node_glist_.get_next();
	while ( p_ans != nullptr ) {
		if ( p_ans->is_emptry() ) {
			if ( p_ans->try_to_set_delete_ptr( this ) ) {
				return;
			}
		}
		p_ans = p_ans->get_next();
	}

	// 空きノードが見つからなかったので、新しいノードを用意する。
	add_one_new_glist_node();

	//		printf( "glist is added.\n" );
	return;
}

void hazard_node_glist_base::deregist_self_from_list( void )
{
	node_for_delete_ptr<hazard_node_glist_base>* p_cur_node = head_node_glist_.get_next();
	while ( p_cur_node != nullptr ) {
		std::atomic_thread_fence( std::memory_order_acquire );
		if ( p_cur_node->get_delete_ptr() == this ) {
			p_cur_node->clear_delete_ptr();
			break;
		}
		p_cur_node = p_cur_node->get_next();
	}

	if ( p_cur_node == nullptr ) {
		printf( "Error: fail to find own pointer in list.\n" );
	}

	return;

	return;
}

void hazard_node_glist_base::post_trigger_gc( void )
{
	Garbage_collector::get_instance().post_trigger_gc();
	return;
}

hazard_node_glist_base::hazard_node_glist_base( void )
{
	regist_self_to_list();
	return;
}

hazard_node_glist_base::~hazard_node_glist_base()
{
	std::lock_guard<std::mutex> lock( Garbage_collector::get_instance().glist_access_mtx_ );

	//	printf( "glist destructor is called\n" );

	deregist_self_from_list();

	return;
}

}   // namespace hazard_ptr_internal
}   // namespace concurrent
}   // namespace alpha
