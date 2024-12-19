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

# stack_list class in lf_stack.hpp
Semi-lock free Stack type queue

Template 1st parameter T should be copy assignable.

In case of no avialable free node that carries a value, new node is allocated from heap internally.
In this case, this queue may be locked. And push() may trigger this behavior.

On the other hand, used free node will be recycled without a memory allocation. In this case, push() is lock free.

To reduce lock behavior, pre-allocated nodes are effective.
get_allocated_num() provides the number of the allocated nodes. This value is hint to configuration.

# lockfree_list class in lf_list.hpp
Semi-lock free list

Template 1st parameter T should be copy assignable.

In case of no avialable free node that carries a value, new node is allocated from heap internally.
In this case, this queue may be locked. And push_front()/push_back()/insert() may trigger this behavior.

On the other hand, used free node will be recycled without a memory allocation. In this case, push_front()/push_back()/insert() is lock free.

To reduce lock behavior, pre-allocated nodes are effective.
get_allocated_num() provides the number of the allocated nodes. This value is hint to configuration.

# one_side_deque class in lf_one_side_deque.hpp
Semi-lock free one side deque

Template 1st parameter T should be copy assignable.

In case of no avialable free node that carries a value, new node is allocated from heap internally.
In this case, this queue may be locked. And push_front()/push_back() may trigger this behavior.

On the other hand, used free node will be recycled without a memory allocation. In this case, push_front()/push_back() is lock free.

To reduce lock behavior, pre-allocated nodes are effective.
get_allocated_num() provides the number of the allocated nodes. This value is hint to configuration.

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
After allocation, this class may be lock free.
If disable ALCONCURRENT_CONF_USE_THREAD_LOCAL, this depends whether pthread_getspecific() is lock free or not.

To avoid race condtion b/w dynamic_tls class and thread local storage destructor, currently global mutex lock is introduced.

# general memory allocator class that is semi lock-free in lf_mem_alloc.hpp
This is general memory allocator to get lock-free behavior and to avoid memory fragmentation.

Configured of memory is kept to re-use.
If the required size is over the max size of configuration paramter, it is allocated directly by mmap() and free it by munmap() also.

general_mem_allocator::prune() and gmem_prune() is introduce to release the allocated memory if that is release by general_mem_allocator::deallocate() or gmem_deallocate().

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

### ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER
If compile with ALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER, memory slot offset is checked by simple algorithm.
This makes better behavior for memory corruption.
So, now this option is defined as default configuration.

### ALCONCURRENT_CONF_PREFER_TO_SHARE_CHUNK
If compile with ALCONCURRENT_CONF_PREFER_TO_SHARE_CHUNK, it will behave like sharing the memory slot as much as possible.  
If this option is not set, prefer to use per-thread memory slots. Since each thread has an independent memory slot, it is less likely that memory allocation and release contention will occur. On the other hand, since each thread has an empty memory slot, the efficiency of memory usage decreases.

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

#### ALCONCURRENT_CONF_ENABLE_CHECK_OVERRUN_WRITING
If compile with ALCONCURRENT_CONF_ENABLE_CHECK_OVERRUN_WRITING, write over run is checked.

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
-- コールスタック管理クラスのベース部品クラス化
-- ALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION と ALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERRORのどちらかでロジックエラー検査が有効になるようにする。
-- -Wsign-conversion対応の追加
** コンストラクタのconstexpr対応
** atomic<>変数系の、std::hardware_destructive_interference_size 対応

* alloc_only_chamberの改善
-- サイズ0要求の場合に最低サイズ1で確保する対応。アドレスが必ず異なるようにする要件を保証するため。
-- deallocateのI/F追加。基本的には、削除済みマーキングをするだけ。2重フリー、リークチェックできるようにするため
-- allocate時とdeallcate時のコールスタックを記録する機能の追加。
-- 2重フリーの場合のエラー及びコールスタック情報の出力。
-- noexceptを積極的に付与する
-- デバッグ機能追加
--- 使用中、解放済みをチェックする機能の追加。
--- 内容のdump機能
--- 特定のアドレスに特化した内容のdump機能
--- room_boader内メンバ変数のconst化(is_freed_を除く)
--- room_boaderのI/Fのconst化、constexpr化
--- メイン関数の最後に呼び出す等の方法による、解放漏れによる簡易リークチェック機能の追加。

* dynamic_tlsクラスの改善
** メモリ効率改善など、計算で求められるアドレス情報の省略など

* ハザードポインタ管理クラスの改善
-- RAIIによるハザードポインタ登録管理用クラスの作成
-- atomic<T*>用のRAIIによるハザードポインタ登録管理用クラスの作成 -> 単純なatomic<T*>ではなく、atomic<T*>を内包したhazard_ptrクラスを作る。
-- このクラスでretireは扱わない。ガベージコレクタクラスで扱うべき。
-- ハザードポインタ管理クラスは、alloc_only_chamberにのみ依存するようにする（べき？）
-- 強制リソース解放＆初期化のI/F追加。alloc_only_chamberでのリークチェック用機能を使用してデバッグするため。
-- ハザードポインタのスロットについて、nullptrが指定された場合は、専用スロットを使用する実装で、nullptrが来ても動作可能とする。
   -> nullptrは、確保失敗扱いで実装完了。

* lf_mem_allocクラスの改善
** サイズ0要求の場合に最低サイズ1で確保する対応。アドレスが必ず異なるようにする要件を保証するため。
** allocate/deallocateに特化させる。

* retire管理モジュール
-- hazard pointerモジュールから独立させ、lf_mem_allocよりも上位のレイヤに挿入する
-- 遅延解放処理となるretire系を独立化させる。デストラクタ処理やメモリ開放の遅延実行に対応するためには、別スレッドを建てるなどの対応が必要となるため。


* 各種ロックフリーアルゴリズムの改善
-- フリーノード管理クラスをどうするか？ retire系に一任する？
-- 如何に、mallocを呼ばないか？ retireによって、フリーノードストレージに戻す？
-- ノードは再利用する。
-- フリーノードストレージは、ダイナミックスレッドローカルストレージを使用しない実装にする。
-- フリーノードのリサイクル先の制御に応援要求フラグで制御する。
--- 一番早いスレッドローカルストレージ。ただし、共有できない。
　　- 例えば、プロデューサー・コンシューマーモデルでは、コンシューマー側にノードがたまり続けて活用されない状況を生み出してしまう欠点を持つ。
--- 排他制御付きグローバルストレージ。軽いが、ロックを必要とする。
    - 競合が起きなけれ、ロックを必要とする点は、欠点として表れにくい。
--- ロックフリーグローバルストレージ。重いがロックフリーという性質を持てる。フリーノードストレージの最後の砦。
