# libalconcurrent is Semi lock-free concurrent software asset
The purpose of libalconcurrent library provides semi lock-free algorithms and semi lock-free memory allocator.
If you are possible to design the necessary memory size, semi lock-free algorithms are mostly same behavior of lock-free.

# Pre-requirement
* C++14 standard and standard C++ library are required.

## Supplement
C++17 or newer C++ standard is better to compile.

# Semi-lock free storage class

## fifo_list class in lf_fifo.hpp
Semi-lock free FIFO type queue

## stack_list class in lf_stack.hpp
Semi-lock free Stack type queue

## lockfree_list class in lf_list.hpp
Semi-lock free list

## Common characteristics of these classes
Template 1st parameter T is required below
* copy constructible and copy assignable.

or

* move constructible and move assignable.

In case of no available free node that carries a value, new node is allocated from heap internally.
In this case, this queue may be locked. And push_front()/push_back()/insert() may trigger this behavior.

On the other hand, used free node will be recycled without a memory allocation. In this case, push_front()/push_back()/insert() is lock free.

Below use-case has the possiblity that mmap system call leads blocking behavior;

* Construct a instance itself.
* Any initial API call by each thread.
* pop_front()/push_front()/push_back() call in case of no free internal node.

To reduce blocking behavior possibility, pre-allocated nodes are effective.
get_allocated_num() provides the number of the allocated nodes. This value is hint to configuration.
To make get_allocated_num() valid behavior , please enable compile option ALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE.


# dynamic_tls class in dynamic_tls.hpp
Support dynamic allocatable thread local storage.

When allocating dynamic allocatable thread local storage for a thread, it is not lock free. This will be happened by 1st access of a value instance.
After allocation, this class may be lock free.
If disable ALCONCURRENT_CONF_USE_THREAD_LOCAL, this depends whether pthread_getspecific() is lock free or not.

To avoid race condtion b/w dynamic_tls class and thread local storage destructor, currently global mutex lock is introduced.

# general memory allocator class that is semi lock-free in lf_mem_alloc.hpp
This is general memory allocator to get lock-free behavior and to avoid memory fragmentation.

This memory allocator is not use malloc/free. Instead of that, this memroy allocator uses mmap()/munmap().

Small size memory is not release back to system until process end to reduce blocking behavior possibility by mmap().

On the other hand, if the required size is over the pre-defined max size, it is allocated directly by mmap() and free it by munmap() also.
This means big size memory allocation is not lock-free.


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

### To install your system
Current libalconcurrent builder is not prepared installing logic.
Therefore please do below by your build system or manual operation.
1. copy the header files directory that is libalconcurrent/inc/alconcurrent into your expected header files directory
2. copy the library file "libalconcurrent/libalconcurrent.a" into your expected library files directory

If you build libalconcurrent as a shared library, please copy it.

## How to build by cmake
### Pre-condition:
Please prepare cmake on your system.  
In case of Windows system, please download cmake windows binary from https://cmake.org/download/.  
After install, please copy xxx/CMake/yyy to zzz/migwin/.  
Cmake is installed into C:\Program Files\CMake normally. And E.g, the eclipse environment is C:\Eclipse\pleiades\eclipse\mingw.  
In this case, Copy all folders in C:\Program Files\CMake to C:\Eclipse\pleiades\eclipse\mingw.

To build on Windows, you should replace mmap()/munmap() to other suitable API.

### Build by cmake for library only build
type command in the prepared directory  
        $ make all  

Current linking library of test decided by cmake.
Therefore please configure cmake option "ALCONCURRENT_BUILD_SHARED_LIBS" via make option according your purpose like below;  
        $ make ALCONCURRENT_BUILD_SHARED_LIBS=ON all  

If you would like to do debug build, please type like below;  
        $ make BUILDTYPE=Debug test  

### Build test code and execute test
After Build step, please execute below commands  
        $ make test  

# Configuration MACRO
Please refer common.cmake also

### ALCONCURRENT_CONF_USE_THREAD_LOCAL
If compile with ALCONCURRENT_CONF_USE_THREAD_LOCAL, this library uses thread_local for dynamic thread local storage class instead of pthread thread local storage.  
If you could rely on the destructor behavior of thread_local variable of C++ compiler, you could enable this option.  
And then, you could get better performance.  
(G++ version 11.3.0 works well.)
So, now this option is defined as default configuration.

### Utility option
#### ALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
If define this macro, it enables to measure the additional statistics that is the internal information to debug lock-free algorithm.

### Debug purpose options
#### ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
If you would like to pass through a memory allocation request to malloc() always, please define this macro.

This macro loses the lock-free nature of the memory allocation process and is prone to memory fragmentation. Instead, the compiler sanitizer has the benefit of working effectively.

#### ALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE
If you would like to record the backtrace of allcation and free for debugging, please define this macro.
If you define this macro, the compilation also needs -g(debug symbol) and it is better to define  -rdynamic.

### Internal use build option
#### ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR
If compile with ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR, it will detect logical error and output error log. This is only for internal debugging or porting activity.

#### ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION
If compile with ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION, call std::std::terminate() when detect logical error. This is only for internal debugging or porting activity.

#### ALCONCURRENT_CONF_ENABLE_MALLOC_INSTEAD_OF_MMAP
When doing memory sanitizer test for internal lf_mem_alloc, this option is needed. In case of defined this option, lf_mem_alloc will call malloc/free instead of mmap/munmap. This is only for internal debugging or porting activity.

#### ALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_INFO, ALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DEBUG, ALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_TEST, ALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DUMP
Configuration for log output type.
Error log is alway enable to output.

### 

# Patent
## Hazard pointer algorithm
This software uses Hazard pointer algorithm to solve ABA problem.  
In 2002, Maged Michael of IBM filed an application for a U.S. patent on the hazard pointer technique, but the application was abandoned in 2010.  
Therefore, there is no patent issue.  
US Patent of Hazard pointer algrithm: US20040107227A1
https://patents.google.com/patent/US20040107227


# License
Please see "LICENSE.txt"

# TODO
* 全般的な改善
-- mmap()によるメモリ確保/解放処理を、別スレッドで行い、mmap()処理を隠す