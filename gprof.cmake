
# For gprof build

set(CMAKE_C_FLAGS_DEBUG " -g -pg -fno-omit-frame-pointer")
set(CMAKE_C_FLAGS_RELEASE " -g -pg -O1 -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_DEBUG " -g -pg -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "-pg -O1 -fno-omit-frame-pointer")

