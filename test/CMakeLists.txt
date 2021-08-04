
add_custom_target(build-test)
#add_custom_target(test DEPENDS build-test)


add_subdirectory(googletest)
add_subdirectory(test_hazard_ptr)
add_subdirectory(test_lf_fifo)
add_subdirectory(test_lf_list)
add_subdirectory(test_lf_one_side_deque)
add_subdirectory(test_lf_stack)
add_subdirectory(test_mem_alloc)
add_subdirectory(test_msg_content)


