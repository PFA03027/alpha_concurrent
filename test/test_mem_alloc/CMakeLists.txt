set(EXEC_TARGET test_mem_alloc)

include(../build_test.cmake)

target_include_directories(test_mem_alloc  PUBLIC ../../libalconcurrent/src  )

