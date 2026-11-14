CROSS ?= riscv64-unknown-elf-
CC=$(CROSS)gcc
LD=$(CROSS)ld
OBJCOPY=$(CROSS)objcopy
OBJDUMP=$(CROSS)objdump
ROOT := $(abspath ..)
BUILD ?= build
LAB_CUSTOM_OPS ?= 0
LAB_PROFILE ?= 0
ifneq ($(filter $(LAB_PROFILE),0 1),$(LAB_PROFILE))
$(error LAB_PROFILE must be 0 or 1)
endif
ifeq ($(LAB_CUSTOM_OPS),0)
ARCH := -march=rv32im_zicsr_zifencei -mabi=ilp32
else ifeq ($(LAB_CUSTOM_OPS),1)
ARCH := -march=rv32im_zicsr_zifencei_xen7523ops -mabi=ilp32
else
$(error LAB_CUSTOM_OPS must be 0 or 1)
endif
CFLAGS := $(ARCH) -Os -ffreestanding -fno-builtin -nostdlib -ffunction-sections -fdata-sections -fno-strict-aliasing -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable -Werror=implicit-function-declaration -I$(ROOT) -Isource -DEN7523 -DMT7915 -DLAB_EN7523 -DLAB_PROFILE=$(LAB_PROFILE) -DNPU_WIFI_NAME='"MT7915"' -DNPU_VERSION_MAJOR=7 -DNPU_VERSION_MINOR=13 -DNPU_VERSION_PATCH=24 -DNPU_GIT_REV='"$(GITREV)"'
COMMON := npu_globals.c npu_main.c npu_util.c npu_mutex.c npu_plic.c npu_timer.c npu_mbox.c npu_printf.c npu_ppe_en7523.c
LOCAL := npu_sram.c npu_dbg.c npu_wifi.c npu_wifi_bufid.c npu_wifi_ba.c npu_wifi_rx.c npu_wifi_init.c lab_buf.c lab_qdma.c lab_host.c lab_runtime.c lab_control.c lab_profile.c lab_rx_schedule.c
OBJS := $(addprefix $(BUILD)/,$(COMMON:.c=.o)) $(addprefix $(BUILD)/,$(LOCAL:.c=.o)) $(BUILD)/crt0.o
HEADERS := $(wildcard $(ROOT)/*.h source/*.h)
all: $(BUILD)/firmware.elf $(BUILD)/en7523_mt7915_npu_rv32.bin $(BUILD)/en7523_mt7915_npu_data.bin $(BUILD)/firmware.dis
$(BUILD):
	mkdir -p "$@"
$(addprefix $(BUILD)/,$(COMMON:.c=.o)): $(BUILD)/%.o: $(ROOT)/%.c $(HEADERS) $(BUILD)/.flags | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(addprefix $(BUILD)/,$(LOCAL:.c=.o)): $(BUILD)/%.o: source/%.c $(HEADERS) $(BUILD)/.flags | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/crt0.o: $(ROOT)/crt0.S $(HEADERS) $(BUILD)/.flags | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/link.ld: $(ROOT)/link.ld $(HEADERS) $(BUILD)/.flags | $(BUILD)
	$(CC) $(CFLAGS) -E -P -x c $< -o $@
$(BUILD)/firmware.elf: $(OBJS) $(BUILD)/link.ld
	$(LD) -m elf32lriscv -T $(BUILD)/link.ld --defsym=NPU_LOCAL_DATA_BASE=0x1e900000 --gc-sections --relax --print-memory-usage -Map=$(BUILD)/firmware.map -o $@ $(OBJS) $(shell $(CC) $(ARCH) -print-libgcc-file-name)
$(BUILD)/en7523_mt7915_npu_rv32.bin: $(BUILD)/firmware.elf
	$(OBJCOPY) -O binary -j .text -j .rodata $< $@
$(BUILD)/en7523_mt7915_npu_data.bin: $(BUILD)/firmware.elf
	$(OBJCOPY) -O binary -j .data $< $@
$(BUILD)/firmware.dis: $(BUILD)/firmware.elf Makefile
	$(OBJDUMP) -d -M no-aliases,xen7523ops $< > $@.tmp
	@awk '/^[[:space:]]*[[:xdigit:]]+:/ { if (length($$2)!=8 || ($$3 ~ /^\./ && !($$3==".insn" && $$2=="0005400f" && ($$1=="84000034:" || $$1=="8400003c:")))) bad=1 } END { exit bad }' $@.tmp || { echo 'Lab: unsupported/compressed instruction in linked firmware' >&2; rm -f $@.tmp; exit 1; }
ifeq ($(LAB_CUSTOM_OPS),0)
	@awk '/^[[:space:]]*[[:xdigit:]]+:/ && $$3 ~ /^en7523\./ && $$3!="en7523.cacheop" { bad=1 } END { exit bad }' $@.tmp || { echo 'Lab: custom scalar opcode in standard mode' >&2; rm -f $@.tmp; exit 1; }
endif
	mv $@.tmp $@
$(BUILD)/.flags: FORCE | $(BUILD)
	@printf '%s\n' '$(CC) $(CFLAGS)' > $@.tmp; cmp -s $@.tmp $@ && rm -f $@.tmp || mv -f $@.tmp $@
FORCE:
.PHONY: all FORCE
