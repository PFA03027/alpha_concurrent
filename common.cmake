
# Common compile options
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

#set(CMAKE_CXX_STANDARD 11)	# for test purpose
#set(CMAKE_CXX_STANDARD 14)	# for test purpose
#set(CMAKE_CXX_STANDARD 17)	# for test purpose
# set(CMAKE_CXX_STANDARD 20)	# for test purpose

if("${SANITIZER_TYPE}" EQUAL "1")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTEST_ENABLE_ADDRESSSANITIZER -fno-omit-frame-pointer -fsanitize=address -DALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER")	# for test purpose
 set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -fsanitize=address")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "2")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTEST_ENABLE_ADDRESSSANITIZER -fno-omit-frame-pointer -fsanitize=address")	# for test purpose
 set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -fsanitize=address")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "3")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fstack-protector-strong")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "4")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=leak")	# for test purpose. Should NOT set ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
elseif("${SANITIZER_TYPE}" EQUAL "5")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=leak -DALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER")	# for test purpose. Should NOT set ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
elseif("${SANITIZER_TYPE}" EQUAL "6")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=leak -DALCONCURRENT_CONF_NOT_USE_LOCK_FREE_MEM_ALLOC")	# for test purpose. Should NOT set ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
elseif("${SANITIZER_TYPE}" EQUAL "7")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTEST_ENABLE_THREADSANITIZER -O2 -fno-omit-frame-pointer -fsanitize=thread")	# for test purpose. thread sanitizer needs -O1/-O2. Unfortunately this finds false positive.
elseif("${SANITIZER_TYPE}" EQUAL "8")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTEST_ENABLE_THREADSANITIZER -O2 -fno-omit-frame-pointer -fsanitize=thread -DALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER")	# for test purpose. thread sanitizer needs -O1/-O2. Unfortunately this finds false positive.
elseif("${SANITIZER_TYPE}" EQUAL "9")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTEST_ENABLE_THREADSANITIZER -O2 -fno-omit-frame-pointer -fsanitize=thread -DALCONCURRENT_CONF_NOT_USE_LOCK_FREE_MEM_ALLOC")	# for test purpose. thread sanitizer needs -O1/-O2. Unfortunately this finds false positive.
elseif("${SANITIZER_TYPE}" EQUAL "10")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize-address-use-after-scope")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "11")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize-address-use-after-scope -DALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "12")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize-address-use-after-scope -DALCONCURRENT_CONF_NOT_USE_LOCK_FREE_MEM_ALLOC")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "13")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize=null")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "14")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize=signed-integer-overflow")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "15")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize=alignment")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "16")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize=bool")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "17")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize=enum")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "18")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize=bounds")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "19")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize=shift -fsanitize=shift-exponent")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "20")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize=shift -fsanitize=shift-base")	# for test purpose
else()
 # no sanitizer option
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_USE_THREAD_LOCAL")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DUMP")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_INFO")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DEBUG")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_TEST")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_PREFER_TO_SHARE_CHUNK")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_NOT_USE_LOCK_FREE_MEM_ALLOC")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_NON_REUSE_MEMORY_SLOT")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE")   # To use this option, it is better to define  -rdynamic. This option is also enable double free check
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_CHECK_OVERRUN_WRITING")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_EXCEPTION")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_DETECT_UNEXPECTED_DEALLOC_CALLING")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_LF_ALGO_USE_LOCAL_ALLOCATER")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_INDIVIDUAL_KEY_EXCLUSIVE_ACCESS")  # Because this lead perfomance degrade, this option is experimental

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")	# for test purpose
# set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -pg")	# for test purpose


set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${CMAKE_C_FLAGS} -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${CMAKE_C_FLAGS} -O2")
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_CXX_FLAGS} -g")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_CXX_FLAGS} -g -rdynamic")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${CMAKE_CXX_FLAGS} -O2")

