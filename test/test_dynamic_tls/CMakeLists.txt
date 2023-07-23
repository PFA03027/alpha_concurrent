set(EXEC_TARGET test_dynamic_tls)

include(../build_test.cmake)

target_include_directories(test_dynamic_tls  PUBLIC ../../libalconcurrent/src  )

