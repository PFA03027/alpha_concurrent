CXXFLAGS =	-c -MMD -MP -std=c++17 -O2 -g -Wall -I./inc

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

