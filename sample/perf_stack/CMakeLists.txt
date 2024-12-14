set(EXEC_TARGET perf_stack)
include(../build_sample.cmake)

target_compile_features(${EXEC_TARGET} PRIVATE cxx_std_20)

