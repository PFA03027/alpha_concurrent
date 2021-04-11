
include ../common.mk

CXXFLAGS =	-c -MMD -MP $(CXX_STD) -O2 -g -Wall -I./inc

SRCS := $(wildcard src/*.cpp)
OBJS := $(SRCS:%.cpp=%.o)
DEPS := $(SRCS:%.cpp=%.d)

LIBS = 

TARGET =	libalconcurrent.a

-include $(DEPS)

$(TARGET):	$(OBJS)
	$(AR) rcs $(TARGET) $(OBJS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(DEPS)

