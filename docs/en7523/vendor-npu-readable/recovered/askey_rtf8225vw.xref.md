# Extended cross-references — askey_rtf8225vw

Address references are derived by local constant propagation. Reads/writes whose base address is dynamically computed may not be detected.

## Exact MMIO access sites

| Register | Instruction sites |
|---|---|
| MMIO NPU_BOOT_CONFIG 0x1ec08004 | `0x84003fe2`, `0x84003ff6` |
| MMIO NPU_BOOT_TRIGGER 0x1ec08000 | `0x84003ffa` |
| MMIO PPE_RETURN_COUNT 0x1fb50fe4 | `0x84002c7c` |
| MMIO PPE_RETURN_DATA 0x1fb50fe0 | `0x84002ca4`, `0x84002d0e` |
| MMIO PPE_RETURN_INFO 0x1fb50fdc | `0x84002cb2` |
| MMIO QDMA_IRQ_CLEAR 0x1fb54064 | `0x84002c5c` |
| MMIO QDMA_TX_BASE 0x1fb541c0 | `0x84002af8`, `0x84002b04`, `0x84002e0a` |
| MMIO QDMA_TX_CPU 0x1fb541c8 | `0x84002db6` |
| MMIO UART_BASE 0x1fbf0000 | `0x84004538`, `0x84004582`, `0x840045b0`, `0x840045ca`, `0x840045fa`, `0x84004608`, `0x8400466a`, `0x8400467a`, `0x84004688`, `0x84004698`, `0x840046ae`, `0x840046be` |

## Entrypoints referenced from `npu_data.bin`

| Firmware address | Data offsets | Direct callers |
|---|---|---:|
| `84005968` | `0x00c` | 0 |
| `8400596c` | `0x010` | 0 |
| `84005970` | `0x014` | 0 |
| `84005974` | `0x01c` | 0 |
| `84005988` | `0x020` | 0 |
| `840059b4` | `0x024` | 0 |
| `840059b8` | `0x028` | 0 |
| `840059bc` | `0x02c` | 0 |
| `840059e0` | `0x030` | 0 |
| `840059f0` | `0x034` | 0 |
| `84005a14` | `0x038` | 0 |
| `84005a3c` | `0x03c` | 0 |
| `84005a4c` | `0x040` | 0 |
| `84005a6c` | `0x044` | 0 |
| `84005ab8` | `0x048` | 0 |
| `84005af8` | `0x04c` | 0 |
| `84005b08` | `0x050` | 0 |
| `84005b2c` | `0x054` | 0 |
| `84005b3c` | `0x058` | 0 |
| `84005b4c` | `0x05c` | 0 |
| `84005bac` | `0x064` | 0 |
| `84005bb8` | `0x068` | 0 |
| `84005bcc` | `0x06c` | 0 |
| `84005c10` | `0x070` | 0 |
| `84005c38` | `0x074` | 0 |
| `8400da82` | `0x2b4` | 0 |
| `8400e31e` | `0x07c` | 0 |
