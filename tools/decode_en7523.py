#!/usr/bin/env python3
"""Decode raw EN7523 NPU image pairs into annotated, browsable RV32 listings.

Requires clang, ld.lld and llvm-objdump on PATH. No vendor ELF or headers.
Usage: python3 tools/decode_en7523.py /path/to/npu_bins output-directory

IMPORTANT: this is a disassembler and pointer/string decoder, NOT a C
source decompiler. <unknown> means a vendor-specific opcode that stock
LLVM cannot decode. The original symbol names were stripped.
"""

import argparse
from collections import defaultdict
import hashlib
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

BASE = 0x84000000
ANALYSIS_CUTOFF = b"timer_isr  Store Value"
ASCIIRUN = re.compile(rb"[\x20-\x7e]{7,}")
P_CALL = re.compile(r"^([0-9a-f]+):.*?\t(?:jal|j)\s+(?:(?:ra|t0|a0),\s*)?(0x[0-9a-f]+)", re.I)
P_LOADA = re.compile(r"^([0-9a-f]+):.*?\tlui\s+([a-z0-9]+),\s+(0x[0-9a-f]+)", re.I)
P_ADDI = re.compile(r"^([0-9a-f]+):.*?\taddi\s+([a-z0-9]+),\s+([a-z0-9]+),\s+(-?0x[0-9a-f]+)", re.I)


def cmd(args, cwd=None):
    proc = subprocess.run(args, cwd=cwd, capture_output=True, text=True)
    if proc.returncode:
        raise RuntimeError(f"{' '.join(map(str,args))}:\n{proc.stderr}")
    return proc.stdout


def strings_at(blob, first):
    out = []
    for match in ASCIIRUN.finditer(blob[first:]):
        ofs = first + match.start()
        value = match.group().decode('ascii', errors='replace')
        if len(value) > 120:
            value = value[:120] + '...'
        out.append((BASE + ofs, value))
    return out


def analyze(stem, code_file, code, data, dest):
    dest.mkdir(parents=True, exist_ok=True)
    cutoff = code.find(ANALYSIS_CUTOFF)
    if cutoff < 0:
        cutoff = int(len(code) * 0.8)
    cutoff &= ~1
    with tempfile.TemporaryDirectory(prefix='npu-dis-') as tmp:
        asm = Path(tmp, 'raw.S')
        asm.write_text('.section .text,"ax",@progbits\n.globl _start\n_start:\n.incbin "' + str(code_file).replace('"', '\\"') + '"\n')
        obj, elf = Path(tmp, 'raw.o'), Path(tmp, 'raw.elf')
        cmd(['clang', '--target=riscv32-unknown-elf', '-march=rv32imc_zicsr_zifencei',
             '-mabi=ilp32', '-c', str(asm), '-o', str(obj)])
        cmd(['ld.lld', '-m', 'elf32lriscv', '-Ttext=0x84000000', '-e', '_start',
             '-o', str(elf), str(obj)])
        listing = cmd(['llvm-objdump', '-d', '--no-show-raw-insn',
                       f'--stop-address={BASE+cutoff}', str(elf)])
    strings = strings_at(code, cutoff)
    stringmap = dict(strings)
    with (dest/'strings.tsv').open('w') as f:
        f.write('address\toffset\tASCII (single line)\n')
        for address, value in strings:
            f.write(f'0x{address:08x}\t0x{address-BASE:05x}\t{value!r}\n')
    with (dest/'data.tsv').open('w') as f:
        f.write('offset\tNPU address (EN7523)\tu32 LE\ttype\n')
        for off in range(0,len(data)-3,4):
            value = struct.unpack_from('<I',data,off)[0]
            if BASE <= value < BASE + len(code):
                typ = 'code/rodata pointer'
                if value in stringmap: typ += ' => ' + repr(stringmap[value])
            elif 0x1e800000 <= value <= 0x1ebfffff:
                typ = 'NPU local SRAM pointer'
            elif value == 0: typ = 'zero'
            else: typ = 'scalar / flag'
            f.write(f'0x{off:03x}\t0x{0x1e900000+off:08x}\t0x{value:08x}\t{typ}\n')
    # Find direct jump targets; multiple invokers are useful for naming funcs.
    calls = defaultdict(list)
    for ln in listing.splitlines():
        m = P_CALL.search(ln.strip())
        if m:
            pc,dst=int(m[1],16),int(m[2],16)
            if BASE<=dst<BASE+cutoff and pc!=dst:
                calls[dst].append(pc)
    with (dest/'calls.tsv').open('w') as f:
        f.write('target\tcallers (first 10)\tcount\n')
        for dst,sources in sorted(calls.items()):
            f.write(f'0x{dst:08x}\t{",".join(hex(x) for x in sources[:10])}\t{len(sources)}\n')
    # Use the disassembler's native listing and add a sidecar of recognized
    # absolute-address loads. RISC-V LUI + ADDI has a signed low 12 bits.
    absolute = []
    pending = {}
    for ln in listing.splitlines():
        s=ln.strip()
        m=P_LOADA.search(s)
        if m:
            pc,reg,val=int(m[1],16),m[2],int(m[3],16)
            pending[reg]=(pc,val<<12)
            continue
        m=P_ADDI.search(s)
        if m and m[2]==m[3] and m[2] in pending:
            addr,hi = pending.pop(m[2])
            if int(m[1],16)-addr <= 8:
                v= (hi + int(m[4],16)) & 0xffffffff
                if v>=0x10000000:
                    typ = 'MMIO' if 0x1f000000<=v<0x20000000 else (
                          'local-SRAM' if 0x1e800000<=v<0x1eb00000 else
                          'DRAM/code' if 0x84000000<=v<0x85000000 else 'address')
                    label = ''
                    for base,strvalue in strings:
                        if base == v:
                            label=' => '+repr(strvalue); break
                    absolute.append((addr,v,typ,label))
    with (dest/'absolute-addresses.tsv').open('w') as f:
        f.write('instruction\tresolved address\tregion\tstring (if exact start)\n')
        for pc,addr,typ,label in absolute:
            f.write(f'0x{pc:08x}\t0x{addr:08x}\t{typ}{label}\n')
    (dest/'code.dis').write_text(
        f'; RV32 little-endian at 0x{BASE:08x}, vendor file {stem}\n'
        '; No symbols available. Private instructions appear as <unknown>.\n'
        '; Listings stop before the ASCII string pool to avoid decoding strings as opcodes.\n'
        + listing)
    # Raw 32-bit instruction words for the vendor boot sequence; bytes
    # preserve custom opcodes that LLVM calls <unknown>.
    with (dest/'bootstrap-words.tsv').open('w') as f:
        f.write('PC\tRV32 instruction word\n')
        for off in range(0,min(len(code),0x110),4):
            f.write(f'0x{BASE+off:08x}\t0x{struct.unpack_from("<I",code,off)[0]:08x}\n')
    (dest/'README.md').write_text(
        f'# {stem}\n\n'
        f'Code image: {len(code)} bytes, data image: {len(data)} bytes.\n\n'
        f'**Bootstrap:** `0x{BASE:08x}`. The MT7915 reference\'s '
        f'trap entry was observed around `0x84000174`; this address should not be '
        f'assumed to hold for other variants.\n\n'
        f'ASCII strings begin around `0x{BASE+cutoff:08x}`; this is a '
        'heuristic to delimit the code listing, not an ELF section boundary.\n\n'
        'Files: `code.dis` (instructions), `bootstrap-words.tsv` (including vendor '
        'opcodes), `data.tsv` (decoded 32-bit fields), `strings.tsv`, '
        '`calls.tsv` (direct branch targets), and `absolute-addresses.tsv` '
        '(reconstructed LUI+ADDI addresses).\n\n'
        'Do not infer exact function prototypes or original source names from the listing.\n')
    return cutoff, len(strings), len(calls)


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('vendor_dir',type=Path)
    parser.add_argument('output_dir',type=Path)
    args=parser.parse_args()
    seen={}; summary=[]
    for code_file in sorted(args.vendor_dir.glob('*_rv32.bin')):
        stem=code_file.name[:-len('_rv32.bin')]
        data_file=args.vendor_dir/(stem+'_data.bin')
        if not data_file.is_file():
            raise SystemExit(f'Missing data for {code_file}')
        code,data=code_file.read_bytes(),data_file.read_bytes()
        digest=hashlib.sha256(code+data).hexdigest()
        if digest in seen:
            summary.append((stem,seen[digest],'alias'))
            continue
        seen[digest]=stem
        cutoff, nstrings,ncalls=analyze(stem,code_file,code,data,args.output_dir/stem)
        summary.append((stem,stem,f'{len(code)} code, {len(data)} data; {nstrings} strings, {ncalls} branch targets'))
        print(f'{stem}: {len(code)} code, {len(data)} data; disassembled through 0x{BASE+cutoff:08x}')
    (args.output_dir/'INDEX.md').write_text('# EN7523 vendor NPU images (deduplicated)\n\n'
        '| Input variant | Human-readable directory | Notes |\n|---|---|---|\n' +
        ''.join(f'| {n} | `{canon}/` | {note} |\n' for n,canon,note in summary) +
        '\nAll blobs are raw RISC-V images, not source/ELF with debug symbols.\n')
    print(f'{len(seen)} unique image pairs decoded, {len(summary)} variants indexed')
