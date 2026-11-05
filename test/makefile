CXX = g++
CXXFLAGS = -std=c++11 
DEBUG_FLAGS = -g3 -O0 -Wall -Wextra -pthread

.PHONY: all clean
all: server client

server: main.cpp ../src/server.hpp
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $< -o $@

client: test_client.cpp ../src/server.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f server client
