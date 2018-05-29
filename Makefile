odb = sikradio-sender
nad = sikradio-receiver

TARGET: sender receiver

CXX	= g++
CXXFLAGS= -lpthread -Wall -g -O2 -Wextra -std=c++17

sender: $(nad).o menu.o utils.o
	$(CXX) $(CXXFLAGS) -o $(nad) $(nad).o utils.o

receiver: $(odb).o menu.o utils.o
	$(CXX) $(CXXFLAGS) -o $(odb) $(odb).o menu.o utils.o

$(nad).o: menu.o utils.o
	$(CXX) -c $(CXXFLAGS) $(nad).cc

$(odb).o: menu.o utils.o
	$(CXX) -c $(CXXFLAGS) $(odb).cc

menu.o:
	$(CXX) -c $(CXXFLAGS) menu.cc

utils.o: utils.cc consts.hpp
	$(CXX) -c $(CXXFLAGS) utils.cc

.PHONY: clean TARGET
clean:
	rm -f $(nad) $(odb) *.o *~ *.bak
