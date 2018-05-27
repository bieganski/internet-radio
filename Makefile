odb = sikradio-sender
nad = sikradio-receiver

TARGET: sender receiver

CXX	= g++
CXXFLAGS= -Wall -O2 -Wextra -std=c++17

sender: $(nad).o menu.o utils.o
	$(CXX) -o $(nad) $(nad).o utils.o

receiver: $(odb).o menu.o utils.o
	$(CXX) -o $(odb) $(odb).o menu.o utils.o

$(nad).o:  menu.o utils.o
	$(CXX) -c $(CXXFLAGS) $(nad).cc

$(odb).o: menu.o
	$(CXX) -c $(CXXFLAGS) $(odb).cc

menu.o:
	$(CXX) -c $(CXXFLAGS) menu.cc

utils.o: utils.cc
	$(CXX) -c $(CXXFLAGS) utils.cc

.PHONY: clean TARGET
clean:
	rm -f main server *.o *~ *.bak
