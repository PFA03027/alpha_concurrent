cmake_minimum_required(VERSION 3.16)
project(
    alpha_concurrent
    #
    DESCRIPTION "libalconcurrent is Semi lock-free concurrent software asset"
    LANGUAGES CXX
    VERSION 2.0.5
    )

set(CMAKE_VERBOSE_MAKEFILE 1)

include(common.cmake)

# Switch build configuration
set(BUILD_CONFIG normal CACHE STRING "Build configuration?")
message("[BUILD] ${BUILD_CONFIG}")
include(${BUILD_CONFIG}.cmake)


enable_testing()

add_subdirectory(libalconcurrent)

add_subdirectory(test)
add_subdirectory(sample)
