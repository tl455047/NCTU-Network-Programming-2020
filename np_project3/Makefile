CXX=g++
CXXFLAGS=-std=c++14 -Wall -pedantic -pthread -lboost_system
CXX_INCLUDE_DIRS=/usr/local/include
CXX_INCLUDE_PARAMS=$(addprefix -I , $(CXX_INCLUDE_DIRS))
CXX_LIB_DIRS=/usr/local/lib
CXX_LIB_PARAMS=$(addprefix -L , $(CXX_LIB_DIRS))

CXX_WIN_FLAGS=-lws2_32 -lwsock32 -std=c++14 -lboost_system -lboost_thread

all: http_server.cpp
	$(CXX) http_server.cpp request_parser.cpp cgi_environ.cpp -o http_server $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)
	$(CXX) console.cpp request_parser.cpp cgi_environ.cpp cgi_environ.hpp -o console.cgi $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)
clean:
	rm -f http_server
	rm -f console.cgi
	rm -f cgi_server.exe
part1:
	$(CXX) http_server.cpp request_parser.cpp cgi_environ.cpp -o http_server $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)
	$(CXX) console.cpp request_parser.cpp cgi_environ.cpp cgi_environ.hpp -o console.cgi $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)
part2:
	$(CXX) http_server_win.cpp request_parser_win.cpp cgi_environ_win.cpp -o cgi_server.exe $(CXX_WIN_FLAGS)
