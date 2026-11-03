CXX=g++
CXXFLAGS=-Wall -Wextra -std=c++17 -pthread -g -Iinclude

fuentes=$(wildcard src/*.cpp)
objetos=$(fuentes:src/%.cpp=build/%.o)

torneo: $(objetos)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build torneo

.PHONY: clean
