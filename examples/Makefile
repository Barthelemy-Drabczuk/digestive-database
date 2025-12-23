# Examples Makefile

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -I../include
LDFLAGS := -llz4 -lzstd

SRC_DIR := ..
BUILD_DIR := build

# Common source files
COMMON_SOURCES := $(SRC_DIR)/src/digestive_database.cpp \
                  $(SRC_DIR)/src/chunking_engine.cpp \
                  $(SRC_DIR)/src/index_engine.cpp \
                  $(SRC_DIR)/src/sql_engine.cpp

COMMON_OBJECTS := $(BUILD_DIR)/digestive_database.o \
                  $(BUILD_DIR)/chunking_engine.o \
                  $(BUILD_DIR)/index_engine.o \
                  $(BUILD_DIR)/sql_engine.o

# Targets
TARGETS := simple_example hybrid_demo

all: $(BUILD_DIR) $(TARGETS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build simple_example
simple_example: $(BUILD_DIR)/simple_example.o $(COMMON_OBJECTS)
	$(CXX) $^ -o $@ $(LDFLAGS)
	@echo "✓ Build complete: simple_example"

# Build hybrid_demo
hybrid_demo: $(BUILD_DIR)/hybrid_demo.o $(COMMON_OBJECTS)
	$(CXX) $^ -o $@ $(LDFLAGS)
	@echo "✓ Build complete: hybrid_demo"

# Compile example source files
$(BUILD_DIR)/simple_example.o: simple_example.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/hybrid_demo.o: hybrid_demo.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile common source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGETS) *.db
	@echo "Cleaned build files and databases"

run-simple: simple_example
	@echo "\n=== Running Simple Example ==="
	./simple_example

run-hybrid: hybrid_demo
	@echo "\n=== Running Hybrid Demo ==="
	./hybrid_demo

run: run-simple

help:
	@echo "Examples Makefile - Available targets:"
	@echo ""
	@echo "  make                - Build all examples"
	@echo "  make simple_example - Build simple example only"
	@echo "  make hybrid_demo    - Build hybrid demo only"
	@echo "  make run            - Run simple example (default)"
	@echo "  make run-simple     - Run simple example"
	@echo "  make run-hybrid     - Run hybrid demo"
	@echo "  make clean          - Remove all build files"
	@echo "  make help           - Show this help"

.PHONY: all clean run run-simple run-hybrid help
