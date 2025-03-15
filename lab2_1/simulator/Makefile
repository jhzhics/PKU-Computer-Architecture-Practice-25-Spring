SIM = sim/build/Simulator
TARGET = test/build/$(T).bin

all:
	@echo "-------Build Simulator-------"
	@$(MAKE) -C sim
	@echo "-------Build Test-------"
	@$(MAKE) -C test T=$(T)
	@echo "-------Start Simulation-------"
	@$(SIM) $(TARGET)

clean:
	@$(MAKE) -C sim clean
	@$(MAKE) -C test clean

.PHONY: clean all
