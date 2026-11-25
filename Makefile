CXX      := g++
CC       := gcc
CXXFLAGS := -std=c++23 -O2 -Wall -Wextra -fPIC
CFLAGS   := -O2 -Wall -Wextra
GTKFLAGS := $(shell pkg-config --cflags --libs gtk+-3.0 cairo)

BUILD_DIR := build
SRC_DIR   := src

APP       := $(BUILD_DIR)/hypreyes
PLUGIN    := $(BUILD_DIR)/hypreyes-plugin.so

.PHONY: all clean

all: $(APP) $(PLUGIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(APP): $(SRC_DIR)/hypreyes.c | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -x c++ $< -o $@ $(GTKFLAGS) -lm

$(PLUGIN): $(SRC_DIR)/hypreyes-plugin.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -shared $< -o $@ $$(pkg-config --cflags --libs hyprland)

clean:
	rm -rf $(BUILD_DIR)
