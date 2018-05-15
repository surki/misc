CC=gcc
CXX=g++
RM=rm -f
SANITIZER_CPPFLAGS= #-fsanitize=address
SANITIZER_LIBS= #-lasan
CPPFLAGS=-g $(SANITIZER_CPPFLAGS) -I$(CURDIR)/source -I$(CURDIR)/include 
LDFLAGS=-g -L/usr/local/lib64/
LDLIBS=$(SANITIZER_LIBS) -ltins -levent -lfmt

SRCS=source/common/buffer/buffer_impl.cc codec.cc test.cc
OBJS=$(subst .cc,.o,$(SRCS))

all: test

test: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS) 

depend: .depend

.depend: $(SRCS)
	$(RM) ./.depend
	$(CXX) $(CPPFLAGS) -MM $^>>./.depend;

clean:
	$(RM) $(OBJS)

distclean: clean
	$(RM) *~ .depend

include .depend
