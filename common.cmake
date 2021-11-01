
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DUMP")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DUMP -DCONF_LOGGER_INTERNAL_ENABLE_OUTPUT_DEBUG")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNOT_USE_LOCK_FREE_MEM_ALLOC")

# Common compile options
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

#set(CMAKE_CXX_STANDARD 11)	# for test purpose
#set(CMAKE_CXX_STANDARD 14)	# for test purpose
#set(CMAKE_CXX_STANDARD 17)	# for test purpose

# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=shift -fsanitize=shift-exponent")	# for test purpose
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=shift -fsanitize=shift-base")	# for test purpose
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=null")	# for test purpose
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=signed-integer-overflow")	# for test purpose
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=alignment")	# for test purpose
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=bool")	# for test purpose
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=enum")	# for test purpose
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=bounds")	# for test purpose
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fsanitize-address-use-after-scope")	# for test purpose
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_THREADSANITIZER -O2 -fno-omit-frame-pointer -fsanitize=thread")	# for test purpose. thread sanitizer needs -O1/-O2. Unfortunately this finds false positive.
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fsanitize=leak")	# for test purpose
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fsanitize=address")	# for test purpose
# set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -fsanitize=address")	# for test purpose
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fstack-protector-strong")	# for test purpose

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")	# for test purpose

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${CMAKE_C_FLAGS} -g -O2")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${CMAKE_C_FLAGS} -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_CXX_FLAGS} -g -O2")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${CMAKE_CXX_FLAGS} -O2")

