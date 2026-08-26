CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pthread -Iinclude

SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:src/%.cpp=build/%.o)
TARGET := log_monitor

BENCHMARK_SRC := tests/benchmark.cpp
BENCHMARK_BIN := benchmark_bin
BENCHMARK_TSAN_BIN := benchmark_tsan

.PHONY: all clean run benchmark benchmark-tsan

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

benchmark-tsan: $(BENCHMARK_SRC)
	$(CXX) $(CXXFLAGS) -fsanitize=thread -g -o $(BENCHMARK_TSAN_BIN) $(BENCHMARK_SRC)
	./$(BENCHMARK_TSAN_BIN)

run: all
	./$(TARGET)

clean:
	rm -rf build $(TARGET) $(BENCHMARK_BIN) $(BENCHMARK_TSAN_BIN)