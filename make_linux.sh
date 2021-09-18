echo $@

# If you would like to build with target or environment specific configuration:
# 1. please prepare XXXX.cmake that includes build options
# 2. provide file information of XXXX.cmake like below to CMakeLists.txt
#    with -D option like below
#        $ cmake -D BUILD_TARGET=`pwd`/../XXXX
#    common.cmake is default configurations
# 
BUILDTARGET=common

if [ $# -eq 0 ]; then
	mkdir -p build
	cd build
	cmake -D BUILD_TARGET=${BUILDTARGET} -G "Unix Makefiles" ../
	cmake --build . -j 8 -v
else
	if [ "$1" = "clean" ]; then
		rm -fr build
	else
		mkdir -p build
		cd build
		cmake -D BUILD_TARGET=${BUILDTARGET} -G "Unix Makefiles" ../
		if [ "$1" = "full" ]; then
			cmake --build . --clean-first -j 8 -v
		else
			cmake --build . -j 8 -v --target $1
		fi
	fi
fi

