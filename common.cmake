
# Common compile options
set(WARNING_OPT "-Wall -Wconversion -Wsign-conversion -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNING_OPT}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_OPT}")

# set(CMAKE_CXX_STANDARD 11)	# for test purpose
# set(CMAKE_CXX_STANDARD 14)	# for test purpose
# set(CMAKE_CXX_STANDARD 17)	# for test purpose
# set(CMAKE_CXX_STANDARD 20)	# for test purpose

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_USE_THREAD_LOCAL")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_PRUNE_THREAD")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_YIELD_IN_HAZARD_POINTER_THREAD_DESTRUCTION")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_PREFER_TO_SHARE_CHUNK")

### Compiler depending option
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_COUNTERMEASURE_GCC_BUG_66944")   # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66944
### Utility option
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_HAZARD_PTR_PROFILE")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_OD_NODE_PROFILE")
### Debug purpose options
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE")   # To use this option, it is better to define  -rdynamic. This option is also enable double free check
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_CHECK_OVERRUN_WRITING")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_CHECK_PUSH_FRONT_FUNCTION_NULLPTR")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_CHECK_TAIL_NODE_NEXT_NULLPTR")
### Internal use build option
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_INFO")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DEBUG")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_TEST")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DUMP")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_MALLOC_INSTEAD_OF_MMAP") # memory sanitizer test for internal lf_mem_alloc, please enable this option
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_INDIVIDUAL_KEY_EXCLUSIVE_ACCESS")  # Because this lead perfomance degrade, this option is experimental
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_MODULO_OPERATION_BY_BITMASK") # effectiveness depends on CPU instruction set performance
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_USE_MALLOC_FREE_LF_ALGO_NODE_ALLOC") # lf_fifo/lf_list/lf_stack/lf_one_side_deque use malloc/free
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPERFORMANCE_ANALYSIS_LOG1")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_ALL_NODE_RECYCLE_BY_PRUNE_THREAD")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_PRUNE_THREAD_SLEEP_SEC=1")

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")	# for test purpose
# set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -pg")	# for test purpose
