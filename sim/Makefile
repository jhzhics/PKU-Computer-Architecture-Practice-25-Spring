## 1. General Compilation Targets
BUILD = build
$(shell mkdir -p $(BUILD)/objs)
C_SRC   = $(wildcard src/*.c)
CPP_SRC = $(wildcard src/*.cpp)
OBJS    = $(addprefix $(BUILD)/objs/, $(notdir $(C_SRC:.c=.o) $(CPP_SRC:.cpp=.o)))
SIMULATOR = $(BUILD)/Simulator
CACHE_SIMULATOR = $(BUILD)/CacheSimulator
CHCHE_SIMULATOR3-2 = $(BUILD)/CacheSimulator3-2

## 2. General Compilation Flags
CC  = gcc
CXX = g++
LD  = $(CC)
CFLAGS  = -I./include -Wall -O0 -g
CXXFLAGS = $(CFLAGS)
LDFLAGS = -lstdc++

## 3. Rules
all: $(SIMULATOR) $(CACHE_SIMULATOR) $(CHCHE_SIMULATOR3-2)

$(BUILD)/objs/%.o: src/%.c
	@echo + CC "->" $<
	@$(CC) -std=gnu11 $(CFLAGS) -c -o $@ $<

$(BUILD)/objs/%.o: src/%.cpp
	@echo + CXX "->" $<
	@$(CXX) -std=c++17 $(CXXFLAGS) -c -o $@ $<

$(SIMULATOR): src/executable/sim.c $(OBJS) 
	@echo + CC "->" $^
	$(CC) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(CACHE_SIMULATOR): src/executable/cache_sim.cpp $(OBJS)
	@echo + CXX "->" $^
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(CHCHE_SIMULATOR3-2): src/executable/cache_sim_lab3-2.cpp $(OBJS)
	@echo + CXX "->" $^
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD)

.PHONY: clean all
