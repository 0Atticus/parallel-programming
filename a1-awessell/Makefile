CXX := g++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra
LDFLAGS :=

SRC_DIR := src
BIN_DIR := bin

TARGETS := $(BIN_DIR)/arrayMax $(BIN_DIR)/cachesweep $(BIN_DIR)/exclusivePrefix

.PHONY: all clean
all: $(TARGETS)

$(BIN_DIR)/%: $(SRC_DIR)/%.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(BIN_DIR)