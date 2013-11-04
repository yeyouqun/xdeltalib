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

CFLAGS := -DPIC -fpic -fPIC -pthread -D_LINUX # -D__CXX11
EXTRA_CFLAGS :=  -Wall -Wno-format -Wdeprecated-declarations
ifeq ($(STD),1)
EXTRA_CFLAGS += -std=c++0x -D__CXX_11__ 
else
EXTRA_CFLAGS += -Wno-deprecated -Wl,-rpath,/usr/local/lib64 # -Wl,-soname,libxdelta.so.1
endif

TEST_EXTRA_CFLAGS = -D_LINUX

LDFLAGS += -shared  -fPIC -fpic
ifeq ($(DEBUG),1)
EXTRA_CFLAGS += -g
TEST_EXTRA_CFLAGS += -g
else
EXTRA_CFLAGS += -O2
LDFLAGS += -O2
TEST_EXTRA_CFLAGS += -O2
endif

ifeq ($(V),1)
EXTRA_CFLAGS += -DVERBOSE
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
	$(CXX) -Wl,-rpath,. -o $@ testserver.cpp $(EXTRA_CFLAGS) -pthread $(TEST_EXTRA_CFLAGS)  \
                -Wno-deprecated -L. -lxdelta -fPIC -fpic -DPIC -Wl,-rpath,/usr/local/lib64
                
test-client:xdelta testclient.cpp
	$(CXX) -Wl,-rpath,. -o $@ testclient.cpp $(EXTRA_CFLAGS) -pthread $(TEST_EXTRA_CFLAGS)  \
                -Wno-deprecated -L. -lxdelta -fPIC -fpic -DPIC -Wl,-rpath,/usr/local/lib64
                
clean:
	rm -f *.o libxdelta.so* test-server test-client
