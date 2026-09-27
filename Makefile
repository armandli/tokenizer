BUILD_DIR ?= build
BUILD_TYPE ?= Release
NPROC := $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

# C++26 needs GCC >= 14 (and CMake >= 3.30) on Linux, where the default g++ may
# be older. Prefer a versioned g++ if one is installed; override with
# CXX_COMPILER=... . macOS uses the default Apple clang.
ifeq ($(shell uname -s),Linux)
CXX_COMPILER ?= $(firstword $(shell for c in g++-15 g++-14; do command -v $$c; done))
endif
CMAKE_ARGS := $(if $(CXX_COMPILER),-DCMAKE_CXX_COMPILER=$(CXX_COMPILER))

.PHONY: all configure build test test-unit test-integration clean rebuild run

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_ARGS)

build: configure
	cmake --build $(BUILD_DIR) -j$(NPROC)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

test-unit: build
	ctest --test-dir $(BUILD_DIR) -L unit --output-on-failure

test-integration: build
	ctest --test-dir $(BUILD_DIR) -L integration --output-on-failure

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean build

run: build
	$(BUILD_DIR)/bin/tokenize_bpe
