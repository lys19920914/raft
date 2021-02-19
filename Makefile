INCLUDES = -I./include -I../msgpack-c/include/ -I./

LIBS = lib/rpclib.a -lboost_system -lboost_log_setup -lboost_log -lboost_thread -lpthread

CPPFLAGS = -O0 -g -std=c++14 -Wall $(INCLUDES)

SRCS = $(wildcard src/*.cpp)
OBJS = $(patsubst src/%.cpp, obj/%.o, $(SRCS))

TARGETS = bin/node bin/client

all: $(TARGETS)

bin/node: obj/entry_logs.o obj/peer.o obj/raft_node.o example/obj/node.o
	$(CXX) $^ -o $@ $(LIBS)

bin/client: obj/raft_client.o example/obj/client.o
	$(CXX) $^ -o $@ $(LIBS)

obj/%.o: src/%.cpp
	$(CXX) $< -c -o $@ $(CPPFLAGS)

example/obj/%.o: example/%.cpp
	$(CXX) $< -c -o $@ $(CPPFLAGS)

clean:
	rm -f $(OBJS)
	rm -f example/obj/*.o
	rm $(TARGETS)

clear:
	rm -rf bin/node1
	rm -rf bin/node2
	rm -rf bin/node3