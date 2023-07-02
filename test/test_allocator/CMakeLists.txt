set(EXEC_TARGET test_allocator)

include(../build_test.cmake)

target_include_directories(test_allocator PRIVATE ../../libalconcurrent/src)

