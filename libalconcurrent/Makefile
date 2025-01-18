
include ../common.mk

CXXFLAGS +=	-c -MMD -MP $(CXX_STD) -O2 -g -Wall -I./inc/ -I./src_mem/ -I./src/

SRCS := $(wildcard src/*.cpp src_mem/*.cpp)
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

