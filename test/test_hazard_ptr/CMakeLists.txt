
file(GLOB SOURCES src/*.cpp )
add_executable(test_hazard_ptr ${SOURCES})

target_link_libraries(test_hazard_ptr alconcurrent gtest gtest_main pthread)

