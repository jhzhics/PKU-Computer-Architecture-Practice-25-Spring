SIM = sim/build/Simulator
TARGET = test/build/$(T).bin
MODE ?= batch
MEM_TRACE ?= OFF

all:
	@echo "-------Build Simulator-------"
	@$(MAKE) -C sim
	@echo "-------Build Test-------"
	@$(MAKE) -C test T=$(T)
	@echo "-------Start Simulation-------"
ifeq ($(MEM_TRACE), ON)
	@$(SIM) --$(MODE) --mem-trace $(TARGET)
	@echo "make: use \"cat memtrace.out\" to check the memtrace."
else
	@$(SIM) --$(MODE) $(TARGET)
endif

clean:
	@$(MAKE) -C sim clean
	@$(MAKE) -C test clean
	rm -f memtrace.out

.PHONY: clean all
