# Pawn Star build
#   make            optimized engine -> ./pawnstar
#   make debug      AddressSanitizer + UBSan build -> ./pawnstar-debug
#   make test       build, then run the perft suite
#   make clean

CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -MMD -MP
RELEASE  := -O3 -DNDEBUG
DEBUG    := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer

# -march=native lets the compiler use popcnt/bmi on x86; Apple Silicon (arm64)
# already targets the host CPU by default, so add nothing there.
ifeq ($(shell uname -m),x86_64)
    RELEASE += -march=native
endif

SRC       := $(wildcard src/*.cpp)
OBJ       := $(SRC:src/%.cpp=build/release/%.o)
DEBUG_OBJ := $(SRC:src/%.cpp=build/debug/%.o)

.PHONY: all debug test clean

all: pawnstar

pawnstar: $(OBJ)
	$(CXX) $(CXXFLAGS) $(RELEASE) $^ -o $@

pawnstar-debug: $(DEBUG_OBJ)
	$(CXX) $(CXXFLAGS) $(DEBUG) $^ -o $@

debug: pawnstar-debug

build/release/%.o: src/%.cpp | build/release
	$(CXX) $(CXXFLAGS) $(RELEASE) -c $< -o $@

build/debug/%.o: src/%.cpp | build/debug
	$(CXX) $(CXXFLAGS) $(DEBUG) -c $< -o $@

build/release build/debug:
	mkdir -p $@

test: pawnstar
	./pawnstar perft-suite

clean:
	rm -rf build pawnstar pawnstar-debug

-include $(OBJ:.o=.d) $(DEBUG_OBJ:.o=.d)
