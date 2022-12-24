# libalconcurrent is Semi lock-free concurrent software asset
The purpose of libalconcurrent library provides semi lock-free algorithms and semi lock-free memory allocator.
If you are possible to design the necessary memory size, semi lock-free algorithms are mostly same behavior of lock-free.

# Pre-requirement
* C++11 standard and standard C++ library are required.
* POSIX pthread thread local storage API is required.

## Supplement
C++14 or newer C++ standard is better to compile.

# fifo_list class in lf_fifo.hpp
Semi-lock free FIFO type queue

Template 1st parameter T should be copy assignable.

In case of no avialable free node that carries a value, new node is allocated from heap internally.
In this case, this queue may be locked. And push() may trigger this behavior.

On the other hand, used free node will be recycled without a memory allocation. In this case, push() is lock free.

To reduce lock behavior, pre-allocated nodes are effective.
get_allocated_num() provides the number of the allocated nodes. This value is hint to configuration.

If you configure the paramter of node allocation by alpha::concurrent::set_param_to_free_nd_mem_alloc(),
semi lock-free memory allocater(lf_mem_alloc class) is used to allocate node.
Therefore lock behavior will be reduced more.

# stack_list class in lf_stack.hpp
Semi-lock free Stack type queue

Template 1st parameter T should be copy assignable.

In case of no avialable free node that carries a value, new node is allocated from heap internally.
In this case, this queue may be locked. And push() may trigger this behavior.

On the other hand, used free node will be recycled without a memory allocation. In this case, push() is lock free.

To reduce lock behavior, pre-allocated nodes are effective.
get_allocated_num() provides the number of the allocated nodes. This value is hint to configuration.

If you configure the paramter of node allocation by alpha::concurrent::set_param_to_free_nd_mem_alloc(),
semi lock-free memory allocater(lf_mem_alloc class) is used to allocate node.
Therefore lock behavior will be reduced more.

# lockfree_list class in lf_list.hpp
Semi-lock free list

Template 1st parameter T should be copy assignable.

In case of no avialable free node that carries a value, new node is allocated from heap internally.
In this case, this queue may be locked. And push_front()/push_back()/insert() may trigger this behavior.

On the other hand, used free node will be recycled without a memory allocation. In this case, push_front()/push_back()/insert() is lock free.

To reduce lock behavior, pre-allocated nodes are effective.
get_allocated_num() provides the number of the allocated nodes. This value is hint to configuration.

If you configure the paramter of node allocation by alpha::concurrent::set_param_to_free_nd_mem_alloc(),
semi lock-free memory allocater(lf_mem_alloc class) is used to allocate node.
Therefore lock behavior will be reduced more.

# one_side_deque class in lf_one_side_deque.hpp
Semi-lock free one side deque

Template 1st parameter T should be copy assignable.

In case of no avialable free node that carries a value, new node is allocated from heap internally.
In this case, this queue may be locked. And push_front()/push_back() may trigger this behavior.

On the other hand, used free node will be recycled without a memory allocation. In this case, push_front()/push_back() is lock free.

To reduce lock behavior, pre-allocated nodes are effective.
get_allocated_num() provides the number of the allocated nodes. This value is hint to configuration.

If you configure the paramter of node allocation by alpha::concurrent::set_param_to_free_nd_mem_alloc(),
semi lock-free memory allocater(lf_mem_alloc class) is used to allocate node.
Therefore lock behavior will be reduced more.

# Supplement
To resolve ABA issue, this FIFO / Stack / list uses hazard pointer approach.

Non lock free behavior cases are below;

* Construct a instance itself.
* Any initial API call by each thread.
* pop_front()/push_front()/push_back() call in case of no free internal node.
  * In case that template parameter ALLOW_TO_ALLOCATE is false, these API does not allocate internal node. therefore push()/push_front()/push_back()/insert() is lock free.


# dynamic_tls class in dynamic_tls.hpp
Support dynamic allocatable thread local storage.

When allocating dynamic allocatable thread local storage for a thread, it is not lock free. This will be happened by 1st access of a value instance.
After allocation, this class may be lock free. This depends whether pthread_getspecific() is lock free or not.

# general memory allocator class that is semi lock-free in lf_mem_alloc.hpp
This is general memory allocator to get lock-free behavior and to avoid memory fragmentation.
The current implementation requires a small additional overhead compared to malloc()/free().

Configured of memory is kept to re-use.
If the required size is over the max size of configuration paramter, it allocates directly from malloc() and free it by free() also.

# Important points
Whether the provided class is lock-free depends on whether the POSIX API for thread-local storage is lock-free.

If the POSIX thread-local storage API is lock-free, the main operations such as push () / pop () will behave as lock-free.

# Build
There is 2way for build
1. build by make  
Even if your build environment has no cmake, at least you could make libalconcurrent at least
2. build by cmake  
This build way builds not only libalconcurrent but also test code is possible to build.

## Build by make
### How to build libalconcurrent.a by make
1. Change the current directory to libalconcurrent
2. Execute "make all"

There is no platform specific code. Therefore I expect to build this regardless Linux/Windows.
If you would like to build this library as a shared library on your platform, please modify Makefile.

### To install your system
Current libalconcurrent builder is not prepared installing logic.
Therefore please do below by your build system or manual operation.
1. copy the folder libalconcurrent/inc/alconcurrent into your expected header file directory
2. copy the library file "libalconcurrent/libalconcurrent.a" into your expected library files directory

If you build libalconcurrent as a shared library, please copy it.

## How to build by cmake
### Pre-condition:
1. Checkout googletest.  
Because libalconcurrent includes googletset as submodule, please execute below to checkout googletest;  
        $ git submodule update --init --recursive  
Currently, libalconcurrent uses googletest v1.11.0.

2. Please prepare cmake on your system.  
In case of Windows system, please download cmake windows binary from https://cmake.org/download/.  
After install, please copy xxx/CMake/yyy to zzz/migwin/.  
Cmake is installed into C:\Program Files\CMake normally. And E.g, the eclipse environment is C:\Eclipse\pleiades\eclipse\mingw.  
In this case, Copy all folders in C:\Program Files\CMake to C:\Eclipse\pleiades\eclipse\mingw.

### Build step
1. Prepare build directory for cmake build
2. type command in the prepared directory  
        $ mkdir build  
        $ cd build  
        $ cmake -G "your target generater" <path of alpha_concurrent>  
        $ cmake --build .  
You could refer make_win_eclipse.sh or make_linux.sh as the sample for above commands

Current linking library of test decided by cmake.
Therefore please configure cmake global option "BUILD_SHARED_LIBS" according your purpose like below;
        $ cmake -D BUILD_SHARED_LIBS=ON .....(other command line options)

If you would like to do parallel build, please use -j N option or environment variable CMAKE_BUILD_PARALLEL_LEVEL for CMake.
Especially, in case that you will use Eclipse with CMake project, please select the approach "environment variable CMAKE_BUILD_PARALLEL_LEVEL for CMake".

### Build test code and execute test
After Build step, please execute below commands  
        $ cmake --build . --target build-test  
        $ cmake --build . --target test  

# Configuration MACRO
Please refer libalconcurrent/CMakeLists.txt

### ALCONCURRENT_CONF_NOT_USE_LOCK_FREE_MEM_ALLOC
If comiple with ALCONCURRENT_CONF_NOT_USE_LOCK_FREE_MEM_ALLOC, lock free algorithms uses malloc/free instead of general_mem_allocator.

### ALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_INFO, ALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DEBUG, ALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_TEST, ALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DUMP
Configuration for log output type.
Error log is alway enable to output.

### ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
If define this macro, it enables to measure the additional statistics that is the internal information to debug lock-free algorithm.

### ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
If you would like to pass through a memory allocation request to malloc() always, please define this macro.

This macro loses the lock-free nature of the memory allocation process and is prone to memory fragmentation. Instead, the compiler sanitizer has the benefit of working effectively.

### ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
If you would like to record the backtrace of allcation and free for debugging, please define this macro.
If you define this macro, the compilation also needs -g(debug symbol)

### ALCONCURRENT_CONF_ENABLE_NON_REUSE_MEMORY_SLOT
Even if an allocated memory via general_mem_allocator is free, it just marks as discared and not re-use it.
This will makes memory leak. On the other hand, it is effective to analyze double free bug.
If you use this option, it is recommened to enable ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE also.

# Patent
## Hazard pointer algorithm
This software uses Hazard pointer algorithm to solve ABA problem.  
In 2002, Maged Michael of IBM filed an application for a U.S. patent on the hazard pointer technique, but the application was abandoned in 2010.  
Therefore, there is no patent issue.  
US Patent of Hazard pointer algrithm: US20040107227A1
https://patents.google.com/patent/US20040107227


# License
Please see "LICENSE.txt"
