# For normal build and build with sanitizer

if("${SANITIZER_TYPE}" EQUAL "1")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTEST_ENABLE_ADDRESSSANITIZER -fno-omit-frame-pointer -fsanitize=address")	# for test purpose
 set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -fsanitize=address")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "2")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTEST_ENABLE_ADDRESSSANITIZER -fno-omit-frame-pointer -fsanitize=address -DALCONCURRENT_CONF_ENABLE_MALLOC_INSTEAD_OF_MMAP")	# for test purpose
 set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -fsanitize=address")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "3")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fstack-protector-strong")	# for test purpose
elseif("${SANITIZER_TYPE}" EQUAL "4")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=leak")	# for test purpose. Should NOT set ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
elseif("${SANITIZER_TYPE}" EQUAL "5")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=leak -DALCONCURRENT_CONF_ENABLE_MALLOC_INSTEAD_OF_MMAP")	# for test purpose. Should NOT set ALCONCURRENT_CONF_USE_MALLOC_ALLWAYS_FOR_DEBUG_WITH_SANITIZER
elseif("${SANITIZER_TYPE}" EQUAL "6")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTEST_ENABLE_THREADSANITIZER -g -O2 -fsanitize=thread")	# for test purpose. thread sanitizer needs -O1/-O2. Unfortunately this finds false positive.
elseif("${SANITIZER_TYPE}" EQUAL "7")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTEST_ENABLE_THREADSANITIZER -g -O2 -fno-omit-frame-pointer -fsanitize=thread -DALCONCURRENT_CONF_ENABLE_MALLOC_INSTEAD_OF_MMAP")	# for test purpose. thread sanitizer needs -O1/-O2. Unfortunately this finds false positive.
elseif("${SANITIZER_TYPE}" EQUAL "8")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTEST_ENABLE_UBSANITIZER -g -fno-omit-frame-pointer -fsanitize=undefined -fno-sanitize-recover=all")	# for test purpose
else()
 # no sanitizer option
endif()


set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")
# set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_CXX_FLAGS} -g")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -rdynamic")
# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -rdynamic -fno-omit-frame-pointer")
# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -rdynamic")

