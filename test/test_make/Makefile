INCLUDES = -I ./include
SRC = src/*.cpp
SRC2 = $(wildcard src/*.cpp)
OBJ = $(patsubst src/%.cpp, obj/%.o, $(SRC2))
CXXFLAGS = -std=c++11 -g -O0
DEPS = hellomake.h

TARGET = main

all: $(TARGET)


obj/main.o: example/main.cpp
	$(CXX) -c -o $@ $^ $(CXXFLAGS) $(INCLUDES)

obj/%.o: src/%.cpp
	$(CXX) -c -o $@ $^ $(CXXFLAGS) $(INCLUDES)

main: $(OBJ) obj/main.o
	$(CXX) -o $@ $^ $(CXXFLAGS) $(INCLUDES)

clean:
	rm $(OBJ)
	rm $(TARGET)

echo:
	@echo $(SRC)
	@echo $(OBJ)

dep/%.d: src/%.cpp
	$(CXX) -MM $(CXXFLAGS) $(INCLUDES) $< > $@
	sed -i '' '1s/^/$@ /' $@
	@echo hello
-include $(SRC:.cpp=.d)