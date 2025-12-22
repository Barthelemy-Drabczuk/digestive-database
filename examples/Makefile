# Hybrid Demo Makefile

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -I../include
LDFLAGS := -llz4 -lzstd

SRC_DIR := ..
BUILD_DIR := build

# Source files
SOURCES := hybrid_demo.cpp \
           $(SRC_DIR)/src/digestive_database.cpp \
           $(SRC_DIR)/src/chunking_engine.cpp \
           $(SRC_DIR)/src/index_engine.cpp \
           $(SRC_DIR)/src/sql_engine.cpp

OBJECTS := $(BUILD_DIR)/hybrid_demo.o \
           $(BUILD_DIR)/digestive_database.o \
           $(BUILD_DIR)/chunking_engine.o \
           $(BUILD_DIR)/index_engine.o \
           $(BUILD_DIR)/sql_engine.o

TARGET := hybrid_demo

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

$(BUILD_DIR)/hybrid_demo.o: hybrid_demo.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET) *.db
	@echo "Cleaned build files and databases"

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
