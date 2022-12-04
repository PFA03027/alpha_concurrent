cmake_minimum_required(VERSION 3.1)
project(libalconcurrent CXX)

file(GLOB SOURCES src/*.cpp )
add_library(alconcurrent ${SOURCES} )


target_include_directories(alconcurrent  PUBLIC inc  )

