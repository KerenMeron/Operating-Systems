
CXX = g++
CXXFLAGS = -Wextra -Wall -Wvla -g
CPPFLAGS = -DNDEBUG

CODE_FILES = whatsappClient.cpp whatsappServer.cpp
OBJECTS = whatsappClient.o whatsappServer.o

all: whatsappServer whatsappClient

whatsappServer: whatsappServer.cpp
	${CXX} ${CXXFLAGS} $^ -o $@

whatsappClient: whatsappClient.cpp
	${CXX} ${CXXFLAGS} $^ -o $@

clean: 
	rm -f $(OBJECTS) ex5.tar whatsappServer whatsappClient

valgrind: whatsappServer
	valgrind --leak-check=full --show-possibly-lost=yes --show-reachable=yes --undef-value-errors=yes --track-origins=yes $< 8080

tar: ex5.tar
ex5.tar: whatsappServer.cpp whatsappClient.cpp Makefile README
	tar -uvf $@ $?

.PHONY:	all clean valgrind tar whatsappServer whatsappClient
