echo $@

# If you would like to build with target or environment specific configuration:
# 1. please prepare XXXX.cmake that includes build options
# 2. provide file information of XXXX.cmake like below to CMakeLists.txt
#    with -D option like below
#        $ cmake -D BUILD_TARGET=`pwd`/../XXXX
#    common.cmake is default configurations
# 
BUILDTARGET=codecoverage

# Debug or Release or ...
BUILDTYPE=Debug
# BUILDTYPE=Release

JOBS=$[$(grep cpu.cores /proc/cpuinfo | sort -u | sed 's/[^0-9]//g') + 1]
echo JOBS: ${JOBS}

rm -fr build
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TARGET=codecoverage -G "Unix Makefiles" ../
cmake --build . -j ${JOBS} -v --target build-test
cmake --build . -j ${JOBS} --target test
find . -type f -name "*.gcda" | xargs -I@ gcov -l -b @
lcov --rc lcov_branch_coverage=1 -c -d . -o tmp.info
lcov --rc lcov_branch_coverage=1 -b -c -d . -r tmp.info  '/usr/include/*' -o tmp2.info
lcov --rc lcov_branch_coverage=1 -b -c -d . -r tmp2.info  '*/test/*' -o output.info
genhtml --branch-coverage -o OUTPUT -p . -f output.info


