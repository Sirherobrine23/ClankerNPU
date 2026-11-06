# EN7523 NPU v1 system services

The four-hart boot prefix, MPIS windows and instruction set are documented in
`md32rv_bootup.md`, `md32rv_memory_space.md` and `md32rv_isa.md` in this folder.
Those specifications do not establish that the AN75xx peripheral drivers are
usable on EN7523. The controller, mutex and clock differences below must be
handled before enabling interrupts or any offload consumers.

## Reference inputs

The SDK reference is `airoha/airoha_sdk` revision
a6640f519b331479d8825bca3888f8c04d2f9021

The relevant image is `install_bsp/filesystem/userfs/npu_rv32.bin`, SHA256
`fc809ddb9a1c29cfd6a5d6926fb440652663adb975a3cf1279eb006ddf1b4368`. Its
0x108-byte boot prefix matches this EN7523 target. The binary function
addresses below are relative to its load address 0x84000000; they must not
be applied to unrelated firmware profiles. No vendor binary is embedded in
the new source implementation.

The host-side cross-checks are available in the attached SDK:

- `release_bsp/UNION_EN7523_GLIBC_7915_7915_ActiveEthWan_demo/modules/private/NPU/host/npu_rv32.c`:
  `CR_PLIC_IE_T0_S0_31` is 0xbec09098 and timer 0 is bit 18. This is the
  host's uncached alias of physical 0x1ec09098. Per-hart enable stride is 16.
- `linux/arch/arm/mach-econet/ecnt_npu.c`: EN7523 timer control preserves
  0x02000027; mailbox and timer physical bases match the table below.

## Register map

| Service                 | EN7523 register or formula                                  | Evidence in reference image |
| ----------------------- | ----------------------------------------------------------- | --------------------------- |
| Source priority         | 0x1ec09018 + (source / 4) * 4; seven bits per byte          | 0x3450, 0x3740              |
| Hart source enable      | 0x1ec09098 + hart * 16 + (source / 32) * 4                  | 0x34a4, 0x35d4              |
| Threshold               | 0x1ec09118 + hart * 4, mask 0x7f                            | 0x3374, 0x338c              |
| Active priority         | 0x1ec09138 + hart * 4; save/restore around handler          | 0x3304                      |
| Interrupt claim         | CSR 0x5c0, source already zero-based                        | 0x4330                      |
| Interrupt complete      | CSR 0x5c2, same source value                                | 0x3304                      |
| Local IRQ configuration | CSR 0xbc0=1, 0x5d8..0x5db and 0x5e8..0x5eb=~0               | 0x4394, 0x4464              |
| Global interrupt gate   | MCTREN bit 6; clear during vector/mask setup, set last      | 0x4464                      |
| Mutex status            | 0x1ec09800 + id * 4                                         | 0x5844                      |
| Mutex acquire           | 0x1ec09b00 + id * 4; write hart<<8 or 0x40                  | 0x5844                      |
| Mutex release           | 0x1ec09c00 + id * 4; write hart<<8                          | 0x58b8                      |
| Mutex IDs               | 0..63, offset mask 0xfc; status is global, not hart-strided | 0x5910, 0x5844              |
| Timer control/reload    | 0x1ec10100 / 0x1ec10104; source 18, W1C bit 16              | 0x39a0, 0x3aa8              |
| NPU PLL                 | 0x1fa201bc; selector bit 8, divider bits 2:0 plus one       | 0x39dc                      |
| PLL MHz table           | 500, 540; timer runs at NPU clock / 2                       | data 0xe7b0, 0x39fc         |
| Mailbox routing         | 0x1ec0c008 + hart*4 = 1<<hart, source 8+hart                | 0x40c8                      |

The image initializes 128 source priorities to 64, except source 96 at 65.
The platform port preserves those values and enables only sources 8..11 and 18. Priority and mode behavior are derived from the vendor image; they have
not yet been independently verified on silicon with this new runtime.

The source code uses standard RV32IM loads/stores and fences. No RVC or
AN75xx cache-control opcode is introduced. The original EN7523 boot prefix
sets MCTREN to 0x139; native IRQ initialization adds bit 6, producing 0x179.
D-cache bit 1 remains clear.

## Next datapath work

Starting the system services is distinct from completing the packet engine.
Do not enable WLAN/PPE capability bits until the corresponding handlers and
host contracts exist and have been tested. The remaining work includes:

1. EN7523 SRAM allocation and address aliases: the v1 SRAM-C base is
   0x1e900000; AN75xx uses 0x3e900000 in the firmware. Do not reuse the
   unconditional host-to-NPU SRAM conversion for v1 descriptors.
2. Native TDMA and host-adaptor ring layout, ownership, DMA addresses, IRQs
   and interaction with the existing Ethernet/QDMA setup.
3. The EN7523 MT7915 mailbox layout and reorder/buffer services. Its SDK
   command enums differ from the newer MT7916/MT799x command tables.
4. PPE command ABI, buffer lifetime and synchronization with the host
   driver's offload users. CPU PPE hardware acceleration is not evidence
   that the NPU firmware supports this interface.
5. Error recovery, watchdogs and warm restart. Current boot is cold-only;
   an active cluster must not be overwritten after a failed handshake.

The EN7523 NOWIFI image rejects unimplemented offload commands. Its current
SDK mailbox subset and kernel loading path are described in [runtime.md](runtime.md).
