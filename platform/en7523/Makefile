CROSS ?= riscv64-unknown-elf-
AS := $(CROSS)as
LD := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
OBJDUMP := $(CROSS)objdump
BUILD ?= ../../build/EN7523_DIAG
ARCH := -march=rv32im_zicsr_zifencei -mabi=ilp32
.DEFAULT_GOAL := all
.PHONY: all clean disasm check
all: $(BUILD)/npu_rv32.bin $(BUILD)/npu_data.bin
$(BUILD):
	mkdir -p $@
$(BUILD)/start.o: start.S | $(BUILD)
	$(AS) $(ARCH) -o $@ $<
$(BUILD)/firmware.elf: $(BUILD)/start.o link.ld
	$(LD) -m elf32lriscv -T link.ld --no-relax -Map=$(BUILD)/firmware.map -o $@ $<
$(BUILD)/npu_rv32.bin: $(BUILD)/firmware.elf
	$(OBJCOPY) -O binary -j .text -j .rodata $< $@
$(BUILD)/npu_data.bin: $(BUILD)/firmware.elf
	$(OBJCOPY) -O binary -j .data $< $@
disasm: all
	$(OBJDUMP) -d -M no-aliases $(BUILD)/firmware.elf > $(BUILD)/firmware.dis
check: all
	python3 verify.py $(BUILD)/npu_rv32.bin "$(REFERENCE)"
clean:
	rm -f $(BUILD)/start.o $(BUILD)/firmware.elf $(BUILD)/firmware.map \
		$(BUILD)/firmware.dis $(BUILD)/npu_rv32.bin $(BUILD)/npu_data.bin
