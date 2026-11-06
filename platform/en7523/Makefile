CROSS ?= riscv64-unknown-elf-
CC := $(CROSS)gcc
AS := $(CROSS)as
LD := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
OBJDUMP := $(CROSS)objdump
BUILD ?= ../../build/EN7523_DIAG
ARCH := -march=rv32im_zicsr_zifencei -mabi=ilp32
CFLAGS := $(ARCH) -Os -ffreestanding -fno-builtin -fno-stack-protector \
	-fno-pic -fno-pie -fno-unwind-tables -fno-asynchronous-unwind-tables \
	-msmall-data-limit=0 -mno-relax -Wall -Wextra -Werror
.DEFAULT_GOAL := all
.PHONY: all clean disasm check
all: $(BUILD)/npu_rv32.bin $(BUILD)/npu_data.bin
$(BUILD):
	mkdir -p $@
$(BUILD)/start.o: start.S | $(BUILD)
	$(AS) $(ARCH) -o $@ $<
$(BUILD)/runtime.o: runtime.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD)/firmware.elf: $(BUILD)/start.o $(BUILD)/runtime.o link.ld
	$(LD) -m elf32lriscv -T link.ld --no-relax -Map=$(BUILD)/firmware.map -o $@ $(BUILD)/start.o $(BUILD)/runtime.o
$(BUILD)/npu_rv32.bin: $(BUILD)/firmware.elf
	$(OBJCOPY) -O binary -j .text -j .rodata $< $@
$(BUILD)/npu_data.bin: $(BUILD)/firmware.elf
	$(OBJCOPY) -O binary -j .data $< $@
disasm: all
	$(OBJDUMP) -d -M no-aliases $(BUILD)/firmware.elf > $(BUILD)/firmware.dis
check: all
	python3 verify.py $(BUILD)/npu_rv32.bin "$(REFERENCE)"
	python3 test-mailbox.py $(BUILD)/npu_rv32.bin
clean:
	rm -f $(BUILD)/start.o $(BUILD)/runtime.o $(BUILD)/firmware.elf $(BUILD)/firmware.map \
		$(BUILD)/firmware.dis $(BUILD)/npu_rv32.bin $(BUILD)/npu_data.bin
