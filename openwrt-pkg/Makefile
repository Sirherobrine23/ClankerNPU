# SPDX-License-Identifier: GPL-2.0-only

include $(TOPDIR)/rules.mk

PKG_NAME:=airoha-npu-firmware
PKG_VERSION:=0.1.0
PKG_RELEASE:=6

PKG_BUILD_PARALLEL:=1

CLANKERNPU_SOURCE_DIR?=$(abspath $(CURDIR)/..)
CLANKERNPU_CROSS?=riscv64-unknown-elf-
CLANKERNPU_GITREV?=$(shell git -C "$(CLANKERNPU_SOURCE_DIR)" describe --always --dirty --abbrev=7 2>/dev/null || echo $(PKG_VERSION))

CLANKERNPU_SOURCES:=$(wildcard \
  $(CLANKERNPU_SOURCE_DIR)/*.c \
  $(CLANKERNPU_SOURCE_DIR)/*.h \
  $(CLANKERNPU_SOURCE_DIR)/*.S) \
  $(CLANKERNPU_SOURCE_DIR)/Makefile \
  $(CLANKERNPU_SOURCE_DIR)/link.ld

PKG_FILE_DEPENDS:=$(CLANKERNPU_SOURCES)
PKG_CONFIG_DEPENDS:=CLANKERNPU_CROSS CLANKERNPU_GITREV

include $(INCLUDE_DIR)/package.mk

CLANKERNPU_VARIANTS:= \
  EN7523_NOWIFI EN7523_MT7915 \
  AN7552_NOWIFI AN7552_MT7916 AN7552_MT7991 AN7552_MT7993 \
  AN7581_NOWIFI AN7581_MT7916 AN7581_MT7992 \
  AN7583_NOWIFI AN7583_MT7916 AN7583_MT7992 AN7583_MT7993 AN7583_MT7996

clankernpu_lower=$(shell printf '%s' '$(1)' | tr '[:upper:]' '[:lower:]')
clankernpu_package=airoha-$(subst _,-,$(call clankernpu_lower,$(1)))-npu-firmware
clankernpu_prefix=$(patsubst %_nowifi,%,$(call clankernpu_lower,$(1)))

CLANKERNPU_SOC=$(word 1,$(subst _, ,$(BUILD_VARIANT)))
CLANKERNPU_WIFI=$(word 2,$(subst _, ,$(BUILD_VARIANT)))
CLANKERNPU_OUTPUT:=$(PKG_BUILD_DIR)/firmware

define Package/airoha-npu-firmware/Default
  SECTION:=firmware
  CATEGORY:=Firmware
  SUBMENU:=Airoha NPU Firmware
  URL:=https://sirherobrine23.com.br/airoha/ClankerNPU
  DEPENDS:=@arm_v7||aarch64
  PKGARCH:=all
endef

define BuildFirmwarePackage
  define Package/$(call clankernpu_package,$(1))
    $$(call Package/airoha-npu-firmware/Default)
    TITLE:=ClankerNPU firmware for $(1)
    VARIANT:=$(1)
  endef

  define Package/$(call clankernpu_package,$(1))/install
	$$(INSTALL_DIR) $$(1)/lib/firmware/airoha
	$$(INSTALL_DATA) \
		$$(CLANKERNPU_OUTPUT)/$(call clankernpu_prefix,$(1))_npu_data.bin \
		$$(CLANKERNPU_OUTPUT)/$(call clankernpu_prefix,$(1))_npu_rv32.bin \
		$$(1)/lib/firmware/airoha/
  endef

  $$(eval $$(call BuildPackage,$(call clankernpu_package,$(1))))
endef

define Build/Prepare
	[ -f "$(CLANKERNPU_SOURCE_DIR)/npu_main.c" ] || { \
		echo "Set CLANKERNPU_SOURCE_DIR to the ClankerNPU source tree" >&2; \
		exit 1; \
	}
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) $(CLANKERNPU_SOURCES) $(PKG_BUILD_DIR)/
	$(Build/Patch)
endef

define Build/Configure
	rm -rf $(PKG_BUILD_DIR)/build $(CLANKERNPU_OUTPUT)
endef

define Build/Compile
	+$(MAKE) $(PKG_JOBS) -C $(PKG_BUILD_DIR) \
		CROSS="$(CLANKERNPU_CROSS)" \
		SOC="$(CLANKERNPU_SOC)" WIFI="$(CLANKERNPU_WIFI)" \
		OUTPUT="$(CLANKERNPU_OUTPUT)" \
		GITREV="$(CLANKERNPU_GITREV)" \
		MAILTRACE=0 NPUDBG=1 PROF=0 NPUTX=1 CLANKER=0 \
		all
endef

$(eval $(call TestHostCommand,clankernpu-toolchain, \
	Install a bare-metal RISC-V GCC toolchain with RV32 ILP32 support or set CLANKERNPU_CROSS., \
	command -v $(CLANKERNPU_CROSS)gcc && \
	command -v $(CLANKERNPU_CROSS)ld && \
	command -v $(CLANKERNPU_CROSS)objcopy))

$(foreach variant,$(CLANKERNPU_VARIANTS),$(eval $(call BuildFirmwarePackage,$(variant))))
