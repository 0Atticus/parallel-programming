CXX      := g++
STD      := -std=c++17
WARN     := -Wall -Wextra
OPT      := -O3
LDFLAGS  :=

SRC_DIR  := src
BIN_DIR  := bin

PART1    := $(BIN_DIR)/arrayMax $(BIN_DIR)/cachesweep
PART2    := $(BIN_DIR)/exclusivePrefix_O0 \
            $(BIN_DIR)/exclusivePrefix_O2 \
            $(BIN_DIR)/exclusivePrefix_O3
PART3    := $(BIN_DIR)/Matmul
PART4    := $(BIN_DIR)/Mergesort
PART5    := $(BIN_DIR)/Bfs
TARGETS  := $(PART1) $(PART2) $(PART3) $(PART4) $(PART5)

.PHONY: all clean asm
all: $(TARGETS)

$(BIN_DIR)/Matmul: OPT := -O2

$(BIN_DIR)/exclusivePrefix_O%: $(SRC_DIR)/exclusivePrefix.cpp | $(BIN_DIR)
	$(CXX) $(STD) $(WARN) -O$* -DOPT_LABEL='"O$*"' $< -o $@ $(LDFLAGS)

$(BIN_DIR)/%: $(SRC_DIR)/%.cpp | $(BIN_DIR)
	$(CXX) $(STD) $(WARN) $(OPT) $< -o $@ $(LDFLAGS)

$(BIN_DIR):
	mkdir -p $@

asm: $(SRC_DIR)/arrayMax.cpp | $(BIN_DIR)
	$(CXX) $(STD) $(WARN) $(OPT) -S $< -o $(BIN_DIR)/arrayMax.s

clean:
	rm -rf $(BIN_DIR)