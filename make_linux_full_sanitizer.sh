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
function exec_sanitizer () {
	rm -fr build
	mkdir -p build
	cd build
	echo cmake -DCMAKE_BUILD_TYPE=$1 -DBUILD_TARGET=$2 -DSANITIZER_TYPE=$3 -G "Unix Makefiles" ../
	cmake -DCMAKE_BUILD_TYPE=$1 -DBUILD_TARGET=$2 -DSANITIZER_TYPE=$3 -G "Unix Makefiles" ../
	cmake --build . -j 8 -v --target build-test
	echo $3 / 14.
	cmake --build . -j 8 -v --target test
	result=$?
	if [ "$result" = "0" ]; then
		echo $3 is OK. result=$result
	else
		echo $3 is FAIL. result=$result
		cd ..
		exit $result
	fi
	cd ..
}

if [ "$#" = "0" ]; then
	for i in {1..14}
	do
		exec_sanitizer ${BUILDTYPE} ${BUILDTARGET} ${i}
	done
else
	exec_sanitizer ${BUILDTYPE} ${BUILDTARGET} $1
fi
