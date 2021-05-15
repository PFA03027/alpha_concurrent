
include ../common.mk

CXXFLAGS =	-c -MMD -MP $(CXX_STD) -O2 -g -Wall -I../libalconcurrent/inc -I../libalconcurrent/src

SRCS := $(wildcard src/*.cpp)
OBJS := $(SRCS:%.cpp=%.o)
DEPS := $(SRCS:%.cpp=%.d)
LIB_FILES :=  ../libalconcurrent/libalconcurrent.a

LIBS = -L../libalconcurrent -lalconcurrent -lpthread

TARGET =	test_mem_alloc.exe

-include $(DEPS)

$(TARGET):	$(OBJS) $(LIB_FILES)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(DEPS)

