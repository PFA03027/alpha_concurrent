#!/bin/bash

echo $@

# If you would like to build with target or environment specific configuration:
# 1. please prepare XXXX.cmake that includes build options
# 2. provide file information of XXXX.cmake like below to CMakeLists.txt
#    with -D option like below
#        $ cmake -D BUILD_TARGET=`pwd`/../XXXX
#    common.cmake is default configurations
# 
BUILDTARGET=common

# Debug or Release or ...
BUILDTYPE=Debug
#BUILDTYPE=Release

# 1st: BUILDTYPE
# 2nd: BUILDTARGET
# 3rd: SANITIZER type number. please see common.cmake
function build_sanitizer () {
	mkdir -p build
	cd build
	echo cmake -DCMAKE_BUILD_TYPE=$1 -DBUILD_TARGET=$2 -DSANITIZER_TYPE=$3 -G "Unix Makefiles" ../
	cmake -DCMAKE_BUILD_TYPE=$1 -DBUILD_TARGET=$2 -DSANITIZER_TYPE=$3 -G "Unix Makefiles" ../
	cmake --build . -j ${JOBS} -v --target build-test
	echo $3 / 15.
	cd ..
}

JOBS=$[$(grep cpu.cores /proc/cpuinfo | sort -u | sed 's/[^0-9]//g') + 1]

if [ "$#" = "0" ]; then
	exit $result
else
	build_sanitizer ${BUILDTYPE} ${BUILDTARGET} $1
fi
