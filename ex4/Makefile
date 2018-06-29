CC=g++
RANLIB=ranlib

LIBSRC=CacheFS.cpp Cache.cpp
LIBOBJ=$(LIBSRC:.cpp=.o)

INCS=-I.
CXXFLAGS = -Wall -g $(INCS)
LOADLIBES = -L./ 

TARGETS = CacheFS.a

TAR=tar
TARFLAGS=-cvf
TARNAME=ex4.tar
TARSRCS=$(LIBSRC) Makefile README Cache.h Answers.pdf

all: $(TARGETS)

%.o: %.h

test: TEST.cpp CacheFS.h Cache.h CacheFS.cpp Cache.cpp
	$(CC) -g $^ -o $@

CacheFS.a: $(LIBOBJ)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@

clean:
	$(RM) $(TARGETS) $(OSMLIB) $(OBJ) $(LIBOBJ) *~ *core

valgrind: test
	valgrind --leak-check=full --show-possibly-lost=yes --show-reachable=yes --undef-value-errors=yes --track-origins=yes $<

depend:
	makedepend -- $(CXXFLAGS) -- $(SRC) $(LIBSRC)

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)

.PHONY:	all clean valgrind tar test
