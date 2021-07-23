
file(GLOB SOURCES src/*.cpp )
add_executable(test_lf_list ${SOURCES})

target_link_libraries(test_lf_list alconcurrent gtest gtest_main pthread)

