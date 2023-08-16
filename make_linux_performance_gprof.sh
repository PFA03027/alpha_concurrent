make BUILDTARGET=gprof BUILDTYPE=Release build-test
#./build/test/test_mem_alloc/test_mem_alloc --gtest_filter='*lfmemAllocFreeBwMultThread.TC2*'
./build/test/test_mem_alloc/test_mem_alloc --gtest_filter='many_tls/lfmemAllocFreeBwMultThread.TC2/0'
gprof ./build/test/test_mem_alloc/test_mem_alloc gmon.out > test_mem_alloc.prof.out.txt