set(EXEC_TARGET test_od_node)

include(../build_test.cmake)

target_compile_features(${EXEC_TARGET} PRIVATE cxx_std_20)
