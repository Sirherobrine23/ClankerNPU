ifneq ($(SOC)_$(WIFI),EN7523_MT7915)
$(error FW_IMPL=wireless-lab only supports EN7523/MT7915)
endif
OUTPUT ?= firmwares
BUILD ?= build/$(SOC)_$(WIFI)/wireless-lab
LAB_CUSTOM_OPS ?= 0
LAB_PROFILE ?= 0
GITREV ?= $(shell git describe --always --dirty --abbrev=7 2>/dev/null || echo nogit)
LAB_CROSS := $(if $(findstring /,$(CROSS)),$(abspath $(CROSS)),$(CROSS))
LAB_FILES := en7523_mt7915_npu_rv32.bin en7523_mt7915_npu_data.bin
.DEFAULT_GOAL := all
.PHONY: all disasm clean lab-test
all:
	@echo "ClankerNPU: EN7523 Wireless Lab 7.13.24 custom_ops=$(LAB_CUSTOM_OPS)"
	+$(MAKE) -C wireless-lab CROSS="$(LAB_CROSS)" BUILD="$(abspath $(BUILD))" LAB_CUSTOM_OPS="$(LAB_CUSTOM_OPS)" LAB_PROFILE="$(LAB_PROFILE)" GITREV="$(GITREV)" all
	mkdir -p "$(OUTPUT)"
	@set -e; for name in $(LAB_FILES); do \
	 cp "$(abspath $(BUILD))/$$name" "$(OUTPUT)/$$name.tmp"; \
	 if cmp -s "$(OUTPUT)/$$name.tmp" "$(OUTPUT)/$$name"; then rm -f "$(OUTPUT)/$$name.tmp"; else mv -f "$(OUTPUT)/$$name.tmp" "$(OUTPUT)/$$name"; fi; \
	done
disasm: all
	@echo "Disassembly: $(abspath $(BUILD))/firmware.dis"
lab-test:
	sh wireless-lab/tests/run-tests.sh
clean:
	rm -rf "$(BUILD)"
	rm -f $(addprefix $(OUTPUT)/,$(LAB_FILES))
