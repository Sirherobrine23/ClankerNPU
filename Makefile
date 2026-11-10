CROSS   ?= riscv64-unknown-elf-
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
OBJDUMP := $(CROSS)objdump
SIZE    := $(CROSS)size

# SoC: EN7523, AN7552, AN7581, AN7583
SOC     ?= AN7583
# WiFi: MT7915, MT7991, MT7992, MT7993, MT7996, NOWIFI
WIFI    ?= $(if $(filter EN7523,$(SOC)),NOWIFI,MT7996)
# 1 logs every WiFi mailbox command, from inside the mailbox ISR
MAILTRACE ?= 0
# 0 stages host tx frames but never writes the WiFi tx ring
NPUTX ?= 1
# 1 starts with the stats and WiFi print bits of the debug block set
NPUDBG ?= 0
# 1 builds the profiler: section timers and PC sampling (docs/debug.md)
PROF ?= 0
# 1 profiles native MT7915 RX at 1/64 frames and active DMA batches.
RXPROF ?= 0
# 1 builds for the Clanker host drivers, this changes behaviour and might break with stock driver
CLANKER ?= 0
# git short hash in the boot version line; pin it to compare images
GITREV ?= $(shell git describe --always --dirty --abbrev=7 2>/dev/null || echo nogit)

NPU_VERSION_MAJOR  := 7
NPU_VERSION_MINOR  := $(if $(filter EN7523_MT7915 EN7523_MT7916,$(SOC)_$(WIFI)),14,13)
NPU_VERSION_REV    := $(if $(filter EN7523_MT7915 EN7523_MT7916,$(SOC)_$(WIFI)),1,0)
NPU_VERSION_STABLE := 0

ifeq ($(SOC),EN7523)
ifeq ($(filter $(WIFI),NOWIFI MT7915 MT7916),)
$(error EN7523 supports NOWIFI, MT7915 or MT7916)
endif
ARCH := -march=rv32im_zicsr_zifencei -mabi=ilp32
else
ARCH := -march=rv32imc_zicsr_zifencei -mabi=ilp32
endif
CFLAGS  := $(ARCH) -Os -ffunction-sections -fdata-sections \
           -fno-builtin -ffreestanding -nostdlib \
           -Wall -Wno-unused-function \
           -D$(SOC) -D$(WIFI) -DNPU_WIFI_NAME='"$(WIFI)"' \
           -DNPU_GIT_REV='"$(GITREV)"' \
           -DNPU_VERSION_MAJOR='$(NPU_VERSION_MAJOR)' -DNPU_VERSION_MINOR='$(NPU_VERSION_MINOR)' \
           -DNPU_VERSION_PATCH='$(NPU_VERSION_REV)' -DNPU_VERSION_BUILD='$(NPU_VERSION_STABLE)' \
           $(if $(filter 1,$(MAILTRACE)),-DNPU_MAIL_TRACE) \
           $(if $(filter 0,$(NPUTX)),-DEAGLE_NO_TX_PUSH) \
           $(if $(filter 1,$(NPUDBG)),-DNPU_DATAPATH_DBG) \
           $(if $(filter 1,$(PROF)),-DNPU_PROFILE) \
           $(if $(filter EN7523_MT7915_1,$(SOC)_$(WIFI)_$(RXPROF)),-DEN7523_RX_PROFILE) \
           $(if $(filter 1,$(CLANKER)),-DUSE_CLANKER_DRIVER)
ASFLAGS := $(ARCH) -D$(SOC) -D$(WIFI)
LIBGCC  := $(shell $(CC) $(ARCH) -print-libgcc-file-name)

BUILD   := build/$(SOC)_$(WIFI)
OUTPUT  ?= build

LINK_SCRIPT := $(BUILD)/link.ld
LDFLAGS := -m elf32lriscv -T $(LINK_SCRIPT) -nostdlib --gc-sections --relax \
           --print-memory-usage
ELF     := $(BUILD)/firmware.elf
MAP     := $(BUILD)/firmware.map
DIS     := $(BUILD)/firmware.dis

BIN     := $(OUTPUT)/$(shell echo $(SOC) | tr '[:upper:]' '[:lower:]')$(if $(filter NOWIFI,$(WIFI)),,_$(shell echo $(WIFI) | tr '[:upper:]' '[:lower:]'))_npu_rv32.bin
DATA    := $(OUTPUT)/$(shell echo $(SOC) | tr '[:upper:]' '[:lower:]')$(if $(filter NOWIFI,$(WIFI)),,_$(shell echo $(WIFI) | tr '[:upper:]' '[:lower:]'))_npu_data.bin

SRCS_S  := crt0.S
# Keep npu_globals.c first among C objects to preserve the data tables.
# EN7523 also has a cold-boot barrier word in crt0.S.
SRCS_C  := npu_globals.c \
           npu_main.c npu_util.c npu_mutex.c npu_plic.c npu_timer.c \
           npu_mbox.c npu_sram.c npu_bridge.c npu_printf.c \
           npu_dba.c npu_tr471.c npu_dbg.c \
           npu_wifi.c npu_wifi_bufid.c npu_wifi_fwd.c npu_wifi_ba.c \
           npu_wifi_rx.c npu_wifi_init.c \
           npu_qdma_en7523.c npu_rx_en7523.c npu_wifi_kite.c npu_wifi_eagle.c npu_wifi_eagle_dp.c \
           npu_tdma.c npu_hostadpt.c npu_sta_q.c \
           npu_tunnel.c npu_ppe.c npu_l4s.c
OBJS    := $(patsubst %.S,$(BUILD)/%.o,$(SRCS_S)) \
           $(patsubst %.c,$(BUILD)/%.o,$(SRCS_C))
FLAGS   := $(BUILD)/.flags

.PHONY: all clean disasm check

all: $(BIN) $(DATA)

$(BUILD): $(OUTPUT)
	mkdir -p $(BUILD)

$(OUTPUT):
	mkdir -p $(OUTPUT)

# rebuild when the build flags change, not just the sources
$(BUILD)/.flags: FORCE | $(BUILD)
	@echo '$(CFLAGS)' > $@.tmp; cmp -s $@.tmp $@ || mv $@.tmp $@; rm -f $@.tmp
FORCE:

$(BUILD)/%.o: %.S npu_config.h $(FLAGS) | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c -o $@ $<

$(BUILD)/%.o: %.c npu_config.h npu_regs.h npu_types.h npu_internal.h \
		 npu_wifi.h npu_wlan.h $(FLAGS) | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(LINK_SCRIPT): link.ld $(FLAGS) | $(BUILD)
	$(CC) -E -P -x c -D$(SOC) -o $@ $<

$(ELF): $(OBJS) $(LINK_SCRIPT)
	$(LD) $(LDFLAGS) -Map=$(MAP) -o $@ $(OBJS) $(LIBGCC)

$(BIN): $(ELF)
	$(OBJCOPY) -O binary -j .text -j .rodata $< $@

$(DATA): $(ELF)
	$(OBJCOPY) -O binary -j .data $< $@

disasm: $(ELF)
	$(OBJDUMP) -d -M no-aliases $< > $(DIS)

clean:
	rm -rf build/

# Build the supported variant matrix
VARIANTS := EN7523_NOWIFI EN7523_MT7915 EN7523_MT7916                                                                       \
            AN7552_NOWIFI AN7552_MT7916 AN7552_MT7991               AN7552_MT7993               \
            AN7581_NOWIFI AN7581_MT7916               AN7581_MT7992                             \
            AN7583_NOWIFI AN7583_MT7916               AN7583_MT7992 AN7583_MT7993 AN7583_MT7996

.PHONY: all-variants $(VARIANTS)

all-variants: $(VARIANTS)

$(VARIANTS):
	$(MAKE) SOC=$(word 1,$(subst _, ,$@)) WIFI=$(word 2,$(subst _, ,$@))
