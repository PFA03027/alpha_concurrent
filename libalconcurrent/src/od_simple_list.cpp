/**
 * @file od_simple_list.cpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-10-20
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#include <memory>

#include "alconcurrent/internal/od_node_essence.hpp"
#include "alconcurrent/internal/od_simple_list.hpp"

namespace alpha {
namespace concurrent {
namespace internal {

od_simple_list::od_simple_list( od_simple_list&& src ) noexcept
  : p_head_( src.p_head_ )
  , p_tail_( src.p_tail_ )
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
  , count_( src.count_ )
#endif
{
	src.p_head_ = nullptr;
	src.p_tail_ = nullptr;
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	src.count_ = 0;
#endif

#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_TAIL_NODE_NEXT_NULLPTR
	if ( p_tail_ != nullptr ) {
		if ( p_tail_->next() != nullptr ) {
			LogOutput( log_type::ERR, "tail node has non-nullptr in next" );
			std::terminate();
		}
	}
#endif
}
od_simple_list& od_simple_list::operator=( od_simple_list&& src ) noexcept
{
	od_simple_list( std::move( src ) ).swap( *this );

#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_TAIL_NODE_NEXT_NULLPTR
	if ( p_tail_ != nullptr ) {
		if ( p_tail_->next() != nullptr ) {
			LogOutput( log_type::ERR, "tail node has non-nullptr in next" );
			std::terminate();
		}
	}
#endif

	return *this;
}
od_simple_list::~od_simple_list()
{
	clear();
}

void od_simple_list::swap( od_simple_list& src ) noexcept
{
	od_node_simple_link* p_tmp;

	p_tmp       = p_head_;
	p_head_     = src.p_head_;
	src.p_head_ = p_tmp;

	p_tmp       = p_tail_;
	p_tail_     = src.p_tail_;
	src.p_tail_ = p_tmp;

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	size_t tmp_cnt;
	tmp_cnt    = count_;
	count_     = src.count_;
	src.count_ = tmp_cnt;
#endif

#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_TAIL_NODE_NEXT_NULLPTR
	if ( p_tail_ != nullptr ) {
		if ( p_tail_->next() != nullptr ) {
			LogOutput( log_type::ERR, "tail node has non-nullptr in next" );
			std::terminate();
		}
	}
#endif
}

void od_simple_list::push_front( od_node_simple_link* p_nd ) noexcept
{
	if ( p_nd == nullptr ) return;
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_PUSH_FRONT_FUNCTION_NULLPTR
	if ( p_nd->next() != nullptr ) {
		LogOutput( log_type::WARN, "od_simple_list::push_front() receives a od_node<T> that has non nullptr in next" );
		p_nd->set_next( nullptr );
	}
#endif
	if ( p_head_ == nullptr ) {
		p_tail_ = p_nd;
	}
	od_node_simple_link* p_nd_link_t = p_nd;
	p_nd_link_t->set_next( p_head_ );
	p_head_ = p_nd_link_t;

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	count_++;
#endif
}

void od_simple_list::push_back( od_node_simple_link* p_nd ) noexcept
{
	if ( p_nd == nullptr ) return;
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_PUSH_FRONT_FUNCTION_NULLPTR
	if ( p_nd->next() != nullptr ) {
		LogOutput( log_type::WARN, "od_simple_list::push_back() receives a od_node<T> that has non nullptr in next" );
		p_nd->set_next( nullptr );
	}
#endif
	if ( p_head_ == nullptr ) {
		p_head_ = p_nd;
		p_tail_ = p_nd;
	} else {
		p_tail_->set_next( p_nd );
		p_tail_ = p_nd;
	}

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	count_++;
#endif
}

void od_simple_list::merge_push_front( od_simple_list&& src ) noexcept
{
	if ( src.p_head_ == nullptr ) return;

	od_node_simple_link* p_src_head = src.p_head_;
	od_node_simple_link* p_src_tail = src.p_tail_;
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	size_t tmp_cnt = src.count_;
	src.count_     = 0;
#endif
	src.p_head_ = nullptr;
	src.p_tail_ = nullptr;

	merge_push_front( p_src_head, p_src_tail );
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	count_ += tmp_cnt;
#endif
}

void od_simple_list::merge_push_front( od_node_simple_link* p_nd ) noexcept
{
	if ( p_nd == nullptr ) return;

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	size_t tmp_cnt = 0;
#endif
	od_node_simple_link* p_cur = p_nd;
	od_node_simple_link* p_nxt = p_cur->next();
	while ( p_nxt != nullptr ) {
		p_cur = p_nxt;
		p_nxt = p_cur->next();
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		tmp_cnt++;
#endif
	}

	merge_push_front( p_nd, p_cur );
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	count_ += tmp_cnt;
#endif
}

void od_simple_list::merge_push_back( od_simple_list&& src ) noexcept
{
	if ( src.p_head_ == nullptr ) return;

	od_node_simple_link* p_src_head = src.p_head_;
	od_node_simple_link* p_src_tail = src.p_tail_;
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	size_t tmp_cnt = src.count_;
	src.count_     = 0;
#endif
	src.p_head_ = nullptr;
	src.p_tail_ = nullptr;

	merge_push_back( p_src_head, p_src_tail );
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	count_ += tmp_cnt;
#endif
}

void od_simple_list::merge_push_back( od_node_simple_link* p_nd ) noexcept
{
	if ( p_nd == nullptr ) return;

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	size_t tmp_cnt = 0;
#endif
	od_node_simple_link* p_cur = p_nd;
	od_node_simple_link* p_nxt = p_cur->next();
	while ( p_nxt != nullptr ) {
		p_cur = p_nxt;
		p_nxt = p_cur->next();
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
		tmp_cnt++;
#endif
	}

	merge_push_back( p_nd, p_cur );
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	count_ += tmp_cnt;
#endif
}

od_node_simple_link* od_simple_list::pop_front( void ) noexcept
{
	od_node_simple_link* p_ans = p_head_;
	if ( p_ans == nullptr ) return nullptr;

	p_head_ = p_ans->next();
	if ( p_head_ == nullptr ) {
		p_tail_ = nullptr;
	}

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	count_--;
#endif

	p_ans->set_next( nullptr );
	return p_ans;
}

void od_simple_list::clear( void )
{
	clear( std::default_delete<od_node_simple_link> {} );
}

od_simple_list od_simple_list::split_if( std::function<bool( const od_node_simple_link* )> pred )
{
	od_simple_list ans;

#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_TAIL_NODE_NEXT_NULLPTR
	if ( p_tail_ != nullptr ) {
		if ( p_tail_->next() != nullptr ) {
			LogOutput( log_type::ERR, "tail node has non-nullptr in next" );
			std::terminate();
		}
	}
#endif

	od_node_simple_link* p_pre = nullptr;
	od_node_simple_link* p_cur = p_head_;
	while ( p_cur != nullptr ) {
		od_node_simple_link* p_next = p_cur->next();
		if ( pred( p_cur ) ) {
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
			count_--;
#endif
			if ( p_pre == nullptr ) {
				p_head_ = p_next;
				if ( p_head_ == nullptr ) {
					p_tail_ = nullptr;
				}

				p_cur->set_next( nullptr );
				ans.push_back( p_cur );

				p_cur = p_head_;
			} else {
				p_pre->set_next( p_next );
				if ( p_next == nullptr ) {
					p_tail_ = p_pre;
				}

				p_cur->set_next( nullptr );
				ans.push_back( p_cur );

				p_cur = p_next;
			}
		} else {
			p_pre = p_cur;
			p_cur = p_next;
		}
	}

	return ans;
}

void od_simple_list::clear( std::function<void( od_node_simple_link* )> pred )
{
	od_node_simple_link* p_cur = p_head_;
	p_head_                    = nullptr;
	p_tail_                    = nullptr;
	while ( p_cur != nullptr ) {
		od_node_simple_link* p_nxt = p_cur->next();
		p_cur->set_next( nullptr );
		pred( p_cur );
		p_cur = p_nxt;
	}

#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	count_ = 0;
#endif
}

size_t od_simple_list::profile_info_count( void ) const
{
#ifdef ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE
	if ( count_ == 0 ) {
		if ( p_head_ != nullptr ) {
			throw std::runtime_error( "internal error: count is zero, but p_head_ is not nullptr. counting and/or internal status is unexpected" );
		}
		if ( p_tail_ != nullptr ) {
			throw std::runtime_error( "internal error: count is zero, but p_tail_ is not nullptr. counting and/or internal status is unexpected" );
		}
	} else if ( count_ == 1 ) {
		if ( p_head_ != p_tail_ ) {
			throw std::runtime_error( "internal error: count is one, but p_head_ is not same to p_tail_. counting and/or internal status is unexpected" );
		}
	}
	return count_;
#else
	return 0;
#endif
}

void od_simple_list::merge_push_front( od_node_simple_link* p_nd_head, od_node_simple_link* p_nd_tail ) noexcept
{
	if ( p_head_ == nullptr ) {
		p_head_ = p_nd_head;
		p_tail_ = p_nd_tail;
	} else {
		p_nd_tail->set_next( p_head_ );
		p_head_ = p_nd_head;
	}

#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_TAIL_NODE_NEXT_NULLPTR
	if ( p_tail_ != nullptr ) {
		if ( p_tail_->next() != nullptr ) {
			LogOutput( log_type::ERR, "tail node has non-nullptr in next" );
			std::terminate();
		}
	}
#endif
}

void od_simple_list::merge_push_back( od_node_simple_link* p_nd_head, od_node_simple_link* p_nd_tail ) noexcept
{
#ifdef ALCONCURRENT_CONF_ENABLE_CHECK_TAIL_NODE_NEXT_NULLPTR
	if ( p_nd_tail != nullptr ) {
		if ( p_nd_tail->next() != nullptr ) {
			LogOutput( log_type::ERR, "tail node has non-nullptr in next" );
			std::terminate();
		}
	}
#endif

	if ( p_head_ == nullptr ) {
		p_head_ = p_nd_head;
		p_tail_ = p_nd_tail;
	} else {
		p_tail_->set_next( p_nd_head );
		p_tail_ = p_nd_tail;
	}
}

}   // namespace internal
}   // namespace concurrent
}   // namespace alpha
