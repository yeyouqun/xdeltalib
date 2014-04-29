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

CXXFLAGS := -DPIC -fpic -fPIC -Wno-deprecated -m64 -D_LINUX -I..
# -D_UNIX if you compile it on unix
EXTRA_CFLAGS :=  -Wall -Wno-format -Wdeprecated-declarations

ifeq ($(STD),1)
	EXTRA_CFLAGS += -std=c++0x -D__CXX_11__ 
endif

LDFLAGS += -shared -pthread -Wl,-rpath,/usr/local/lib
#-lsocket if you compile on unix.
#-Wl,-rpath,/usr/local/lib if you have self-installed gcc compiler.
ifeq ($(DEBUG),1)
EXTRA_CFLAGS += -g
else
EXTRA_CFLAGS += -O2
endif

SERVER_OBJS = ./test/testserver.o
CLIENT_OBJS = ./test/testclient.o

XDELTA_OBJS =  active_socket.o \
                BigInteger.o \
                BigIntegerAlgorithms.o \
                BigIntegerUtils.o \
                BigUnsigned.o \
                BigUnsignedInABase.o \
                fnv64.o \
                inplace.o \
                lz4.o \
                lz4hc.o \
                md4.o \
                md5.o \
                multiround.o \
                passive_socket.o \
                platform.o \
                reconstruct.o \
                rollsim.o \
                rollsum.o \
                rw.o \
                sha1.o \
                simhash.o \
                simple_socket.o \
                stream.o \
                tinythread.o \
                xdeltalib.o \
                xdelta_client.o \
                xdelta_server.o \
                xxhash.o

CXX      := g++


all: xdelta test-server test-client

%.o:%.cpp
	$(CXX) -I. $(CXXFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
	
xdelta: $(XDELTA_OBJS)
	$(CXX) $(LDFLAGS) -o libxdelta.so $^
	
test-server:xdelta $(SERVER_OBJS)
	$(CXX) -Wl,-rpath,. -o server $(SERVER_OBJS) $(CXXFLAGS)  $(EXTRA_CFLAGS)  \
                -Wno-deprecated -L. -lxdelta $(TEST_LD_FLAGS)
                
test-client:xdelta $(CLIENT_OBJS)
	$(CXX) -Wl,-rpath,. -o client $(CLIENT_OBJS) $(CXXFLAGS)  $(EXTRA_CFLAGS)  \
                -Wno-deprecated -L. -lxdelta $(TEST_LD_FLAGS)
                   
clean:
	rm -f *.o libxdelta.so* server client $(SERVER_OBJS) $(CLIENT_OBJS)
