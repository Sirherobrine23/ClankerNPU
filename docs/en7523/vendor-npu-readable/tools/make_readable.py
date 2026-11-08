#!/usr/bin/env python3
"""Best-effort raw Airoha NPU binary analysis; no vendor source is implied.

Comments/documentation are in English.  EN7523 only is decoded as md32rv;
other SoC families use LLVM RISC-V decode without md32rv assumptions.
"""
import collections, csv, hashlib, importlib.util, json, re, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INPUT = ROOT/'input/npu_bins'
OUT = ROOT/'recovered'
OUT.mkdir(exist_ok=True)
MD = ROOT/'tools/md32rv-objdump.py'
spec=importlib.util.spec_from_file_location('md32_decode', MD)
md=importlib.util.module_from_spec(spec)
spec.loader.exec_module(md)

MMIO = {
    0x1fb50fdc: ('PPE_RETURN_INFO', 'PPE return metadata / reason / hash'),
    0x1fb50fe0: ('PPE_RETURN_DATA', 'PPE return packet ID, state and ACK'),
    0x1fb50fe4: ('PPE_RETURN_COUNT', 'PPE return FIFO pending count'),
    0x1fb541c0: ('QDMA_TX_BASE', 'QDMA TX descriptor base pointer'),
    0x1fb541c8: ('QDMA_TX_CPU', 'QDMA TX CPU enqueue doorbell'),
    0x1fb54064: ('QDMA_IRQ_CLEAR', 'QDMA IRQ completion acknowledgement'),
    0x1ec08000: ('NPU_BOOT_TRIGGER', 'NPU core reset release'),
    0x1ec08004: ('NPU_BOOT_CONFIG', 'NPU core enable mask'),
    0x1fbf0000: ('UART_BASE', 'Serial console peripheral'),
    0x1fa02000: ('WED0_BASE', 'Host WED control registers'),
    0x1fa03000: ('WED1_BASE', 'Host WED control registers'),
    0x1fa06000: ('WDMA0_BASE', 'Wi-Fi DMA block'),
    0x1fa06400: ('WDMA1_BASE', 'Wi-Fi DMA block'),
}
PRIMARY = {
    0x84000108: ('vendor_boot_entry','Per-hart boot entry, BSS initialization and startup handoff'),
    0x84000174: ('vendor_trap_entry','Trap register save, trap dispatch and mret'),
    0x8400029c: ('vendor_core_dispatch','Hart-specific startup selector'),
    0x84002ecc: ('vendor_qdma_irq_poll','QDMA IRQ ring poll (inferred from MMIO) '),
    0x84002e90: ('vendor_qdma_read_tx_base','Return configured TX descriptor base register'),
    0x84002e9c: ('vendor_qdma_write_tx_base','Write configured TX descriptor base register'),
    0x84003004: ('vendor_ppe_return_poll','PPE return FIFO polling and completion'),
    0x840030c0: ('vendor_qdma_tx_submit','Dequeue bookkeeping and submit TX descriptor'),
    0x84003180: ('vendor_qdma_tx_ring_init','Build 1024 descriptors and bookkeeping nodes'),
    0x84004330: ('vendor_trap_handler','Trap handling called from trap entry'),
    0x840004cc: ('vendor_process_ppe_cpu_return','PPE exception-path callback, internals incompletely recovered'),
    0x8400552c: ('vendor_complete_ppe_packet','Packet completion helper called from return FIFO'),
}
ADDR_RE=re.compile(r'^([0-9a-fA-F]{8}):\s+([0-9a-fA-F]{4,8})\s+(.*)$')
TARGET_RE=re.compile(r'\b(0x[0-9a-fA-F]{8})\b')
LINE_RE=re.compile(r'^(\S+)\s*(.*)$')


def u32(v):return v&0xffffffff

def sign(v,n):
    v &= (1<<n)-1
    return v-(1<<n) if v&(1<<(n-1)) else v


def strings(data,base=0x84000000):
    ret=[]
    for m in re.finditer(rb'[\x20-\x7e]{5,}',data):
        value=m.group().decode('ascii','replace')
        if not any(c.isalpha() for c in value):continue
        ret.append((base+m.start(),value))
    return ret


def disassemble(name, binary,custom):
    elf=OUT/(name+'.elf')
    subprocess.run(['llvm-objcopy','-I','binary','-O','elf32-littleriscv',
      '--rename-section=.data=.text,alloc,load,readonly,code',str(binary),str(elf)],
      check=True,stdout=subprocess.DEVNULL)
    proc=subprocess.run(['llvm-objdump','-d','-M','no-aliases','--mattr=+m,+a,+f,+c',
      '--adjust-vma=0x84000000',str(elf)], check=True,text=True,capture_output=True)
    records=[]
    for line in proc.stdout.splitlines():
        m=ADDR_RE.match(line.strip())
        if not m:continue
        addr=int(m.group(1),16); op=int(m.group(2),16); size=len(m.group(2))//2
        ins=m.group(3).strip()
        if size==4 and custom:
            dec=md.md32rv_decode(op,addr)
            if dec: ins=dec[0]
        records.append({'addr':addr,'op':op,'size':size,'asm':ins})
    elf.unlink(missing_ok=True)
    return records


def is_in_range(address,size):return 0x84000000<=address<0x84000000+size


def get_target(r):
    s=r['asm']; m=TARGET_RE.search(s)
    return int(m.group(1),16) if m else None


def mnem(r):return r['asm'].split()[0] if r['asm'] else ''


def direct_call(r):
    x=mnem(r)
    if x in ('c.jal',):return get_target(r)
    if x=='jal':
        # Treat ra and t0 as possible procedure-call link registers.
        if re.match(r'^jal\s+(ra|t0),',r['asm']):return get_target(r)
    return None


def crossrefs(records,data_size):
    calls=collections.defaultdict(list)
    for r in records:
        dst=direct_call(r)
        if dst and is_in_range(dst,data_size):calls[dst].append(r['addr'])
    return calls


def find_ref(records):
    # Conservative LUI + ADDI materialization only; no dynamic base tracking.
    by_addr={r['addr']:r for r in records}
    matches=collections.defaultdict(list)
    for idx,r in enumerate(records):
        if r['size']!=4 or r['op']&0x7f!=0x37:continue
        reg=(r['op']>>7)&31
        high=r['op']&0xfffff000
        for later in records[idx+1:idx+5]:
            if later['addr']-r['addr']>16:break
            w=later['op']
            if later['size']==4 and w&0x7f==0x13 and ((w>>12)&7)==0:
                rs1=(w>>15)&31; rd=(w>>7)&31
                if rs1==reg:
                    target=u32(high+sign(w>>20,12))
                    if target>=0x1e800000 and target<=0x1fbfffff or is_in_range(target,500000):
                        matches[later['addr']].append((target,r['addr']))
                    if rd==reg:break
    return matches


def recover_functions(recs, size, named):
    calls=crossrefs(recs,size)
    addrs={r['addr'] for r in recs}
    candidates={0x84000108}
    candidates.update(a for a in calls if a in addrs)
    candidates.update(a for a in named if a in addrs)
    candidates=sorted(candidates)
    func=[]
    for i,entry in enumerate(candidates):
        nm=named.get(entry,(f'sub_{entry:08x}', 'Unclassified call target'))
        func.append({'entry':entry,'label':nm[0],'description':nm[1],
                     'calls':len(calls.get(entry,[])),
                     'end_hint':candidates[i+1] if i+1<len(candidates) else 0x84000000+size})
    return func,calls


def annotate(name,binary,recs,custom):
    data=binary.read_bytes(); size=len(data)
    named=PRIMARY if name=='en7523_7915' else {0x84000108:('vendor_boot_entry','Boot entry')}
    funcs,calls=recover_functions(recs,size,named)
    symbols={f['entry']:f['label'] for f in funcs}
    by_addr={r['addr']:r for r in recs}
    refs=find_ref(recs)
    str_list=strings(data)
    str_by_start={a:t for a,t in str_list}
    string_marks=[]
    for where,ref_list in refs.items():
        for dest,lui_at in ref_list:
            match=next(((a,s) for a,s in str_list if a<=dest<a+len(s)),None)
            if match:
                string_marks.append((where,dest,match[0],match[1]))
    str_cross=collections.defaultdict(list)
    for where,dst,origin,s in string_marks:str_cross[origin].append(where)
    labels=set(symbols)
    for r in recs:
        addr=get_target(r)
        if addr and addr in by_addr:labels.add(addr)
    is_main='en7523' in name or 'askey' in name
    hdr=[f'# Automatically generated annotated assembly for {name}',
         '# This is a static decoding, not original source code.',
         '# Code and data are interleaved; reachability is not fully established.',
         '# md32rv custom mnemonics are enabled only for EN7523 files.',
         f'# Base VA: 0x84000000; raw size: {size} bytes; SHA256: {hashlib.sha256(data).hexdigest()}',
         '# Evidence marker: [MMIO], [STR], [CALL], [CUSTOM], [BRANCH].','']
    out=hdr[:]
    for r in recs:
        a=r['addr']; op=r['op']; s=r['asm']; comments=[]
        if a in symbols:
            comments.append('FUNCTION START / '+str(named.get(a,(None,'Call target'))[1]))
            out.extend(['', '# '+'='*76,f'{symbols[a]}:'])
        elif a in labels:
            out.append(f'.L_{a:08x}:')
        for dst,src in refs.get(a,[]):
            if dst in MMIO:comments.append(f'[MMIO] {MMIO[dst][0]} = 0x{dst:08x}')
        for where,dst,origin,txt in string_marks:
            if where==a:comments.append(f'[STR] 0x{dst:08x}: {txt[:105]!r}')
        dst=direct_call(r)
        if dst:
            comments.append('[CALL] '+symbols.get(dst,f'sub_{dst:08x}'))
        if s.startswith('md.'):comments.append('[CUSTOM] md32rv')
        out.append(f'{a:08x}: {op:0{r["size"]*2}x} {s:<74}'+ (' # '+'; '.join(comments) if comments else ''))
    p=OUT/(name+'.annotated.S')
    p.write_text('\n'.join(out)+'\n')
    # Function labels are candidates, not an authoritative full function boundary map.
    lines=[f'# Function index — {name}', '',
           'Entries are direct-call targets from static decoding. Interleaved data and '
           'tail calls can make the approximate boundaries unreliable. A named entry '
           'is verified only to the extent described in its annotation.', '',
           '| Entry | Proposed symbol | Direct calls | Approx. next entry |',
           '|---|---|---:|---|']
    for f in funcs:
        lines.append(f'| `0x{f["entry"]:08x}` | `{f["label"]}` | {f["calls"]} | `0x{f["end_hint"]:08x}` |')
    lines += ['','## Key MMIO references (LUI+ADDI sites)', '',
               'These are address-materialization sites, **not** proof of a direct MMIO access.','',
               '| Site | Materialized address | Register / annotation |','|---|---|---|']
    relevant=[]
    for at,targets in sorted(refs.items()):
        for dst,base_at in targets:
            if dst in MMIO:
                relevant.append((at,dst,MMIO[dst][0]));lines.append(f'| `0x{at:08x}` | `0x{dst:08x}` | {MMIO[dst][0]} |')
    if not relevant:lines.append('| — | — | No mapped address materialization |')
    (OUT/(name+'.functions.md')).write_text('\n'.join(lines)+'\n')
    strings_lines=[f'# ASCII strings and verified address references — {name}', '',
       'Printable runs may be false positives inside instructions. A reference is reported '
       'only if the LUI+ADDI sequence resolves into the run; addresses may still be '
       'part of data rather than code.', '',
       '| Address | Length | Xrefs | String (truncated) |', '|---|---:|---|---|']
    for a,s in str_list:
        if len(s)<7:continue
        xrefs=','.join(f'`0x{x:08x}`' for x in str_cross.get(a,[])[:8]) or '—'
        safe=s[:120].replace('|','\\|').replace('`','\\`').replace('\n',' ')
        strings_lines.append(f'| `0x{a:08x}` | {len(s)} | {xrefs} | `{safe}` |')
    (OUT/(name+'.strings.md')).write_text('\n'.join(strings_lines)+'\n')
    meta={'name':name,'bytes':size,'sha256':hashlib.sha256(data).hexdigest(),
      'decoded_instruction_slots':len(recs),'function_candidates':len(funcs),
      'matched_mmio_sites':len(relevant),'strings':len(str_list),'custom_isa_enabled':custom}
    return meta


def data_map(name,binary):
    b=binary.read_bytes()
    lines=[f'# Initialized NPU data — {name}', '',
      'Base address used for EN7523 SRAM-C is `0x1e900000`. This base is not '
      'assumed to be identical on AN7552/AN7581/AN7583.', '',
      'This file is a 32-bit little-endian word view, **not** a recovered C structure. '
      'Strings, raw floats, integers, pointers, and packed flags may overlap word boundaries.', '',
      '| Offset | 32-bit little-endian value | Heuristic class |','|---|---|---|']
    for off in range(0,len(b),4):
        word=int.from_bytes(b[off:off+4],'little')
        k='zero' if word==0 else ('firmware address' if 0x84000000<=word<0x85000000 else
           ('NPU SRAM/peripheral address' if 0x1e000000<=word<0x40000000 else 'scalar or packed data'))
        lines.append(f'| `0x{off:04x}` | `0x{word:08x}` | {k} |')
    (OUT/(name+'.data_map.md')).write_text('\n'.join(lines)+'\n')


def main():
    names={}
    for bin in sorted(INPUT.glob('*_npu_rv32.bin')):
        name=bin.name.removesuffix('_npu_rv32.bin')
        h=hashlib.sha256(bin.read_bytes()).hexdigest()
        names.setdefault(h,[]).append((name,bin))
    all_meta=[];aliases={}
    for group in sorted(names.values(), key=lambda g: (not g[0][0].startswith(('en7523','askey','old_askey')),g[0][0])):
        if any(n=='en7523_7915' for n,_ in group): canonical='en7523_7915'; binary=next(p for n,p in group if n==canonical)
        elif any(n=='en7523_npu' for n,_ in group): canonical='en7523'; binary=next(p for n,p in group if n==canonical)
        else:canonical,binary=group[0]
        custom=(canonical.startswith('en7523') or 'askey' in canonical)
        recs=disassemble(canonical,binary,custom)
        meta=annotate(canonical,binary,recs,custom)
        df=INPUT/(canonical+'_npu_data.bin')
        if df.exists():data_map(canonical,df)
        meta['aliases']=[n for n,_ in group]
        all_meta.append(meta)
        for name,path in group:aliases[name]=canonical
        print(f'{canonical}: instructions={len(recs)} candidate_functions={meta["function_candidates"]} MMIO={meta["matched_mmio_sites"]} strings={meta["strings"]} aliases={len(group)}',flush=True)
    (OUT/'inventory.json').write_text(json.dumps({'images':all_meta,'aliases':aliases},indent=2)+'\n')
    (OUT/'ALIASES.md').write_text('\n'.join(['# Identical firmware images','',
       'Entries in each row have identical SHA-256 of `npu_rv32.bin`. Data-file equality is separately checked in the manifest.','',
       '| Canonical image | Aliases | Size | Code SHA-256 |','|---|---|---:|---|']+
       [f'| `{x["name"]}` | '+', '.join('`'+n+'`' for n in x['aliases'])+f' | {x["bytes"]} | `{x["sha256"]}` |' for x in all_meta])+'\n')

if __name__=='__main__':main()
