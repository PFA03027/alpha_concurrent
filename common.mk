

#CXX_STD = -std=c++11	# for test purpose
#CXX_STD = -std=c++14	# for test purpose
#CXX_STD = -std=c++17	# for test purpose

#CXXFLAGS += -DALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_INFO
#CXXFLAGS += -DALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DEBUG
#CXXFLAGS += -DALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_TEST
CXXFLAGS += -DALCONCURRENT_CONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DUMP
#CXXFLAGS += -DALCONCURRENT_CONF_NOT_USE_LOCK_FREE_MEM_ALLOC
#CXXFLAGS += -DALCONCURRENT_CONF_ENABLE_DETAIL_STATISTICS_MESUREMENT
#CXXFLAGS += -DALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
#CXXFLAGS += -DALCONCURRENT_CONF_ENABLE_RECORD_BACKTRACE
