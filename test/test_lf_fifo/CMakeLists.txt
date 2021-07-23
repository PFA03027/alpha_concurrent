
file(GLOB SOURCES src/*.cpp )
add_executable(test_lf_fifo ${SOURCES})

target_link_libraries(test_lf_fifo alconcurrent gtest gtest_main pthread)

