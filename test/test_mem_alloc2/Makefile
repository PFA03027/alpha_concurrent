
include ../../common.mk

CXXFLAGS +=	-c -MMD -MP $(CXX_STD) -O2 -g -Wall -I../../libalconcurrent/inc -I../../libalconcurrent/src -I../googletest/googletest/include

SRCS := $(wildcard src/*.cpp)
OBJS := $(SRCS:%.cpp=%.o)
DEPS := $(SRCS:%.cpp=%.d)
LIB_FILES :=  ../../build/libalconcurrent/libalconcurrent.a ../../build/lib/libgtest.a ../../build/lib/libgtest_main.a

LIBS += -L../../build/libalconcurrent -L../../build/lib
LIBS += -lalconcurrent -lpthread -lgtest -lgtest_main

TARGET =	test_mem_alloc2.exe

-include $(DEPS)

$(TARGET):	$(OBJS) $(LIB_FILES)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(DEPS)

