## 1. General Compilation Targets
BUILD = build
$(shell mkdir -p $(BUILD)/objs)
OBJS = $(patsubst %.c, $(BUILD)/objs/%.o, $(notdir $(wildcard src/*.c)))
SIMULATOR = $(BUILD)/Simulator

## 2. General Compilation Flags
CC  = gcc
CXX = g++
LD  = $(CC)
CFLAGS   = -I./include -Wall
CXXFLAGS = $(CFLAGS)

## 3. Rules
all: $(SIMULATOR)

$(BUILD)/objs/%.o: src/%.c
	@echo + CC "->" $<
	@$(CC) -std=gnu11 $(CFLAGS) -c -o $@ $<

$(BUILD)/objs/%.o: src/%.cpp
	@echo + CXX "->" $<
	@$(CXX) -std=c++17 $(CXXFLAGS) -c -o $@ $<

$(SIMULATOR): $(OBJS)
	@echo + LD "->" $^
	@$(LD) -o $@ $^

clean:
	rm -rf $(BUILD)

.PHONY: clean all
