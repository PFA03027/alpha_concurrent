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

for i in {1..14}
do
	rm -fr build
	mkdir -p build
	cd build
	echo cmake -DCMAKE_BUILD_TYPE=${BUILDTYPE} -DBUILD_TARGET=${BUILDTARGET} -DSANITIZER_TYPE=${i} -G "Unix Makefiles" ../
	cmake -DCMAKE_BUILD_TYPE=${BUILDTYPE} -DBUILD_TARGET=${BUILDTARGET} -DSANITIZER_TYPE=${i} -G "Unix Makefiles" ../
	cmake --build . -j 8 -v --target build-test
	echo $i / 14.
	cmake --build . -j 8 -v --target test
	result=$?
	if [ "$result" = "0" ]; then
		echo $i is OK. result=$result
	else
		echo $i is FAIL. result=$result
		exit $result
	fi
	cd ..
done
