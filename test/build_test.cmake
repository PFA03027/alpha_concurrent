# Common code for test execuable

file(GLOB SOURCES src/*.cpp )

add_executable(${EXEC_TARGET} EXCLUDE_FROM_ALL ${SOURCES})

target_include_directories(${EXEC_TARGET} PRIVATE ../../libalconcurrent/src)
target_include_directories(${EXEC_TARGET} PRIVATE ../../libalconcurrent/src_mem)
target_include_directories(${EXEC_TARGET} PRIVATE ../test_common_inc)

target_link_libraries(${EXEC_TARGET} alconcurrent gtest gtest_main pthread)

add_dependencies(build-test ${EXEC_TARGET})

add_test(NAME ${EXEC_TARGET} COMMAND $<TARGET_FILE:${EXEC_TARGET}>)
#gtest_discover_tests(${EXEC_TARGET})
