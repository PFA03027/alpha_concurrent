CXXFLAGS =	 -std=c++17 -O2 -g -Wall -fmessage-length=0 -I../libalconcurrent/inc

OBJS =		test_msg_content.o test_crc32.o

LIBS = -L../libalconcurrent/Debug -lalconcurrent

TARGET =	test_msg_content.exe

$(TARGET):	$(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
