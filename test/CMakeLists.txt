cmake_minimum_required(VERSION 3.16)

find_package(GTest)
find_package(Git QUIET)

# Gitがインストールされており、トップディレクトリに.gitディレクトリがある
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    enable_testing()
    option(GOOGLETEST "Check Google Test during build" ON)
    # -DGOOGLETEST=onオプションを付けて実行したらsubmoduleを最新版にする
    if(GOOGLETEST)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()

if(EXISTS "${PROJECT_SOURCE_DIR}/test/googletest/CMakeLists.txt")
    add_custom_target(build-test-type)
    add_subdirectory(test_type)

    add_custom_target(build-test)

    add_subdirectory(googletest)
    add_subdirectory(test_hazard_ptr)
    add_subdirectory(test_dynamic_tls)
    add_subdirectory(test_mem_alloc_sanitize)
    add_subdirectory(test_mem_alloc)
    add_subdirectory(test_lf_fifo)
    add_subdirectory(test_lf_stack)
    add_subdirectory(test_lf_one_side_deque)
    add_subdirectory(test_lf_list)
    add_subdirectory(test_msg_content)
    add_subdirectory(test_conf)
    add_subdirectory(test_allocator)
    add_subdirectory(test_profile_special)
else()
    message("The submodules were not downloaded! GOOGLETEST was turned off or failed. Skip unit test build configuration.")
endif()



