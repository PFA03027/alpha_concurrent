make BUILDTYPE=Release build-test
time ./build/test/test_mem_alloc/test_mem_alloc --gtest_filter='*lfmemAllocFreeBwMultThread*'
