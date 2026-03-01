cmake_minimum_required(VERSION 3.16)

file(GLOB SOURCES src/*.cpp src_mem/*.cpp )
file(GLOB INSTALL_PUBLIC_HEADER_FILES  inc/alconcurrent/*.hpp )
file(GLOB INSTALL_PRIVATE_HEADER_FILES inc/alconcurrent/internal/*.hpp)


# GREETINGS_BUILD_SHARED_LIBSというオプションを作成。デフォルトをOFFに設定。
option(ALCONCURRENT_BUILD_SHARED_LIBS "build alconcurrent as a shared library" ON)

if (ALCONCURRENT_BUILD_SHARED_LIBS)
  add_library(alconcurrent SHARED ${SOURCES} )
else()
  add_library(alconcurrent STATIC ${SOURCES} )
endif()

target_include_directories(alconcurrent  PUBLIC inc/  )
target_include_directories(alconcurrent  PUBLIC src_mem/  ) # for test

set_target_properties(alconcurrent PROPERTIES PUBLIC_HEADER  "${INSTALL_PUBLIC_HEADER_FILES}")
set_target_properties(alconcurrent PROPERTIES PRIVATE_HEADER "${INSTALL_PRIVATE_HEADER_FILES}")

include(GNUInstallDirs)
install(TARGETS alconcurrent
        PUBLIC_HEADER
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/alconcurrent
        PRIVATE_HEADER
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/alconcurrent/internal
)
