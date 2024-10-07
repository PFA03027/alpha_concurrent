
# For gprof build

include(common.cmake)

set(CMAKE_C_FLAGS_DEBUG " -g -pg")
set(CMAKE_C_FLAGS_RELEASE " -g -pg -O2 -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_DEBUG " -g -pg")
set(CMAKE_CXX_FLAGS_RELEASE "-pg -O1")

