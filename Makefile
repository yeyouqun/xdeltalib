###############################################################################
#
# A smart Makefile template for GNU/LINUX programming
#
# Author: PRC (yeyouqun@163.com)
# Date:   2013/08/29
#
# Usage:
#   $ make           Compile and link (or archive)
#   $ make clean     Clean the objectives and target.
###############################################################################

CFLAGS := -DPIC -fpic -fPIC -Wno-deprecated


ifeq ($(P), U32) 
	CFLAGS += -D_UNIX -m32
endif

ifeq ($(P), U64)
	CFLAGS += -D_UNIX -m64
endif

ifeq ($(P),L32)
	CFLAGS += -D_LINUX  -m32
endif

ifeq ($(P),L64)
	CFLAGS += -D_LINUX  -m64
endif

EXTRA_CFLAGS :=  -Wall -Wno-format -Wdeprecated-declarations

ifeq ($(STD),1)
	EXTRA_CFLAGS += -std=c++0x -D__CXX_11__ 
endif

LDFLAGS += -shared

ifeq ($(P),L32)
	LDFLAGS += -pthread -m32 -Wl,-rpath,/usr/local/lib
	TEST_LD_FLAGS := -Wl,-rpath,/usr/local/lib
endif

ifeq ($(P),L64)
	LDFLAGS += -pthread -m64 -Wl,-rpath,/usr/local/lib64
	TEST_LD_FLAGS := -Wl,-rpath,/usr/local/lib64
endif

ifeq ($(P),U32)
	LDFLAGS += -lsocket -m32
endif

ifeq ($(P),U64)
	LDFLAGS += -lsocket -m64
endif

ifeq ($(DEBUG),1)
EXTRA_CFLAGS += -g
else
EXTRA_CFLAGS += -O2
endif

CXX      := g++


all: xdelta test-server test-client

digest.o:digest.cpp
	$(CXX) -I. $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
	
platform.o:platform.cpp
	$(CXX) -I. $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
	
xdeltalib.o:xdeltalib.cpp
	$(CXX) -I. $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
	
rollsum.o:rollsum.cpp
	$(CXX) -I. $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
    
tinythread.o:tinythread.cpp
	$(CXX) -I. $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
	
rw.o:rw.cpp
	$(CXX) -I. $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
	
reconstruct.o:reconstruct.cpp
	$(CXX) -I. $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
	
multiround.o:multiround.cpp
	$(CXX) -I. $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
	
inplace.o:inplace.cpp
	$(CXX) -I. $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
	
active_socket.o:active_socket.cpp
	$(CXX) -I. $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
	
passive_socket.o:passive_socket.cpp
	$(CXX) -I. $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
	
simple_socket.o:simple_socket.cpp
	$(CXX) -I. $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<

xdelta_client.o:xdelta_client.cpp
	$(CXX) -I. $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<

xdelta_server.o:xdelta_server.cpp
	$(CXX) -I. $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
	
stream.o:stream.cpp
	$(CXX) -I. $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
	
xdelta: digest.o platform.o xdeltalib.o rollsum.o tinythread.o \
			 rw.o reconstruct.o multiround.o inplace.o simple_socket.o active_socket.o \
			 passive_socket.o xdelta_server.o xdelta_client.o stream.o
	$(CXX) $(LDFLAGS) -o libxdelta.so $^
	
test-server:xdelta testserver.cpp
	$(CXX) -Wl,-rpath,. -o $@ testserver.cpp $(CFLAGS)  $(EXTRA_CFLAGS)  \
                -Wno-deprecated -L. -lxdelta $(TEST_LD_FLAGS)
                
test-client:xdelta testclient.cpp
	$(CXX) -Wl,-rpath,. -o $@ testclient.cpp $(CFLAGS)  $(EXTRA_CFLAGS)  \
                -Wno-deprecated -L. -lxdelta $(TEST_LD_FLAGS)
                
clean:
	rm -f *.o libxdelta.so* test-server test-client
