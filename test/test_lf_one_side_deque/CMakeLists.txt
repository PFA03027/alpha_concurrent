
file(GLOB SOURCES src/*.cpp )
add_executable(test_lf_one_side_deque ${SOURCES})

target_link_libraries(test_lf_one_side_deque alconcurrent gtest gtest_main pthread)

