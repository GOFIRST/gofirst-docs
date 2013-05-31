CC=g++
CXX=g++
CXXFLAGS=-Wall -Wno-sign-compare
LDFLAGS=-pthread

OBJS=BufferThreaded1.o PacketExample.o BufferThreaded1 PacketExample

all: BufferThreaded1 PacketExample

PacketExample.o: PacketExample.cpp BufferThreadedP.h

BufferThreaded1: BufferThreaded1.o

PacketExample: PacketExample.o

clean:
	\rm -f $(OBJS)
