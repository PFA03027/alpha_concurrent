cmake_minimum_required(VERSION 3.16)
project(libalconcurrent CXX)

file(GLOB SOURCES src/*.cpp src_mem/*.cpp )

# GREETINGS_BUILD_SHARED_LIBSというオプションを作成。デフォルトをOFFに設定。
option(ALCONCURRENT_BUILD_SHARED_LIBS "build alconcurrent as a shared library" OFF)

if (ALCONCURRENT_BUILD_SHARED_LIBS)
  add_library(alconcurrent SHARED ${SOURCES} )
else()
  add_library(alconcurrent STATIC ${SOURCES} )
endif()


target_include_directories(alconcurrent  PUBLIC inc/  )
target_include_directories(alconcurrent  PUBLIC src_mem/  )

