cmake_minimum_required(VERSION 3.16)


include(FetchContent)
FetchContent_Declare(
  googletest
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip
)
#   URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_custom_target(build-test-type)
add_custom_target(build-test)

add_subdirectory(test_type)
add_subdirectory(test_hazard_ptr)
add_subdirectory(test_dynamic_tls)
add_subdirectory(test_mem_alloc)
add_subdirectory(test_lf_fifo)
add_subdirectory(test_lf_stack)
add_subdirectory(test_lf_list)
add_subdirectory(test_msg_content)
add_subdirectory(test_conf)
add_subdirectory(test_od_node)
add_subdirectory(test_allocator)
add_subdirectory(test_profile_special)



