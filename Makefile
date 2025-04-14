SIM = sim/build/Simulator
TARGET = test/build/$(T).bin
MODE ?= batch
PERF ?= OFF
MEM_TRACE ?= OFF
CACHE ?= OFF

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
	@if [ "$(PERF)" != "OFF" ] && [ "$(CACHE)" != "OFF" ]; then \
		$(SIM) --$(MODE) --perf $(PERF) --cache $(TARGET); \
	elif [ "$(PERF)" != "OFF" ]; then \
		$(SIM) --$(MODE) --perf $(PERF) $(TARGET); \
	elif [ "$(CACHE)" != "OFF" ]; then \
		$(SIM) --$(MODE) --cache $(TARGET); \
	else \
		$(SIM) --$(MODE) $(TARGET); \
	fi

endif

clean:
	@$(MAKE) -C sim clean
	@$(MAKE) -C test clean
	rm -f memtrace.out

.PHONY: clean all
