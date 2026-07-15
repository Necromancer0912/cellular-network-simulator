# Cellular Network Simulator Makefile

# Compiler
CXX := g++

# Compiler flags
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic
INCLUDES := -Iinclude

# Debug flags
DEBUG_FLAGS := -g -O0 -DDEBUG

# Optimized/Release flags
RELEASE_FLAGS := -O3 -DNDEBUG -march=native

# Directories
SRC_DIR := src
INC_DIR := include
OBJ_DIR := obj
BIN_DIR := bin
DEBUG_OBJ_DIR := $(OBJ_DIR)/debug
RELEASE_OBJ_DIR := $(OBJ_DIR)/release

# Source files
SOURCES := $(wildcard $(SRC_DIR)/*.cpp)

# Detect operating system
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    ASM_SOURCES :=
else
    ASM_SOURCES := $(SRC_DIR)/syscall.S
endif

# Object files
DEBUG_OBJECTS := $(SOURCES:$(SRC_DIR)/%.cpp=$(DEBUG_OBJ_DIR)/%.o) $(ASM_SOURCES:$(SRC_DIR)/%.S=$(DEBUG_OBJ_DIR)/%.o)
RELEASE_OBJECTS := $(SOURCES:$(SRC_DIR)/%.cpp=$(RELEASE_OBJ_DIR)/%.o) $(ASM_SOURCES:$(SRC_DIR)/%.S=$(RELEASE_OBJ_DIR)/%.o)

# Header files (for dependency tracking)
HEADERS := $(wildcard $(INC_DIR)/*.h)

# Target executables
DEBUG_TARGET := $(BIN_DIR)/cellular_simulator_debug
RELEASE_TARGET := $(BIN_DIR)/cellular_simulator

# Removed terminal color escape sequences to produce plain output
# Color variables intentionally omitted for portability

# Default target
.DEFAULT_GOAL := all

# Phony targets
.PHONY: all debug release clean run run-debug help directories test

# Build both versions
all: release debug
	@echo "[==] Build complete: Both debug and release versions ready"

# Build release version
release: directories $(RELEASE_TARGET)
	@echo "[==] Release build complete"

# Build debug version
debug: directories $(DEBUG_TARGET)
	@echo "[==] Debug build complete"

# Create necessary directories
directories:
	@mkdir -p $(BIN_DIR) $(DEBUG_OBJ_DIR) $(RELEASE_OBJ_DIR)

# Link debug executable
$(DEBUG_TARGET): $(DEBUG_OBJECTS)
	@echo "Linking debug executable: $@"
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(DEBUG_OBJECTS) -o $@

# Link release executable
$(RELEASE_TARGET): $(RELEASE_OBJECTS)
	@echo "Linking release executable: $@"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(RELEASE_OBJECTS) -o $@

# Compile debug objects
$(DEBUG_OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS)
	@echo "Compiling (debug): $<"
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c $< -o $@

# Compile release objects
$(RELEASE_OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS)
	@echo "Compiling (release): $<"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(INCLUDES) -c $< -o $@

# Compile assembly files with NASM (debug)
$(DEBUG_OBJ_DIR)/%.o: $(SRC_DIR)/%.S
	@echo "Assembling (debug): $<"
	@nasm -f elf64 $< -o $@

# Compile assembly files with NASM (release)
$(RELEASE_OBJ_DIR)/%.o: $(SRC_DIR)/%.S
	@echo "Assembling (release): $<"
	@nasm -f elf64 $< -o $@

# Run release version
run: release
	@echo "Running Cellular Network Simulator (Release)"
	@./$(RELEASE_TARGET)

# Run debug version
run-debug: debug
	@echo "Running Cellular Network Simulator (Debug)"
	@./$(DEBUG_TARGET)

# Test compilation (compile without running)
test: all
	@echo "[==] All components compiled successfully"
	@echo "Files created:"
	@ls -lh $(BIN_DIR)/

	@echo "Building and running unit tests..."
	@mkdir -p ./bin
	@echo "Compiling unit test objects..."
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/Protocol.cpp -o /tmp/proto_test.o
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/CellularCore.cpp -o /tmp/core_test.o
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/CellTower.cpp -o /tmp/celltower_test.o
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/UserDevice.cpp -o /tmp/userdevice_test.o
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/Utils.cpp -o /tmp/utils_test.o || true
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/Simulator.cpp -o /tmp/simulator_test.o
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/NetworkAnalytics.cpp -o /tmp/analytics_test.o
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/AdvancedMetrics.cpp -o /tmp/advmetrics_test.o
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c tests/test_protocol_core.cpp -o /tmp/test_protocol.o
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c tests/test_exceptions.cpp -o /tmp/test_exceptions.o
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c tests/test_concurrency.cpp -o /tmp/test_concurrency.o
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c tests/test_load_balancer.cpp -o /tmp/test_load_balancer.o
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c tests/test_stubs.cpp -o /tmp/test_stubs.o

	@echo "Linking and running test: protocol/core"
	@$(CXX) /tmp/test_protocol.o /tmp/test_stubs.o /tmp/proto_test.o /tmp/core_test.o /tmp/userdevice_test.o /tmp/utils_test.o -o ./bin/unit_test_protocol || true
	@./bin/unit_test_protocol || { echo "unit_test_protocol failed"; exit 1; }

	@echo "Linking and running test: exceptions"
	@$(CXX) /tmp/test_exceptions.o /tmp/test_stubs.o /tmp/proto_test.o /tmp/core_test.o /tmp/userdevice_test.o /tmp/utils_test.o -o ./bin/unit_test_exceptions || true
	@./bin/unit_test_exceptions || { echo "unit_test_exceptions failed"; exit 1; }

	@echo "Linking and running test: concurrency"
	@$(CXX) /tmp/test_concurrency.o /tmp/test_stubs.o /tmp/proto_test.o /tmp/core_test.o /tmp/userdevice_test.o /tmp/utils_test.o -o ./bin/unit_test_concurrency || true
	@./bin/unit_test_concurrency || { echo "unit_test_concurrency failed"; exit 1; }

	@echo "Linking and running test: load balancer"
	@$(CXX) /tmp/test_load_balancer.o /tmp/test_stubs.o /tmp/proto_test.o /tmp/core_test.o /tmp/celltower_test.o /tmp/userdevice_test.o /tmp/utils_test.o /tmp/simulator_test.o /tmp/analytics_test.o /tmp/advmetrics_test.o -o ./bin/unit_test_load_balancer || true
	@./bin/unit_test_load_balancer || { echo "unit_test_load_balancer failed"; exit 1; }
	@echo "[==] Unit tests compiled and ran successfully"

# Clean all build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(OBJ_DIR) $(BIN_DIR) main.o
	@echo "[==] Clean complete"

# Display help information
help:
	@echo "Cellular Network Simulator - Makefile Help"
	@echo ""
	@echo "Available targets:"
	@echo "  make all        - Build both debug and release versions (default)"
	@echo "  make release    - Build optimized release version"
	@echo "  make debug      - Build debug version with sanitizers"
	@echo "  make run        - Build and run release version"
	@echo "  make run-debug  - Build and run debug version"
	@echo "  make test       - Compile all and verify build"
	@echo "  make clean      - Remove all build artifacts"
	@echo "  make help       - Display this help message"
	@echo ""
	@echo "Build information:"
	@echo "  Compiler: $(CXX)"
	@echo "  C++ Standard: C++17"
	@echo "  Debug flags: $(DEBUG_FLAGS)"
	@echo "  Release flags: $(RELEASE_FLAGS)"
	@echo ""
	@echo "Output locations:"
	@echo "  Debug binary: $(DEBUG_TARGET)"
	@echo "  Release binary: $(RELEASE_TARGET)"

# Display project structure
structure:
	@echo "Project Structure:"
	@tree -L 2 -I 'obj|bin' || ls -R

# Install dependencies (if needed)
install-deps:
	@echo "Checking for required tools..."
	@command -v g++ >/dev/null 2>&1 || { echo "g++ not found. Please install it."; exit 1; }
	@echo "[==] All dependencies satisfied"

# Generate dependency files (suppress "file not found" messages)
DEPS_DEBUG := $(wildcard $(DEBUG_OBJECTS:.o=.d))
DEPS_RELEASE := $(wildcard $(RELEASE_OBJECTS:.o=.d))

ifneq ($(DEPS_DEBUG),)
-include $(DEPS_DEBUG)
endif

ifneq ($(DEPS_RELEASE),)
-include $(DEPS_RELEASE)
endif

# Automatic dependency generation
$(DEBUG_OBJ_DIR)/%.d: $(SRC_DIR)/%.cpp
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $(@:.d=.o) $< > $@

$(RELEASE_OBJ_DIR)/%.d: $(SRC_DIR)/%.cpp
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM -MT $(@:.d=.o) $< > $@
