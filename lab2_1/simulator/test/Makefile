## 1. General Compilation Targets
BUILD  = build
TARGET = $(BUILD)/$(T).bin
$(shell mkdir -p $(BUILD)/objs)

## 2. General Compilation Flags
### Cross compiler
CROSS_COMPILE = riscv64-unknown-elf-
AS      = $(CROSS_COMPILE)gcc
CC      = $(CROSS_COMPILE)gcc
CXX     = $(CROSS_COMPILE)g++
LD      = $(CROSS_COMPILE)ld
AR      = $(CROSS_COMPILE)ar
OBJDUMP = $(CROSS_COMPILE)objdump
OBJCOPY = $(CROSS_COMPILE)objcopy
READELF = $(CROSS_COMPILE)readelf

### Compilation flags
CFLAGS   = -O2 -static -Wall -Werror -Wa,-march=rv64i -I./include \
           -fno-asynchronous-unwind-tables -fno-builtin -fno-stack-protector \
           -Wno-main -U_FORTIFY_SOURCE -fvisibility=hidden \
		   -fdata-sections -ffunction-sections \
		   -fno-pic -mcmodel=medany -mstrict-align \
		   -DMAINARGS=\"$(mainargs)\"
ASFLAGS  = -MMD $(INCFLAGS) -O0
LDFLAGS  = -z noexecstack -T scripts/linker.ld --gc-sections -e _start -melf64lriscv

## 3. Rules
all: $(TARGET)

$(BUILD)/objs/start.o: trm/start.S
	@echo + AS "->" $<
	@$(AS) $(ASFLAGS) -c -o $@ $<

$(BUILD)/objs/trm.o: trm/trm.c
	@echo + CC "->" $<
	@$(CC) -std=gnu11 $(CFLAGS) -c -o $@ $<

$(BUILD)/objs/%.o: src/%.c
	@echo + CC "->" $<
	@$(CC) -std=gnu11 $(CFLAGS) -c -o $@ $<

$(BUILD)/%.elf: $(addprefix $(BUILD)/objs/, %.o trm.o start.o)
	@echo + LD "->" $^
	@$(LD) $(LDFLAGS) $^ -o $@

$(BUILD)/%.bin: $(BUILD)/%.elf
	@echo + OBJDUMP "->" $(patsubst %.elf, %.txt, $<)
	@$(OBJDUMP) -d $< > $(patsubst %.elf, %.txt, $<)
	@echo + OBJCOPY "->" $@
	@$(OBJCOPY) -S --set-section-flags .bss=alloc,contents -O binary $< $@

clean:
	rm -rf $(BUILD)

.PHONY: clean all
.PRECIOUS: $(addprefix $(BUILD)/, objs/%.o %.elf)

