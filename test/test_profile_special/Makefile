
include ../../common.mk

CXXFLAGS +=	-c -MMD -MP $(CXX_STD) -O2 -g -Wall -I../../libalconcurrent/inc -I../googletest/googletest/include

SRCS := $(wildcard src/*.cpp)
OBJS := $(SRCS:%.cpp=%.o)
DEPS := $(SRCS:%.cpp=%.d)
LIB_FILES :=  ../../libalconcurrent/libalconcurrent.a ../build_googletest/build/lib/libgtest.a ../build_googletest/build/lib/libgtest_main.a

LIBS += -L../../libalconcurrent -L../build_googletest/build/lib
LIBS += -lalconcurrent -lpthread -lgtest -lgtest_main

TARGET =	test_profile_special.exe

-include $(DEPS)

$(TARGET):	$(OBJS) $(LIB_FILES)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(DEPS)

