cmake_minimum_required(VERSION 3.1)
project(alpha_concurrent CXX)
set(CMAKE_VERBOSE_MAKEFILE 1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

# Switch build target
set(BUILD_TARGET common CACHE STRING "Build target?")
message("[BUILD] ${BUILD_TARGET}")
include(${BUILD_TARGET}.cmake)


enable_testing()

add_subdirectory(libalconcurrent)

add_subdirectory(test)
