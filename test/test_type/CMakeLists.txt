set(EXEC_TARGET test_type)

file(GLOB SOURCES src/*.cpp )

add_executable(${EXEC_TARGET} EXCLUDE_FROM_ALL ${SOURCES})

target_link_libraries(${EXEC_TARGET} alconcurrent gtest gtest_main pthread)

add_dependencies(build-test-type ${EXEC_TARGET})
