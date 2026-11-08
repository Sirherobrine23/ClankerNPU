# NPU vendor firmware — readable static recovery

## What this package contains

- Original input binaries are retained in `input/npu_bins/`.
- Fourteen unique code images have `recovered/*.annotated.S`: full llvm-objdump results with `md32rv` decoding for EN7523, candidate function labels, string and MMIO annotations.
- `recovered/*.pseudo.c.txt` converts individual instructions into **C-like register-transfer statements**, retaining original instruction addresses and bytes. These are **not valid, functionally equivalent C sources**.
- `recovered/*.functions.md` lists candidate function entrypoints and direct-call sites.
- `recovered/*.xref.md` shows MMIO dereference sites and function pointers recovered from `npu_data.bin`.
- `recovered/*.strings.md` contains printable ASCII runs and basic code references.
- `recovered/*.data_map.md` shows initialized data words and candidate pointer classifications.
- `recovered/*.unparsed.md` preserves exact ranges not emitted as LLVM instructions.
- `reconstruction/en7523_7915_core_reconstructed.c` is a deliberately incomplete, human-readable model of PPE return and QDMA behavior, supported by the disassembly.
- `tools/make_readable.py`, `tools/semantic_lift.py` and `tools/build_index.py` regenerate the outputs using LLVM and the embedded md32rv decoder.

## Validity, confidence and known limits

**No vendor C sources, original symbols, DWARF, ELF section layout, original types or reliable function boundaries were recovered**. Raw firmware can contain code mixed with inline data, alignment padding, strings and tables. Candidate functions come from direct call targets and firmware pointers; the disassembler also interprets some data as instructions. The `pseudo.c.txt` representation is an inspection aid, not compilable C. Unrecognized md32rv instructions and unknown dynamic jumps limit coverage. No firmware was executed in this analysis.

LLVM is invoked with `rv32imac`-style attributes for all images, but **the md32rv custom opcode decoder is applied to EN7523 images only**. AN7552, AN7581 and AN7583 may use distinct vendor instruction variants that the present decoder does not support. Their outputs require separate silicon-level ISA verification.

## Unique images (code SHA-256 deduplicated)

| Canonical code image | Alias names | RV32 bytes | DATA bytes | Candidate functions | LLVM-unparsed bytes |
|---|---|---:|---:|---:|---:|
| `askey_rtf8225vw` | `askey_rtf8225vw` | 58952 | 712 | 228 | 622 (70 regions) |
| `en7523_7592_7615` | `en7523_7592_7615`, `en7523_7592_7615_tr181` | 45800 | 680 | 146 | 514 (42 regions) |
| `en7523_7915` | `en7523_7915`, `en7523_7916d_7623`, `en7523_xx230v`, `en7523_xx231v` | 60264 | 728 | 244 | 578 (62 regions) |
| `en7523_7916` | `en7523_7916`, `en7523_xx530v_v1` | 59048 | 728 | 237 | 582 (67 regions) |
| `en7523_ex530v` | `en7523_ex530v` | 61320 | 728 | 240 | 586 (66 regions) |
| `en7523_heimgard_cx30` | `en7523_heimgard_cx30`, `en7523` | 69384 | 760 | 279 | 690 (86 regions) |
| `old_askey_rtf8225vw` | `old_askey_rtf8225vw` | 59240 | 712 | 233 | 630 (72 regions) |
| `an7552_mt7916` | `an7552_mt7916` | 86716 | 2964 | 317 | 1574 (261 regions) |
| `an7552_mt7991` | `an7552_mt7991` | 85508 | 2948 | 287 | 1870 (279 regions) |
| `an7581_mt7916` | `an7581_mt7916` | 94044 | 2996 | 392 | 1712 (254 regions) |
| `an7581_mt7992` | `an7581_mt7992` | 102844 | 2996 | 417 | 1864 (288 regions) |
| `an7581_mt7996` | `an7581_mt7996` | 105684 | 2996 | 449 | 1876 (328 regions) |
| `an7583_mt7916` | `an7583_mt7916`, `an7583_mt7996` | 88252 | 3148 | 327 | 1648 (227 regions) |
| `an7583_mt7992` | `an7583_mt7992` | 92708 | 3132 | 316 | 1914 (302 regions) |

## EN7523 PPE and QDMA cross-variant entry-site evidence

Each address below marks an **MMIO read/write instruction**, not a recovered whole-function entry. The `PPE_RETURN_COUNT` call-site is a reliable search anchor to locate the polling function in each variant.

| Image | PPE_COUNT read | PPE_DATA access | PPE_INFO read | QDMA_TX_CPU write |
|---|---|---|---|---|
| `askey_rtf8225vw` | `0x84002c7c` | `0x84002ca4` | `0x84002cb2` | `0x84002db6` |
| `en7523_7592_7615` | not identified | not identified | not identified | not identified |
| `en7523_7915` | `0x8400301c` | `0x84003044` | `0x84003052` | `0x8400315a` |
| `en7523_7916` | `0x84002cc8` | `0x84002cf4` | `0x84002d02` | `0x84002e0a` |
| `en7523_ex530v` | `0x84002f6c` | `0x84002f94` | `0x84002fa2` | `0x840030aa` |
| `en7523_heimgard_cx30` | `0x840030f8` | `0x84003120` | `0x8400312e` | `0x84003332` |
| `old_askey_rtf8225vw` | `0x84002d0c` | `0x84002d34` | `0x84002d42` | `0x84002e46` |

The `en7523_7592_7615` image did not yield the PPE_RETURN register signatures at these addresses. That absence **does not prove** the chip firmware lacks a PPE datapath, only that the given scan did not recover this pattern.

## EN7523 MT7915: recovered high-confidence routines

| Entry | Proposed name | Basis |
|---|---|---|
| `0x84000108` | boot entry | Per-hart startup after the shared 0x108-byte cache/MPIS prologue |
| `0x84000174` | trap entry | Saves all GPRs, invokes trap callback, executes `mret` |
| `0x84002ecc` | QDMA IRQ poll | IRQ word extraction and QDMA_IRQ_POP write |
| `0x84003004` | PPE return poll | MMIO count/data/info, `md.extui` bit16, `md.bfextr16` reason, FIFO ACK |
| `0x840030c0` | QDMA TX submit | Descriptor writes followed by `QDMA_TX_CPU` doorbell |
| `0x84003180` | QDMA ring initialization | 1024-loop, 32-byte descriptor stride, 12-byte bookkeeping stride |

### Comparison with ClankerNPU

See `reconstruction/EN7523_CLANKER_DIFF.md`. Do **not** copy the vendor `npu_data.bin` into a Clanker build: host ring structures, pointers, firmware memory maps and ABI are incompatible.

### Reproducing the analysis

Requirements: Python 3, `llvm-objcopy`, `llvm-objdump` with RISC-V support. From the package root:

```sh
python3 tools/make_readable.py
python3 tools/semantic_lift.py
python3 tools/build_index.py
```

The tools neither execute nor modify firmware images. The source archive is kept byte-for-byte in `input/npu_bins/`.
