BUILD_DIR ?= build
BUILD_TYPE ?= Release
NPROC := $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

.PHONY: all configure build test test-unit test-integration clean rebuild run

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

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
	$(BUILD_DIR)/bin/tokenizer
