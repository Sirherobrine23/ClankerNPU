#!/usr/bin/env python3
"""Summarize decoded images and preserve unparsed byte ranges from LLVM."""
import collections,hashlib,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
INP=ROOT/'input/npu_bins';OUT=ROOT/'recovered'
info=json.loads((OUT/'inventory.json').read_text())
rows=[];limits=[]
for entry in info['images']:
    name=entry['name'];code=INP/f'{name}_npu_rv32.bin';raw=code.read_bytes()
    dat=(INP/f'{name}_npu_data.bin').read_bytes()
    lines=(OUT/f'{name}.annotated.S').read_text().splitlines()
    instr=[]
    for l in lines:
        m=re.match(r'^([0-9a-f]{8}): ([0-9a-f]{4,8}) ',l)
        if m:instr.append((int(m.group(1),16)-0x84000000,len(m.group(2))//2))
    gaps=[]; cursor=0
    for a,size in instr:
        if a>cursor:gaps.append((cursor,a))
        if a>=cursor:cursor=a+size
    if cursor<len(raw):gaps.append((cursor,len(raw)))
    # LLVM may interpret printable or vector constants as instructions; the
    # gap manifest is only where the disassembler returned no code line.
    bad=sum(b-a for a,b in gaps)
    txt=[f'# Unparsed byte ranges — {name}','','These bytes were not emitted as instruction lines by llvm-objdump. '
         'They could be data, unsupported opcodes, or alignment. No claim '
         'that they are executable code is made.','',
         '| File offset | Load address | Byte length | Exact bytes |',
         '|---|---|---:|---|']
    for a,b in gaps:
        sub=raw[a:b]
        hx=' '.join(f'{x:02x}' for x in sub[:128])
        if len(sub)>128:hx+=' ... [truncated here; raw firmware supplied in input/]'
        txt.append(f'| `0x{a:06x}` | `0x{0x84000000+a:08x}` | {b-a} | `{hx}` |')
    (OUT/f'{name}.unparsed.md').write_text('\n'.join(txt)+'\n')
    datsha=hashlib.sha256(dat).hexdigest()
    entry.update(data_bytes=len(dat),data_sha256=datsha,unparsed_bytes=bad,unparsed_regions=len(gaps))
    rows.append((name,entry['aliases'],len(raw),len(dat),entry['function_candidates'],entry['unparsed_regions'],entry['unparsed_bytes']))
    if name.startswith('en7523') or 'askey' in name:
        mtxt=(OUT/f'{name}.xref.md').read_text()
        def hit(reg):
            m=re.search(r'^\| MMIO '+re.escape(reg)+r' \S+ \| `(?P<pc>0x[0-9a-f]+)`',mtxt,re.M)
            return int(m.group('pc'),16) if m else None
        limits.append((name,hit('PPE_RETURN_COUNT'),hit('PPE_RETURN_DATA'),hit('PPE_RETURN_INFO'),hit('QDMA_TX_CPU')))
(OUT/'inventory.json').write_text(json.dumps(info,indent=2)+'\n')
readme=['# NPU vendor firmware — readable static recovery','','## What this package contains','',
 '- Original input binaries are retained in `input/npu_bins/`.',
 '- Fourteen unique code images have `recovered/*.annotated.S`: full llvm-objdump results with `md32rv` decoding for EN7523, candidate function labels, string and MMIO annotations.',
 '- `recovered/*.pseudo.c.txt` converts individual instructions into **C-like register-transfer statements**, retaining original instruction addresses and bytes. These are **not valid, functionally equivalent C sources**.',
 '- `recovered/*.functions.md` lists candidate function entrypoints and direct-call sites.',
 '- `recovered/*.xref.md` shows MMIO dereference sites and function pointers recovered from `npu_data.bin`.',
 '- `recovered/*.strings.md` contains printable ASCII runs and basic code references.',
 '- `recovered/*.data_map.md` shows initialized data words and candidate pointer classifications.',
 '- `recovered/*.unparsed.md` preserves exact ranges not emitted as LLVM instructions.',
 '- `reconstruction/en7523_7915_core_reconstructed.c` is a deliberately incomplete, human-readable model of PPE return and QDMA behavior, supported by the disassembly.',
 '- `tools/make_readable.py`, `tools/semantic_lift.py` and `tools/build_index.py` regenerate the outputs using LLVM and the embedded md32rv decoder.','',
 '## Validity, confidence and known limits','',
 '**No vendor C sources, original symbols, DWARF, ELF section layout, original types or reliable function boundaries were recovered**. Raw firmware can contain code mixed with inline data, alignment padding, strings and tables. Candidate functions come from direct call targets and firmware pointers; the disassembler also interprets some data as instructions. The `pseudo.c.txt` representation is an inspection aid, not compilable C. Unrecognized md32rv instructions and unknown dynamic jumps limit coverage. No firmware was executed in this analysis.',
 '',
 'LLVM is invoked with `rv32imac`-style attributes for all images, but **the md32rv custom opcode decoder is applied to EN7523 images only**. AN7552, AN7581 and AN7583 may use distinct vendor instruction variants that the present decoder does not support. Their outputs require separate silicon-level ISA verification.',
 '',
 '## Unique images (code SHA-256 deduplicated)','',
 '| Canonical code image | Alias names | RV32 bytes | DATA bytes | Candidate functions | LLVM-unparsed bytes |','|---|---|---:|---:|---:|---:|']
for name,aliases,rv,data,func,gap,bytes_ in rows:
    readme.append(f'| `{name}` | '+', '.join('`'+n+'`' for n in aliases)+f' | {rv} | {data} | {func} | {bytes_} ({gap} regions) |')
readme+=['','## EN7523 PPE and QDMA cross-variant entry-site evidence','',
         'Each address below marks an **MMIO read/write instruction**, not a recovered whole-function entry. '
         'The `PPE_RETURN_COUNT` call-site is a reliable search anchor to locate the polling function in each variant.','',
         '| Image | PPE_COUNT read | PPE_DATA access | PPE_INFO read | QDMA_TX_CPU write |',
         '|---|---|---|---|---|']
for name,count,data,info_reg,tx in limits:
    def fmt(v):return f'`0x{v:08x}`' if v is not None else 'not identified'
    readme.append(f'| `{name}` | {fmt(count)} | {fmt(data)} | {fmt(info_reg)} | {fmt(tx)} |')
readme+=['',
 'The `en7523_7592_7615` image did not yield the PPE_RETURN register signatures at these addresses. '
 'That absence **does not prove** the chip firmware lacks a PPE datapath, only that the given scan did not recover this pattern.',
 '',
 '## EN7523 MT7915: recovered high-confidence routines','',
 '| Entry | Proposed name | Basis |','|---|---|---|',
 '| `0x84000108` | boot entry | Per-hart startup after the shared 0x108-byte cache/MPIS prologue |',
 '| `0x84000174` | trap entry | Saves all GPRs, invokes trap callback, executes `mret` |',
 '| `0x84002ecc` | QDMA IRQ poll | IRQ word extraction and QDMA_IRQ_POP write |',
 '| `0x84003004` | PPE return poll | MMIO count/data/info, `md.extui` bit16, `md.bfextr16` reason, FIFO ACK |',
 '| `0x840030c0` | QDMA TX submit | Descriptor writes followed by `QDMA_TX_CPU` doorbell |',
 '| `0x84003180` | QDMA ring initialization | 1024-loop, 32-byte descriptor stride, 12-byte bookkeeping stride |',
 '',
 '### Comparison with ClankerNPU','',
 'See `reconstruction/EN7523_CLANKER_DIFF.md`. Do **not** copy the vendor `npu_data.bin` '
 'into a Clanker build: host ring structures, pointers, firmware memory maps and ABI are incompatible.',
 '',
 '### Reproducing the analysis','',
 'Requirements: Python 3, `llvm-objcopy`, `llvm-objdump` with RISC-V support. From the package root:',
 '',
 '```sh',
 'python3 tools/make_readable.py',
 'python3 tools/semantic_lift.py',
 'python3 tools/build_index.py',
 '```','',
 'The tools neither execute nor modify firmware images. The source archive is kept byte-for-byte in `input/npu_bins/`.']
(ROOT/'README.md').write_text('\n'.join(readme)+'\n')
print('README and unparsed region reports created')
