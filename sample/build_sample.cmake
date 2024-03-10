# Common code for test execuable

file(GLOB SOURCES *.cpp )

add_executable(${EXEC_TARGET} EXCLUDE_FROM_ALL ${SOURCES})

target_link_libraries(${EXEC_TARGET} alconcurrent pthread)

add_dependencies(build-sample ${EXEC_TARGET})

# add_test(NAME ${EXEC_TARGET} COMMAND $<TARGET_FILE:${EXEC_TARGET}>)
