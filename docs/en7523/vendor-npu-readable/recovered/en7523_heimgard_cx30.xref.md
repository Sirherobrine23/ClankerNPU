# Extended cross-references — en7523_heimgard_cx30

Address references are derived by local constant propagation. Reads/writes whose base address is dynamically computed may not be detected.

## Exact MMIO access sites

| Register | Instruction sites |
|---|---|
| MMIO NPU_BOOT_CONFIG 0x1ec08004 | `0x84006076`, `0x8400607e` |
| MMIO NPU_BOOT_TRIGGER 0x1ec08000 | `0x84006082` |
| MMIO PPE_RETURN_COUNT 0x1fb50fe4 | `0x840030f8` |
| MMIO PPE_RETURN_DATA 0x1fb50fe0 | `0x84003120`, `0x8400318e` |
| MMIO PPE_RETURN_INFO 0x1fb50fdc | `0x8400312e` |
| MMIO QDMA_IRQ_CLEAR 0x1fb54064 | `0x840030d8` |
| MMIO QDMA_TX_BASE 0x1fb541c0 | `0x84002f74`, `0x84002f80`, `0x84003372` |
| MMIO QDMA_TX_CPU 0x1fb541c8 | `0x84003332` |
| MMIO UART_BASE 0x1fbf0000 | `0x84006680`, `0x840066ca`, `0x840066f8`, `0x84006712`, `0x84006742`, `0x84006750`, `0x840067da`, `0x840067ea`, `0x840067f8`, `0x84006808`, `0x8400682e`, `0x8400683c` |

## Entrypoints referenced from `npu_data.bin`

| Firmware address | Data offsets | Direct callers |
|---|---|---:|
| `84007060` | `0x024` | 0 |
| `84007078` | `0x028` | 0 |
| `840070a8` | `0x02c` | 0 |
| `840070ac` | `0x030` | 0 |
| `840070b0` | `0x034` | 0 |
| `840070d8` | `0x038` | 0 |
| `840070e8` | `0x03c` | 0 |
| `84007100` | `0x040` | 0 |
| `84007110` | `0x044` | 0 |
| `84007134` | `0x048` | 0 |
| `84007144` | `0x04c` | 0 |
| `84007168` | `0x050` | 0 |
| `8400718c` | `0x054` | 0 |
| `84007190` | `0x058` | 0 |
| `840071b4` | `0x05c` | 0 |
| `840071d4` | `0x060` | 0 |
| `840071e4` | `0x068` | 0 |
| `840071f0` | `0x06c` | 0 |
| `84007204` | `0x070` | 0 |
| `84007320` | `0x074` | 0 |
| `84007348` | `0x078` | 0 |
| `84007370` | `0x080` | 0 |
| `84007398` | `0x084` | 0 |
| `840073c8` | `0x088` | 0 |
| `840073e8` | `0x08c` | 0 |
| `84007448` | `0x090` | 0 |
| `840074c8` | `0x094` | 0 |
| `840074f4` | `0x098` | 0 |
| `84007508` | `0x09c` | 0 |
| `8400fe00` | `0x2e0` | 0 |
| `84010be6` | `0x0a4` | 0 |
