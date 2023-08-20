make BUILDTYPE=Release build-test
time ./build/test/test_mem_alloc/test_mem_alloc --gtest_filter='*lfmemAllocFreeBwMultThread*'
time ./build/test/test_mem_alloc/test_mem_alloc --gtest_repeat=100 --gtest_filter=many_tls/lfmemAllocFreeBwMultThread.TC1/3 > /dev/null
time ./build/test/test_mem_alloc/test_mem_alloc --gtest_repeat=100 --gtest_filter=many_tls/lfmemAllocFreeBwMultThread.TC2/3 > /dev/null
time ./build/test/test_mem_alloc/test_mem_alloc --gtest_repeat=100 --gtest_filter=many_tls/lfmemAllocFreeBwMultThread.TC3/3 > /dev/null

