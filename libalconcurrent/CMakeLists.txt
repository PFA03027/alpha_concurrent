cmake_minimum_required(VERSION 3.1)
project(libalconcurrent CXX)

file(GLOB SOURCES src/*.cpp )
add_library(alconcurrent ${SOURCES} )


target_include_directories(alconcurrent  PUBLIC inc  )
#target_compile_definitions(alconcurrent PRIVATE CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_INFO)
#target_compile_definitions(alconcurrent PRIVATE CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DEBUG)
#target_compile_definitions(alconcurrent PRIVATE CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_TEST)
target_compile_definitions(alconcurrent PRIVATE CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DUMP)
#target_compile_definitions(alconcurrent PRIVATE NOT_USE_LOCK_FREE_MEM_ALLOC)
