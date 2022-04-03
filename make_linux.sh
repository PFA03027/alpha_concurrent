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
#BUILDTYPE=Debug
BUILDTYPE=Release

if [ $# -eq 0 ]; then
	mkdir -p build
	cd build
	cmake -DCMAKE_BUILD_TYPE=${BUILDTYPE} -D BUILD_TARGET=${BUILDTARGET} -G "Unix Makefiles" ../
	cmake --build . -j 8 -v
else
	if [ "$1" = "clean" ]; then
		rm -fr build
	elif [ "$1" = "test" ]; then
		mkdir -p build
		cd build
		cmake -DCMAKE_BUILD_TYPE=${BUILDTYPE} -DBUILD_TARGET=${BUILDTARGET} -G "Unix Makefiles" ../
		cmake --build . -j 8 -v --target build-test
		cmake --build . -j 8 -v --target test
	elif [ "$1" = "full" ]; then
		rm -fr build
		mkdir -p build
		cd build
		cmake -DCMAKE_BUILD_TYPE=${BUILDTYPE} -D BUILD_TARGET=${BUILDTARGET} -G "Unix Makefiles" ../
		cmake --build . --clean-first -j 8 -v build-test
	else
		mkdir -p build
		cd build
		cmake -DCMAKE_BUILD_TYPE=${BUILDTYPE} -D BUILD_TARGET=${BUILDTARGET} -G "Unix Makefiles" ../
		cmake --build . -j 8 -v --target $1
	fi
fi

