
# If you would like to build with target or environment specific configuration:
# 1. please prepare XXXX.cmake that includes build options
# 2. provide file information of XXXX.cmake like below to CMakeLists.txt
#    with -D option like below
#        $ cmake -D BUILD_TARGET=XXXX
#    or
#        $ make BUILDTARGET=XXXX
# 
#    common.cmake is default configurations
#    codecoverage.cmake is the configuration for code coverage of gcov
# 
BUILDTARGET?=common

# Debug or Release or ...
# BUILDTYPE=Debug
# BUILDTYPE=Release
# 
BUILDTYPE?=Release

# Select build library type
# ALCONCURRENT_BUILD_SHARED_LIBS=OFF -> static library
# ALCONCURRENT_BUILD_SHARED_LIBS=ON -> shared library
ALCONCURRENT_BUILD_SHARED_LIBS?=OFF

# Sanitizer test option:
# SANITIZER_TYPE= 1 ~ 20 or ""
#
# Please see common.cmake for detail
# 
SANITIZER_TYPE?=

##### internal variable
BUILDIMPLTARGET?=all
BUILD_DIR?=build

CPUS=$(shell grep cpu.cores /proc/cpuinfo | sort -u | sed 's/[^0-9]//g')
JOBS=$(shell expr ${CPUS} + ${CPUS} / 2)

all: build
	set -e; \
	cd ${BUILD_DIR}; \
	cmake -DCMAKE_BUILD_TYPE=${BUILDTYPE} -DBUILD_TARGET=${BUILDTARGET} -DSANITIZER_TYPE=${SANITIZER_TYPE} -DALCONCURRENT_BUILD_SHARED_LIBS=${ALCONCURRENT_BUILD_SHARED_LIBS} -G "Unix Makefiles" ../; \
	cmake --build . -j ${JOBS} -v --target ${BUILDIMPLTARGET}

build:
	mkdir -p ${BUILD_DIR}

test: build-test
	set -e; \
	cd ${BUILD_DIR}; \
	ctest -j ${JOBS} -v

build-test:
	make BUILDIMPLTARGET=build-test all

sample: build-sample
	echo finish make sample
	build/sample/perf_stack/perf_stack
	# build/sample/perf_fifo/perf_fifo

build-sample:
	make BUILDIMPLTARGET=build-sample all

clean:
	-rm -fr ${BUILD_DIR} build.*

coverage: clean
	set -e; \
	make BUILDTARGET=codecoverage BUILDTYPE=Debug test;  \
	cd ${BUILD_DIR}; \
	find . -type f -name "*.gcda" | xargs -P${JOBS} -I@ gcov -l -b @; \
	lcov --rc lcov_branch_coverage=1 -c -d . -o tmp.info; \
	lcov --rc lcov_branch_coverage=1 -b -c -d . -r tmp.info  '/usr/include/*' -o tmp2.info; \
	lcov --rc lcov_branch_coverage=1 -b -c -d . -r tmp2.info  '*/test/*' -o output.info; \
	genhtml --branch-coverage -o OUTPUT -p . -f output.info

profile: clean
	set -e; \
	make BUILDTARGET=gprof BUILDTYPE=Release test;  \
	cd ${BUILD_DIR}; \
	find . -type f -executable -name "test_*" | xargs -P${JOBS} -I@ sh ../gprof_exec.sh @

sanitizer:
	set -e; \
	for i in `seq 1 21`; do \
		make sanitizer.$$i.sanitizer; \
		echo $$i / 21 done; \
	done

sanitizer.%.sanitizer: clean
	make BUILDTARGET=common BUILDTYPE=Debug SANITIZER_TYPE=$* test

SANP=$(shell expr ${CPUS} / 4)
sanitizer.p:
	set -e; \
	seq 1 21| \
	sed -E 's/([0-9]+)/make BUILD_DIR=build.\1 BUILDTARGET=common BUILDTYPE=Debug SANITIZER_TYPE=\1 test/' | \
	xargs -P${SANP} -i -d'\n' -n1 bash -c {}


.PHONY: test build sanitizer


