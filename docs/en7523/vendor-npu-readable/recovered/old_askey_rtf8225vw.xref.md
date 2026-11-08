# Extended cross-references — old_askey_rtf8225vw

Address references are derived by local constant propagation. Reads/writes whose base address is dynamically computed may not be detected.

## Exact MMIO access sites

| Register | Instruction sites |
|---|---|
| MMIO NPU_BOOT_CONFIG 0x1ec08004 | `0x84004072`, `0x84004086` |
| MMIO NPU_BOOT_TRIGGER 0x1ec08000 | `0x8400408a` |
| MMIO PPE_RETURN_COUNT 0x1fb50fe4 | `0x84002d0c` |
| MMIO PPE_RETURN_DATA 0x1fb50fe0 | `0x84002d34`, `0x84002d9e` |
| MMIO PPE_RETURN_INFO 0x1fb50fdc | `0x84002d42` |
| MMIO QDMA_IRQ_CLEAR 0x1fb54064 | `0x84002cec` |
| MMIO QDMA_TX_BASE 0x1fb541c0 | `0x84002b88`, `0x84002b94`, `0x84002e9a` |
| MMIO QDMA_TX_CPU 0x1fb541c8 | `0x84002e46` |
| MMIO UART_BASE 0x1fbf0000 | `0x840045c8`, `0x84004612`, `0x84004640`, `0x8400465a`, `0x8400468a`, `0x84004698`, `0x840046fa`, `0x8400470a`, `0x84004718`, `0x84004728`, `0x8400473e`, `0x8400474e` |

## Entrypoints referenced from `npu_data.bin`

| Firmware address | Data offsets | Direct callers |
|---|---|---:|
| `840059f8` | `0x00c` | 0 |
| `840059fc` | `0x010` | 0 |
| `84005a00` | `0x014` | 0 |
| `84005a04` | `0x01c` | 0 |
| `84005a18` | `0x020` | 0 |
| `84005a44` | `0x024` | 0 |
| `84005a48` | `0x028` | 0 |
| `84005a4c` | `0x02c` | 0 |
| `84005a70` | `0x030` | 0 |
| `84005a80` | `0x034` | 0 |
| `84005aa4` | `0x038` | 0 |
| `84005acc` | `0x03c` | 0 |
| `84005adc` | `0x040` | 0 |
| `84005afc` | `0x044` | 0 |
| `84005b48` | `0x048` | 0 |
| `84005b88` | `0x04c` | 0 |
| `84005b98` | `0x050` | 0 |
| `84005bbc` | `0x054` | 0 |
| `84005bcc` | `0x058` | 0 |
| `84005bdc` | `0x05c` | 0 |
| `84005c3c` | `0x064` | 0 |
| `84005c48` | `0x068` | 0 |
| `84005c5c` | `0x06c` | 0 |
| `84005ca0` | `0x070` | 0 |
| `84005cc8` | `0x074` | 0 |
| `8400e44e` | `0x07c` | 0 |
