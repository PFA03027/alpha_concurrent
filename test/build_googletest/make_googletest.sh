
mkdir -p build
cd build
cmake -G "Eclipse CDT4 - Unix Makefiles" ../../googletest
make $@
