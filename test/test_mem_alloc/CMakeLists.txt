
file(GLOB SOURCES src/*.cpp )
add_executable(test_mem_alloc ${SOURCES})

target_link_libraries(test_mem_alloc alconcurrent gtest gtest_main pthread)

