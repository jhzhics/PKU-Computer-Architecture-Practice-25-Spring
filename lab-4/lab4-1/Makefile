CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++17 -mavx2 -mfma -O3
# CXXFLAGS = -g -std=c++17
BUILD = build

SRC = $(wildcard src/*.cpp)
EXES = $(patsubst src/%.cpp, $(BUILD)/%, $(SRC))

all:$(EXES)

$(BUILD)/%: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -rf $(BUILD)

.PHONY: clean all
