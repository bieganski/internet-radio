odb = sikradio-sender
nad = sikradio-receiver
HELP = menu.o utils.o audio.o sock.o mess.o
TARGET: sender receiver

CXX	= g++
CXXFLAGS= -lpthread -Wall -O2 -Wextra -std=c++17

sender: $(nad).o $(HELP)
	$(CXX) $(CXXFLAGS) -o $(nad) $(nad).o $(HELP)

receiver: $(odb).o $(HELP)
	$(CXX) $(CXXFLAGS) -o $(odb) $(odb).o $(HELP)

$(nad).o: $(HELP)
	$(CXX) -c $(CXXFLAGS) $(nad).cc

$(odb).o: $(HELP)
	$(CXX) -c $(CXXFLAGS) $(odb).cc


menu.o:
	$(CXX) -c $(CXXFLAGS) Menu.cc -o menu.o

utils.o: utils.cc
	$(CXX) -c $(CXXFLAGS) utils.cc -o utils.o

audio.o: AudioFIFO.cc
	$(CXX) -c $(CXXFLAGS) AudioFIFO.cc -o audio.o

sock.o: GroupSock.cc
	$(CXX) -c $(CXXFLAGS) GroupSock.cc -o sock.o

mess.o: MessageParser.cc
	$(CXX) -c $(CXXFLAGS) MessageParser.cc -o mess.o

.PHONY: clean TARGET
clean:
	rm -f $(nad) $(odb) *.o *~ *.bak
