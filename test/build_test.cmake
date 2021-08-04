# Common code for test execuable

file(GLOB SOURCES src/*.cpp )

add_executable(${EXEC_TARGET} EXCLUDE_FROM_ALL ${SOURCES})

target_link_libraries(${EXEC_TARGET} alconcurrent gtest gtest_main pthread)

add_dependencies(build-test ${EXEC_TARGET})

add_test(NAME ${EXEC_TARGET} COMMAND $<TARGET_FILE:${EXEC_TARGET}>)
