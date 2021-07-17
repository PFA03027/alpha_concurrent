
include ../../common.mk

CXXFLAGS +=	-c -MMD -MP $(CXX_STD) -O2 -g -Wall -I../../libalconcurrent/inc

SRCS := $(wildcard src/*.cpp)
OBJS := $(SRCS:%.cpp=%.o)
DEPS := $(SRCS:%.cpp=%.d)
LIB_FILES :=  ../../libalconcurrent/libalconcurrent.a

LIBS = -L../../libalconcurrent -lalconcurrent -lpthread

TARGET =	test_lf_one_side_deque.exe

-include $(DEPS)

$(TARGET):	$(OBJS) $(LIB_FILES)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(DEPS)

