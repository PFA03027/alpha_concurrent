cmake_minimum_required(VERSION 3.1)
project(alpha_concurrent CXX)

# Switch build target
set(BUILD_TARGET common CACHE STRING "Build target?")

message("[BUILD] ${BUILD_TARGET}")
include(${BUILD_TARGET}.cmake)

add_subdirectory(libalconcurrent)
add_subdirectory(test)
