# SPDX-License-Identifier: GPL-2.0-only

include $(TOPDIR)/rules.mk

PKG_NAME:=clanker-npu
PKG_VERSION:=7.13.0
PKG_RELEASE:=1

PKG_TOOLCHAIN_ARCH:=riscv64
PKG_TOOLCHAIN_TARGET:=riscv64-unknown-elf
PKG_TOOLCHAIN_CPU_TYPE:=rv32imac
PKG_TOOLCHAIN_ABI:=ilp32

CLANKERNPU_SOURCE_DIR:=$(abspath $(CURDIR)/..)
CLANKERNPU_SOURCES:=$(wildcard \
  $(CLANKERNPU_SOURCE_DIR)/*.c \
  $(CLANKERNPU_SOURCE_DIR)/*.h \
  $(CLANKERNPU_SOURCE_DIR)/*.S) \
  $(CLANKERNPU_SOURCE_DIR)/Makefile \
  $(CLANKERNPU_SOURCE_DIR)/link.ld

PKG_FILE_DEPENDS:=$(CLANKERNPU_SOURCES)

include $(INCLUDE_DIR)/package.mk

clankernpu_lower = $(shell printf '%s' '$(1)' | tr '[:upper:]' '[:lower:]')
clankernpu_package = airoha-$(subst _,-,$(call clankernpu_prefix,$(1)))-npu-firmware
clankernpu_prefix = $(patsubst %_nowifi,%,$(call clankernpu_lower,$(1)))

# OpenWrt invokes one build per VARIANT (e.g. EN7523_MT7915).
# Derive the make arguments directly to avoid missing per-variant lookups.
CLANKERNPU_SOC = $(word 1,$(subst _, ,$(BUILD_VARIANT)))
CLANKERNPU_WIFI = $(word 2,$(subst _, ,$(BUILD_VARIANT)))

FW_MAKE_FLAGS_DEFAULT:=MAILTRACE=0 NPUDBG=1 PROF=0 RXPROF=1 NPUTX=1 CLANKER=0
CLANKERNPU_STALLPROF ?= 0

define Package/clanker-npu/Default
  SECTION:=firmware
  CATEGORY:=Firmware
  SUBMENU:=Airoha NPU Firmware
  URL:=https://sirherobrine23.com.br/airoha/ClankerNPU
  DEPENDS:=@arm_v7||aarch64
  PKGARCH:=all
endef

define Build/Prepare
	[ -f "$(CLANKERNPU_SOURCE_DIR)/npu_main.c" ] || { \
		echo "ClankerNPU sources not found: $(CLANKERNPU_SOURCE_DIR)" >&2; \
		exit 1; \
	}
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) $(CLANKERNPU_SOURCES) $(PKG_BUILD_DIR)/
	$(Build/Patch)
endef

define Build/Configure
	rm -rf $(PKG_BUILD_DIR)/build $(PKG_BUILD_DIR)/firmware
endef

define Build/Clean
	rm -rf $(PKG_BUILD_DIR)/build $(PKG_BUILD_DIR)/firmware
endef

define Build/Compile
	@echo "ClankerNPU: variant=$(BUILD_VARIANT) soc=$(CLANKERNPU_SOC) wifi=$(CLANKERNPU_WIFI)"
	@echo "ClankerNPU: flags=$(or $(FW_MAKE_FLAGS_$(BUILD_VARIANT)),$(FW_MAKE_FLAGS_DEFAULT))"
	+$(MAKE) $(PKG_JOBS) -C $(PKG_BUILD_DIR) \
		CROSS="$(PKG_TOOLCHAIN_CROSS)" \
		OUTPUT="$(PKG_BUILD_DIR)/firmware" \
		GITREV="$(CLANKERNPU_GITREV)" \
		SOC="$(CLANKERNPU_SOC)" \
		WIFI="$(CLANKERNPU_WIFI)" \
		$(or $(FW_MAKE_FLAGS_$(BUILD_VARIANT)),$(FW_MAKE_FLAGS_DEFAULT)) \
		STALLPROF="$(CLANKERNPU_STALLPROF)" \
		all
endef

# Declarative firmware targets.
define Firmware/Default
  FW_SOC:=
  FW_WIFI:=NOWIFI
  FW_MAKE_FLAGS:=$(FW_MAKE_FLAGS_DEFAULT)
endef

define Firmware/en7523
  FW_SOC:=EN7523
  FW_WIFI:=NOWIFI MT7915 MT7916
  FW_MAKE_FLAGS=MAILTRACE=1 NPUDBG=1 PROF=0 RXPROF=1 NPUTX=1 CLANKER=0
endef
FIRMWARES += en7523

define Firmware/an7552
  FW_SOC:=AN7552
  FW_WIFI:=NOWIFI MT7916 MT7991 MT7993
endef
FIRMWARES += an7552

define Firmware/an7581
  FW_SOC:=AN7581
  FW_WIFI:=NOWIFI MT7916 MT7992
endef
FIRMWARES += an7581

define Firmware/an7583
  FW_SOC:=AN7583
  FW_WIFI:=NOWIFI MT7916 MT7992 MT7993 MT7996
endef
FIRMWARES += an7583

# Generate an independent OpenWrt package for each SoC/Wi-Fi variant.
define Firmware/Variant
  define Package/$(call clankernpu_package,$(1)_$(2))
    $$(call Package/clanker-npu/Default)
    TITLE:=ClankerNPU firmware for $(1) ($(2))
    VARIANT:=$(1)_$(2)
  endef

  define Package/$(call clankernpu_package,$(1)_$(2))/install
	$$(INSTALL_DIR) $$(1)/lib/firmware/airoha
	$$(INSTALL_DATA) \
		$$(PKG_BUILD_DIR)/firmware/$(call clankernpu_prefix,$(1)_$(2))_npu_data.bin \
		$$(PKG_BUILD_DIR)/firmware/$(call clankernpu_prefix,$(1)_$(2))_npu_rv32.bin \
		$$(1)/lib/firmware/airoha/
  endef

  $$(eval $$(call BuildPackage,$(call clankernpu_package,$(1)_$(2))))
endef

define Firmware/Build
  $(eval $(Firmware/Default))
  $(eval $(Firmware/$(1)))
  $(foreach wifi,$(FW_WIFI), \
    $(eval FW_MAKE_FLAGS_$(FW_SOC)_$(wifi):=$(FW_MAKE_FLAGS)) \
    $(eval $(call Firmware/Variant,$(FW_SOC),$(wifi))) \
  )
endef

$(foreach fw,$(FIRMWARES),$(call Firmware/Build,$(fw)))
