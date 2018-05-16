TARGET: server

CXX	= g++
CXXFLAGS= -Wall -O2 -Wextra -std=c++17

server: main.o menu.o
	$(CXX) -o main main.o menu.o

main.o: main.cc menu.h
	$(CXX) -c $(CXXFLAGS) main.cc

menu.o: menu.cc
	$(CXX) -c $(CXXFLAGS) menu.cc

.PHONY: clean TARGET
clean:
	rm -f main server *.o *~ *.bak
