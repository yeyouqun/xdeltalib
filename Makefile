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
TESTCAPI_OBJS = ./test/testcapi.o
SIMILARITY_OBJS = ./test/similarity.o
DIFF_CALLBACK = ./test/testdiffcb.o

XDELTA_OBJS =  active_socket.o \
                inplace.o \
                lz4.o \
                lz4hc.o \
                multiround.o \
                passive_socket.o \
                platform.o \
                reconstruct.o \
                rollsum.o \
                rw.o \
						md4.o \
                simple_socket.o \
                stream.o \
                tinythread.o \
                xdeltalib.o \
                xdelta_client.o \
                xdelta_server.o \
                xxhash.o \
                capi.o \

CXX      := g++


all: xdelta test

%.o:%.cpp
	$(CXX) -I. $(CXXFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
	
xdelta: $(XDELTA_OBJS)
	$(CXX) $(LDFLAGS) -o libxdelta.so $^
	
test-server:xdelta $(SERVER_OBJS)

                
test:xdelta $(CLIENT_OBJS)  $(SERVER_OBJS)  $(TESTCAPI_OBJS) $(SIMILARITY_OBJS) \
			$(DIFF_CALLBACK)
	$(CXX) -Wl,-rpath,. -o client $(CLIENT_OBJS) $(CXXFLAGS)  $(EXTRA_CFLAGS)  \
                -Wno-deprecated -L. -lxdelta $(TEST_LD_FLAGS)
                
	$(CXX) -Wl,-rpath,. -o server $(SERVER_OBJS) $(CXXFLAGS)  $(EXTRA_CFLAGS)  \
                -Wno-deprecated -L. -lxdelta $(TEST_LD_FLAGS)
                
	$(CXX) -Wl,-rpath,. -o testcapi $(TESTCAPI_OBJS) $(CXXFLAGS)  $(EXTRA_CFLAGS)  \
                -Wno-deprecated -L. -lxdelta $(TEST_LD_FLAGS)
                
	$(CXX) -Wl,-rpath,. -o similarity $(SIMILARITY_OBJS) $(CXXFLAGS)  $(EXTRA_CFLAGS)  \
                -Wno-deprecated -L. -lxdelta $(TEST_LD_FLAGS)

	$(CXX) -Wl,-rpath,. -o diffcb $(DIFF_CALLBACK) $(CXXFLAGS)  $(EXTRA_CFLAGS)  \
                -Wno-deprecated -L. -lxdelta $(TEST_LD_FLAGS)                  
clean:
	rm -f *.o libxdelta.so* server client testcapi similarity $(SERVER_OBJS) $(CLIENT_OBJS) \
	$(TESTCAPI_OBJS) $(SIMILARITY_OBJS) $(DIFF_CALLBACK) diffcb
