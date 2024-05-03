
# Common compile options

include(common.cmake)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_USE_THREAD_LOCAL")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DUMP")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE_CHECK_DOUBLE_FREE")   # To use this option, it is better to define  -rdynamic. This option is also enable double free check
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_SLOT_CHECK_MARKER")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_CHECK_OVERRUN_WRITING")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_CHECK_LOGIC_ERROR")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DALCONCURRENT_CONF_ENABLE_THROW_LOGIC_ERROR_TERMINATION")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${CMAKE_C_FLAGS} -g -O0 --coverage")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${CMAKE_C_FLAGS} -g -O0 --coverage")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_CXX_FLAGS} -g -O0 --coverage")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${CMAKE_CXX_FLAGS} -g -O0 --coverage")

