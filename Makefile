# Thin wrapper around CMake. Usage: make [all|configure|build|run|clean|rebuild]

BUILD_DIR ?= build
BUILD_TYPE ?= Release
QT_PREFIX ?= $(shell brew --prefix qt 2>/dev/null)

.PHONY: all configure build run clean rebuild

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		$(if $(QT_PREFIX),-DCMAKE_PREFIX_PATH=$(QT_PREFIX))

build: configure
	cmake --build $(BUILD_DIR) --parallel

run: build
	open $(BUILD_DIR)/UsbCam.app

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean build
