
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
#MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))	# 相対パス名を得るならこちら。
MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))	# 絶対パス名を得るならこちら。

CPUS=$(shell grep cpu.cores /proc/cpuinfo | sort -u | sed 's/[^0-9]//g')
JOBS=$(shell expr ${CPUS} + ${CPUS} / 2)

all: configure-cmake
	set -e; \
	cd ${BUILD_DIR}; \
	cmake --build . -j ${JOBS} -v --target ${BUILDIMPLTARGET}

test: build-test
	set -e; \
	cd ${BUILD_DIR}; \
	ctest -j ${JOBS} -v

build-test:
	make BUILDIMPLTARGET=build-test all

configure-cmake:
	set -e; \
	mkdir -p ${BUILD_DIR}; \
	cd ${BUILD_DIR}; \
	cmake -DCMAKE_BUILD_TYPE=${BUILDTYPE} -DBUILD_TARGET=${BUILDTARGET} -DSANITIZER_TYPE=${SANITIZER_TYPE} -DALCONCURRENT_BUILD_SHARED_LIBS=${ALCONCURRENT_BUILD_SHARED_LIBS} -G "Unix Makefiles" ${MAKEFILE_DIR}

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

SANITIZER_ALL_IDS=$(shell seq 1 21)

sanitizer:
	set -e; \
	for i in $(SANITIZER_ALL_IDS); do \
		make sanitizer.$$i.sanitizer; \
		echo $$i / 21 done; \
	done

sanitizer.%.sanitizer: clean
	make BUILDTARGET=common BUILDTYPE=Debug SANITIZER_TYPE=$* test

SANITIZER_P_ALL_TARGETS=$(addprefix sanitizer.p.,$(SANITIZER_ALL_IDS))
SANITIZER_P_CONF_ALL_TARGETS=$(addprefix sanitizer.p.configure-cmake.,$(SANITIZER_ALL_IDS))

sanitizer.p: sanitizer.p.configure-cmake
	make -j${JOBS} sanitizer.p_internal

sanitizer.p.configure-cmake: $(SANITIZER_P_CONF_ALL_TARGETS)

sanitizer.p_internal: $(SANITIZER_P_ALL_TARGETS)

define SANITIZER_P_TEMPLATE
sanitizer.p.configure-cmake.$(1):
	make BUILD_DIR=build.p/$(1) BUILDTARGET=common BUILDTYPE=Debug SANITIZER_TYPE=$(1) configure-cmake

sanitizer.p.$(1):
	make BUILD_DIR=build.p/$(1) BUILDTARGET=common BUILDTYPE=Debug SANITIZER_TYPE=$(1) sanitizer.p.test
endef

sanitizer.p.test:
	set -e; \
	cd ${BUILD_DIR}; \
	cmake --build . -v --target build-test; \
	ctest -j ${JOBS} -v

$(foreach pgm,$(SANITIZER_ALL_IDS),$(eval $(call SANITIZER_P_TEMPLATE,$(pgm))))

.PHONY: test sanitizer sanitizer.p configure-cmake
