# Digestive Database Makefile

# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -I./include
LDFLAGS := -llz4 -lzstd

# Directories
SRC_DIR := src
BUILD_DIR := build
INCLUDE_DIR := include

# Source files
SOURCES := $(SRC_DIR)/digestive_database.cpp $(SRC_DIR)/main.cpp
OBJECTS := $(BUILD_DIR)/digestive_database.o $(BUILD_DIR)/main.o

# Target executable
TARGET := digestive_db

# Default target
all: $(BUILD_DIR) $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build executable
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -rf $(BUILD_DIR) $(TARGET) *.db
	@echo "Cleaned build files and databases"

# Run the program
run: $(TARGET)
	./$(TARGET)

# Install dependencies (Ubuntu/Debian)
install-deps:
	@echo "Installing dependencies..."
	sudo apt-get update
	sudo apt-get install -y liblz4-dev libzstd-dev g++ make

# Help
help:
	@echo "Digestive Database - Makefile Commands"
	@echo ""
	@echo "  make              - Build the project"
	@echo "  make clean        - Remove build files and databases"
	@echo "  make run          - Build and run the program"
	@echo "  make install-deps - Install required dependencies (Ubuntu/Debian)"
	@echo "  make help         - Show this help message"

.PHONY: all clean run install-deps help
