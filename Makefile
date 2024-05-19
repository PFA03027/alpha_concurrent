
# If you would like to build with target or environment specific configuration:
# 1. please prepare XXXX.cmake that includes build options
# 2. provide file information of XXXX.cmake like below to CMakeLists.txt
#    with -D option like below
#        $ cmake -D BUILD_TARGET=XXXX
#    or
#        $ make BUILDTARGET=XXXX
# 
#    common.cmake is common configurations
#    normal.cmake is the configuration for normal build
#    gprof.cmake is the configuration for profile and analysis
#    codecoverage.cmake is the configuration for code coverage of gcov
# 
BUILDTARGET?=normal

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
# Please see normal.cmake for detail
# 
SANITIZER_TYPE?=

# GoogleTest Option
TEST_OPTS?=

##### internal variable
BUILDIMPLTARGET?=all
BUILD_DIR?=build
#MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))	# 相対パス名を得るならこちら。
MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))	# 絶対パス名を得るならこちら。

SANITIZER_ALL_IDS=$(shell seq 1 21)
SANITIZER_P_ALL_TARGETS=$(addprefix sanitizer.p.,$(SANITIZER_ALL_IDS))

#TEST_EXECS = $(shell find ${BUILD_DIR} -type f -executable -name "test_*")
TEST_EXECS = $(shell find . -type f -executable -name "test_*")

CMAKE_CONFIGURE_OPTS  = -DCMAKE_EXPORT_COMPILE_COMMANDS=ON  # for clang-tidy
CMAKE_CONFIGURE_OPTS += -DCMAKE_BUILD_TYPE=${BUILDTYPE}
CMAKE_CONFIGURE_OPTS += -DBUILD_TARGET=${BUILDTARGET}
CMAKE_CONFIGURE_OPTS += -DSANITIZER_TYPE=${SANITIZER_TYPE}
CMAKE_CONFIGURE_OPTS += -DALCONCURRENT_BUILD_SHARED_LIBS=${ALCONCURRENT_BUILD_SHARED_LIBS} 

CPUS=$(shell grep cpu.cores /proc/cpuinfo | sort -u | sed 's/[^0-9]//g')
JOBS?=$(shell expr ${CPUS} + ${CPUS} / 2)

all: configure-cmake
	set -e; \
	cd ${BUILD_DIR}; \
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

profile: build-profile
	make -j ${JOBS} make-profile-out

sanitizer:
	set -e; \
	for i in $(SANITIZER_ALL_IDS); do \
		make sanitizer.$$i.sanitizer; \
		echo $$i / 21 done; \
	done

sanitizer.p:
	make -j${JOBS} sanitizer.p_internal

sanitizer.%.sanitizer: clean
	make BUILDTARGET=normal BUILDTYPE=Debug SANITIZER_TYPE=$* test

tidy-fix: configure-cmake
	find ./ -name '*.cpp'|grep -v googletest|grep -v ./build/|xargs -t -P${JOBS} -n1 clang-tidy -p=build --fix
	find ./ -name '*.cpp'|grep -v googletest|grep -v ./build/|xargs -t -P${JOBS} -n1 clang-format -i
	find ./ -name '*.hpp'|grep -v googletest|grep -v ./build/|xargs -t -P${JOBS} -n1 clang-format -i

tidy: configure-cmake
	find ./ -name '*.cpp'|grep -v googletest|grep -v ./build/|xargs -t -P${JOBS} -n1 clang-tidy -p=build

############################################################################
configure-cmake:
	set -e; \
	mkdir -p ${BUILD_DIR}; \
	cd ${BUILD_DIR}; \
	cmake ${CMAKE_CONFIGURE_OPTS} -G "Unix Makefiles" ${MAKEFILE_DIR}

build-profile:
	make BUILDTARGET=gprof BUILDTYPE=${BUILDTYPE} build-test

exec-profile: build-profile
	set -e; \
	cd ${BUILD_DIR}; \
	ctest -j ${JOBS} -v


sanitizer.p_internal: $(SANITIZER_P_ALL_TARGETS)

define SANITIZER_P_TEMPLATE
sanitizer.p.configure-cmake.$(1):
	make BUILD_DIR=build.p/$(1) BUILDTARGET=normal BUILDTYPE=Debug SANITIZER_TYPE=$(1) configure-cmake

sanitizer.p.$(1): sanitizer.p.configure-cmake.$(1)
	make BUILD_DIR=build.p/$(1) BUILDTARGET=normal BUILDTYPE=Debug SANITIZER_TYPE=$(1) sanitizer.p.test

sanitizer.p.configure-cmake.$(shell expr $(1) + 1): sanitizer.p.configure-cmake.$(1)

endef

$(foreach pgm,$(SANITIZER_ALL_IDS),$(eval $(call SANITIZER_P_TEMPLATE,$(pgm))))

ifeq ($(strip $(TEST_EXECS)),)
## $(TEST_EXECS)が空の場合＝buildが実行されていない状況
test.%: build-test
	make test.$*

profile.%: build-profile
	make profile.$*

else
## $(TEST_EXECS)になんらかの値が入っている場合=buildが実行されている状況
# 第1引数: テスト実行ファイルのファイル名
# 第2引数: テスト実行ファイルのあるディレクトリ名
# 第3引数: テスト実行ファイルのフルパス名
define TEST_EXEC_TEMPLATE
test.$(1): build-test
	$(3) $(TEST_OPTS)

profile.$(1): build-profile
	-rm -f $(2)gmon.out
	set -e; (cd $(2); ./$(1) $(TEST_OPTS))
	gprof $(3) $(2)gmon.out > $(2)prof.out.txt
	gprof -A $(3) $(2)gmon.out > $(2)prof_func.out.txt

endef

$(foreach pgm,$(TEST_EXECS),$(eval $(call TEST_EXEC_TEMPLATE,$(notdir $(pgm)),$(dir $(pgm)),$(pgm))))

endif

make-profile-out: $(addprefix profile.,$(notdir $(TEST_EXECS)))

sanitizer.p.test:
	set -e; \
	cd ${BUILD_DIR}; \
	cmake --build . --target build-test; \
	ctest -j $(shell expr ${JOBS} / 2) -v

.PHONY: test sanitizer sanitizer.p configure-cmake
