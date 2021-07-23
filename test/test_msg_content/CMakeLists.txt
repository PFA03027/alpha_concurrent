
file(GLOB SOURCES src/*.cpp )
add_executable(test_msg_content ${SOURCES})

target_link_libraries(test_msg_content alconcurrent gtest gtest_main pthread)

