
file(GLOB SOURCES src/*.cpp )
add_executable(test_lf_stack ${SOURCES})

target_link_libraries(test_lf_stack alconcurrent gtest gtest_main pthread)

