
# Common compile options
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

#set(CMAKE_CXX_STANDARD 11)	# for test purpose
#set(CMAKE_CXX_STANDARD 14)	# for test purpose
#set(CMAKE_CXX_STANDARD 17)	# for test purpose

if("${SANITIZER_TYPE}" EQUAL "1")
 set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fsanitize=address -DALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER")	# for test purpose
 set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -fsanitize=address")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "2")
 set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fstack-protector-strong")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "3")
 set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fsanitize=leak")	# for test purpose. Should NOT set ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
elseif("${SANITIZER_TYPE}" EQUAL "4")
 set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=shift -fsanitize=shift-exponent")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "5")
 set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=shift -fsanitize=shift-base")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "6")
 set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=null")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "7")
 set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=signed-integer-overflow")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "8")
 set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=alignment")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "9")
 set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=bool")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "10")
 set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=enum")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "11")
 set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=bounds")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "12")
 set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fsanitize-address-use-after-scope -DALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "13")
 set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -DENABLE_THREADSANITIZER -O2 -fno-omit-frame-pointer -fsanitize=thread -DALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER")	# for test purpose. thread sanitizer needs -O1/-O2. Unfortunately this finds false positive.
else()
 # set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -DALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER")	# for test purpose. thread sanitizer needs -O1/-O2. Unfortunately this finds false positive.
 # no sanitizer
endif()

#set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")	# for test purpose

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${CMAKE_C_FLAGS} -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${CMAKE_C_FLAGS} -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_CXX_FLAGS} -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${CMAKE_CXX_FLAGS} -O2")

