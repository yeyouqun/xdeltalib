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

TESTCAPI_OBJS = ./test/testcapi.o

XDELTA_OBJS =  capi.o \
                xdeltalib.o \
                tinythread.o \
                rollsum.o \
								md4.o \
								platform.o \
                rw.o \

CXX      := g++


all: xdelta test

%.o:%.cpp
	$(CXX) -I. $(CXXFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
	
xdelta: $(XDELTA_OBJS)
	$(CXX) $(LDFLAGS) -o libxdelta.so $^
	
test:xdelta $(TESTCAPI_OBJS) 
	$(CXX) -Wl,-rpath,. -o testcapi $(TESTCAPI_OBJS) $(CXXFLAGS)  $(EXTRA_CFLAGS)  \
                -Wno-deprecated -L. -lxdelta $(TEST_LD_FLAGS)
                
clean:
	rm -f *.o libxdelta.so* server client testcapi similarity $(SERVER_OBJS) $(CLIENT_OBJS) \
	$(TESTCAPI_OBJS) $(SIMILARITY_OBJS) $(DIFF_CALLBACK) diffcb
