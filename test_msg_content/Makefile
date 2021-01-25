CXXFLAGS =	-c -MMD -MP -std=c++17 -O2 -g -Wall -fmessage-length=0 -I../libalconcurrent/inc

SRCS := $(wildcard *.cpp)
OBJS := $(SRCS:%.cpp=%.o)
DEPS := $(SRCS:%.cpp=%.d)
LIB_FILES :=  ../libalconcurrent/Debug/libalconcurrent.a

LIBS = -L../libalconcurrent/Debug -lalconcurrent

TARGET =	test_msg_content.exe

-include $(DEPS)

$(TARGET):	$(OBJS) $(LIB_FILES)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(DEPS)

