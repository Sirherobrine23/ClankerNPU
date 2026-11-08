#!/usr/bin/env python3
"""Reproducible EN7523 raw-firmware MD32RV static disassembly helper.

Uses LLVM for standard RISC-V and ClankerNPU's docs/en7523/md32rv-objdump.py
for vendor-specific opcodes. Binary is treated as raw code+rodata without
pretending to recover original source symbols.
"""
import argparse, collections, hashlib, importlib.util, os, re, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent
DOC = Path(os.environ.get('MD32RV_DECODER', str(ROOT / 'md32rv-objdump.py')))
if not DOC.is_file():
    DOC = Path('docs/en7523/md32rv-objdump.py')
if not DOC.is_file():
    raise SystemExit('md32rv decoder missing: set MD32RV_DECODER or copy docs/en7523/md32rv-objdump.py beside this script')
spec = importlib.util.spec_from_file_location('md32rv_objdump', DOC)
md32 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(md32)
RE = re.compile(r'^\s*([0-9a-f]{8}):\s+([0-9a-f]{4,8})\s+(.*)$')


def disassemble(src, dest):
    src = Path(src)
    dest = Path(dest)
    dest.parent.mkdir(parents=True, exist_ok=True)
    # LLVM's objcopy imports binary as .data; make it executable for llvm-objdump.
    elf = dest.with_suffix('.elf')
    subprocess.run(['llvm-objcopy', '-I', 'binary', '-O', 'elf32-littleriscv',
                    '--rename-section=.data=.text,alloc,load,readonly,code',
                    str(src), str(elf)], check=True)
    out = subprocess.check_output(['llvm-objdump', '-d', '-M', 'no-aliases',
        '--mattr=+m,+a,+f,+c', '--adjust-vma=0x84000000', str(elf)], text=True)
    lines=[]; instr=[]
    for line in out.splitlines():
        m=RE.match(line)
        if not m:
            if not line.startswith('/') and not line.startswith('Disassembly'):
                lines.append(line)
            continue
        addr=int(m.group(1),16); w=m.group(2); body=m.group(3)
        if len(w)==8:
            decoded=md32.md32rv_decode(int(w,16),addr)
            if decoded:body=decoded[0]
        lines.append(f'{addr:08x}: {w:8}  {body}')
        instr.append((addr,int(w,16),len(w)//2,body))
    dest.write_text('\n'.join(lines)+'\n')
    return instr


def nearby_peripheral_refs(instr, bases, max_print=16):
    ret = collections.defaultdict(list)
    for idx,(addr,w,n,body) in enumerate(instr):
        if n!=4 or (w&0x7f)!=0x37:continue # LUI
        imm=w & 0xfffff000
        if not (0x1e800000 <= imm < 0x40000000):continue
        if any(abs(imm-x)<0x10000 for x in bases):
            rd=(w>>7)&31
            lines=[]
            for x in instr[idx:idx+9]:
                if x[0] - addr > 40:break
                lines.append(f'{x[0]:08x} {x[3]}')
            ret[imm].append(' | '.join(lines))
    return ret


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('firmware',nargs='+')
    parser.add_argument('--out',default=str(ROOT/'disasm'))
    args=parser.parse_args()
    bases=[0x1fb50000,0x1fb54000,0x1fa00000,0x1fa06000,0x1ec00000,0x1e800000,0x1e900000,0x1fbf0000]
    summary=[]
    for f in args.firmware:
        path=Path(f); b=path.read_bytes()
        dis=Path(args.out)/(path.stem+'.dis')
        ins=disassemble(path,dis)
        refs=nearby_peripheral_refs(ins,bases)
        summary.append(f'## {path.name}\nSize: {len(b)} bytes; SHA-256: {hashlib.sha256(b).hexdigest()}\n'
          f'Disassembled instructions (including potential embedded data): {len(ins)}\n')
        for base,hits in sorted(refs.items()):
            summary.append(f'### LUI 0x{base:08x}: {len(hits)} sites')
            for line in hits[:30]:summary.append('  '+line)
        summary.append('')
        print(f'{path.name}: {len(b)} bytes -> {dis.name} ({len(ins)} instructions), {sum(len(v) for v in refs.values())} peripheral LUI sites',flush=True)
    (ROOT/'mmio_site_report.txt').write_text('\n'.join(summary)+'\n')

if __name__=='__main__': main()
