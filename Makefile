CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pthread -Iinclude

SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:src/%.cpp=build/%.o)
TARGET := log_monitor

BENCHMARK_SRC := tests/benchmark.cpp
BENCHMARK_BIN := benchmark_bin

.PHONY: all clean run benchmark

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build:
	mkdir -p build

$(BENCHMARK_BIN): $(BENCHMARK_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $

benchmark: $(BENCHMARK_BIN)
	./$(BENCHMARK_BIN)

run: all
	./$(TARGET)

clean:
	rm -rf build $(TARGET) $(BENCHMARK_BIN)