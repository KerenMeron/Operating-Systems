CC=g++
RANLIB=ranlib

LIBSRC=MapReduceFramework.cpp
LIBOBJ=$(LIBSRC:.cpp=.o)

INCS=-I.
CXXFLAGS = -Wall -g $(INCS)
LOADLIBES = -L./ 

TARGETS = MapReduceFramework.a Search

TAR=tar
TARFLAGS=-cvf
TARNAME=ex3.tar
TARSRCS=$(LIBSRC) Makefile README Search.cpp

all: $(TARGETS)

%.o: %.h

Search: Search.cpp MapReduceClient.h MapReduceFramework.h MapReduceFramework.cpp
	$(CC) -g $^ -o $@ -lpthread

MapReduceFramework.a: $(LIBOBJ)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@

clean:
	$(RM) $(TARGETS) $(OSMLIB) $(OBJ) $(LIBOBJ) *~ *core

valgrind: Search
	valgrind --leak-check=full --show-possibly-lost=yes --show-reachable=yes --undef-value-errors=yes --track-origins=yes $< og folder1 folder2 folder3

depend:
	makedepend -- $(CXXFLAGS) -- $(SRC) $(LIBSRC)

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)

.PHONY:	all clean valgrind tar Search
