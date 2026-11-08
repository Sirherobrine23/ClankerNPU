#!/usr/bin/env python3
"""Add C-like register-transfer listings, MMIO and data-pointer xrefs.

This is a lossless-to-assembly *presentation* aid, not a C decompiler.
"""
import collections, importlib.util, json, re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('source',ROOT/'tools/make_readable.py')
src=importlib.util.module_from_spec(spec)
spec.loader.exec_module(src)
OUT=ROOT/'recovered'
RE_LINE=re.compile(r'^([0-9a-f]{8}):\s+([0-9a-f]{4,8})\s+(\S.*?)(?:\s+#\s+(.*))?$',re.I)
MEM=re.compile(r'^([a-z0-9.]+)\s+([a-z][0-9a-z]*),\s*([-+]?0x[0-9a-f]+|[-+]?\d+)\(([a-z][0-9a-z]*)\)',re.I)
ASMI=re.compile(r'^(\S+)\s*(.*)$')
LOADS=['lw','lh','lb','lhu','lbu','c.lw','c.lwsp','md.lw','md.lh','md.lb','md.lhu','md.lbu']
STORES=['sw','sh','sb','c.sw','c.swsp','md.sw','md.sh','md.sb']
REGS='zero ra sp gp tp t0 t1 t2 s0 s1 a0 a1 a2 a3 a4 a5 a6 a7 s2 s3 s4 s5 s6 s7 s8 s9 s10 s11 t3 t4 t5 t6'.split()
KNOWN={r for r in REGS}


def num(t):return int(t.strip().replace('+',''),0)

def clean(s):return s.split(' <')[0].strip()

def split_parts(ins):
    m=ASMI.match(ins)
    if not m:return '',[]
    return m.group(1),[clean(x) for x in m.group(2).split(',') if x.strip()]


def pseudo(m,args):
    # Conservative register-transfer operations, retaining unknowns as assembly.
    a=args
    try:
        if m in ('lui','c.lui') and len(a)==2:return f'{a[0]} = {a[1]} << 12;'
        if m in ('addi','c.addi'):
            if len(a)==3:return f'{a[0]} = {a[1]} + ({a[2]});'
            if len(a)==2:return f'{a[0]} += ({a[1]});'
        if m in ('ori','andi','xori','slli','srli','srai') and len(a)==3:
            sym={'ori':'|','andi':'&','xori':'^','slli':'<<','srli':'>>','srai':'>>'}[m]
            return f'{a[0]} = {a[1]} {sym} {a[2]};'
        if m in ('c.li','li') and len(a)==2:return f'{a[0]} = {a[1]};'
        if m in ('c.mv','mv') and len(a)==2:return f'{a[0]} = {a[1]};'
        if m in ('add','sub','mul','and','or','xor','sll','srl','sra') and len(a)==3:
            sym={'add':'+','sub':'-','mul':'*','and':'&','or':'|','xor':'^','sll':'<<','srl':'>>','sra':'>>'}[m]
            return f'{a[0]} = {a[1]} {sym} {a[2]};'
        if m in ('c.add','c.and','c.or','c.xor','c.sub','c.mul') and len(a)==2:
            sym={'c.add':'+','c.and':'&','c.or':'|','c.xor':'^','c.sub':'-','c.mul':'*'}[m]
            return f'{a[0]} {sym}= {a[1]};'
        if m in ('md.extui','md.extb') and len(a)==3:
            return f'{a[0]} = ({a[1]} >> ({a[2]})) & 1;'
        if m.startswith('md.bfextr') and len(a)==3:
            shift=int(m[len('md.bfextr'):]);return f'{a[0]} = ({a[1]} >> {shift}) & ((1u << {a[2]}) - 1);'
        if m in ('md.sh1add','md.sh2add') and len(a)==3:
            sh={'md.sh1add':1,'md.sh2add':2}[m];return f'{a[0]} = {a[1]} + ({a[2]} << {sh});'
        if m in ('md.maskl',) and len(a)==3:
            return f'{a[0]} = {a[1]} & ((1u << {a[2]}) - 1);'
        if m in ('md.zext.h',) and len(a)==2:return f'{a[0]} = {a[1]} & 0xffff;'
        if m in ('md.sext',) and len(a)==3:return f'{a[0]} = sign_extend({a[1]}, {a[2]});'
        if m in ('md.clri','md.setbi','md.togbi') and len(a)==3:
            op={'md.clri':'&= ~','md.setbi':'|= ','md.togbi':'^= '}[m]
            return f'{a[0]} = {a[1]}; {a[0]} {op}(1u << {a[2]});'
        if m in ('md.clrb','md.setb','md.setb3') and len(a)==3:
            op={'md.clrb':'& ~','md.setb':'|','md.setb3':'|'}[m]
            return f'{a[0]} = {a[1]} {op} (1u << ({a[2]} & 31));'
        if m in ('c.beqz','c.bnez') and len(a)==2:
            return f'if ({a[0]} {"==" if m=="c.beqz" else "!="} 0) goto loc_{a[1]};'
        if m in ('beq','bne','bltu','bgeu','blt','bge','bltu') and len(a)==3:
            sym={'beq':'==','bne':'!=','bltu':'<','blt':'<','bgeu':'>=','bge':'>='}[m]
            return f'if ({a[0]} {sym} {a[1]}) goto loc_{a[2]};'
        if m.startswith('md.b') and m in ('md.beqi','md.bnei','md.blti','md.bgei','md.bltui','md.bgeui') and len(a)==3:
            sym={'md.beqi':'==','md.bnei':'!=','md.blti':'<','md.bgei':'>=', 'md.bltui':'<','md.bgeui':'>='}[m]
            return f'if ({a[0]} {sym} {a[1]}) goto loc_{a[2]};'
        if m in ('c.j','j') and len(a)==1:return f'goto loc_{a[0]};'
        if m=='jal' and len(a)==2:
            if a[0]=='zero':return f'goto loc_{a[1]}; /* tail jump */'
            return f'CALL_{a[1]}(); /* ABI: a0..a7, rd={a[0]} */'
        if m=='c.jal' and len(a)==1:return f'CALL_{a[0]}(); /* ABI: a0..a7 */'
        if m in ('c.jr','ret') and (not a or a==['ra']):return 'return;'
        if m=='mret':return 'MRET();'
        mm=MEM.match(m+' '+', '.join(a))
        if mm:
            op,reg,off,base=mm.groups()
            width='8' if op.endswith('b') else ('16' if op.endswith('h') else '32')
            if op in LOADS:return f'{reg} = READ{width}({base} + ({off}));'
            if op in STORES:return f'WRITE{width}({base} + ({off}), {reg});'
    except (IndexError, ValueError):pass
    return f'ASM({m} {", ".join(a)});'


def special_xref(binary,lines):
    # Infer constants in straight-line LUI/ADDI/c.addi sequences.
    known={}; notes={}; last_addr=None
    b=binary.read_bytes(); ss=src.strings(b)
    for addr,orig,comment in lines:
        m,args=split_parts(orig)
        if addr in src.PRIMARY and binary.name.startswith('en7523_7915'):
            known={}
        if m=='lui' and len(args)==2 and args[0] in KNOWN:
            try:known[args[0]]=num(args[1])<<12
            except ValueError:known.pop(args[0],None)
        elif m=='c.mv' and len(args)==2 and args[0] in KNOWN:
            if args[1] in known:known[args[0]]=known[args[1]]
            else:known.pop(args[0],None)
        elif m in ('addi','c.addi') and len(args) in (2,3):
            rd=args[0]; bas=args[1] if len(args)==3 else rd
            imm=args[2] if len(args)==3 else args[1]
            if bas in known:
                try:known[rd]=src.u32(known[bas]+num(imm))
                except ValueError:known.pop(rd,None)
            else: known.pop(rd,None)
        elif m in ('lw','c.lw','sw','c.sw','lhu','lh','sb','sh','lb','lbu') and args:
            mem=MEM.match(m+' '+', '.join(args))
            if mem:
                _,dest,off,base=mem.groups()
                if base in known:
                    try:effective=src.u32(known[base]+num(off))
                    except ValueError:effective=None
                    if effective in src.MMIO:
                        notes.setdefault(addr,[]).append(f'MMIO {src.MMIO[effective][0]} 0x{effective:08x}')
                if m in LOADS:known.pop(dest,None)
        elif m in ('jal','c.jal','jalr','c.jalr'):
            # Caller-saved registers cannot be propagated across a call.
            for x in ('ra','a0','a1','a2','a3','a4','a5','a6','a7','t0','t1','t2','t3','t4','t5','t6'):
                known.pop(x,None)
        # Annotate any static address stored by the known register.
        for reg,val in known.items():
            if val in src.MMIO and m in ('addi','c.addi','lui') and reg==args[0]:
                notes.setdefault(addr,[]).append(f'ADDR {src.MMIO[val][0]}')
            if 0x84000000<=val<0x84000000+len(b) and m in ('addi','c.addi') and reg==args[0]:
                for start,s in ss:
                    if start<=val<start+len(s):
                        notes.setdefault(addr,[]).append(f'STR {s[:65]!r}')
                        break
    return notes


def analyze(canonical):
    binary=ROOT/'input/npu_bins'/f'{canonical}_npu_rv32.bin'
    annotated=OUT/f'{canonical}.annotated.S'
    lines=[]; seen=set()
    for raw in annotated.read_text().splitlines():
        m=RE_LINE.match(raw)
        if not m:continue
        addr=int(m.group(1),16); text=m.group(3).strip(); com=m.group(4) or ''
        if addr in seen:continue
        seen.add(addr);lines.append((addr,text,com))
    mmio=special_xref(binary,lines)
    # Data function pointers (not guaranteed entrypoints if source mislabels a data word).
    datafile=ROOT/'input/npu_bins'/f'{canonical}_npu_data.bin'
    fn_ptrs=collections.defaultdict(list)
    if datafile.exists():
        b=datafile.read_bytes()
        for i in range(0,len(b)//4*4,4):
            n=int.from_bytes(b[i:i+4],'little')
            if n in seen and n%2==0:fn_ptrs[n].append(i)
    entry={0x84000108}
    direct=collections.defaultdict(list)
    for addr,s,_ in lines:
        pseudo_s=pseudo(*split_parts(s))
        if 'CALL_' in pseudo_s:
            match=re.search(r'CALL_(0x[0-9a-f]{8})',pseudo_s)
            if match:
                target=int(match.group(1),16)
                if target in seen:
                    direct[target].append(addr);entry.add(target)
    entry.update(fn_ptrs)
    if canonical=='en7523_7915':entry.update(x for x in src.PRIMARY if x in seen)
    # Mark only reachable direct entries, not every branch target.
    entry=sorted(entry)
    e_set=set(entry)
    names={e:src.PRIMARY[e][0] if canonical=='en7523_7915' and e in src.PRIMARY else f'fn_{e:08x}' for e in entry}
    target_labels=set()
    for a,s,_ in lines:
        if any(s.startswith(x) for x in ('b','c.b','md.b','jal','c.j')):
            for tgt in re.findall(r'0x840[0-9a-f]{5}',s):
                target_labels.add(int(tgt,16))
    out=[f'/* {canonical}: RECONSTRUCTED REGISTER-TRANSFER VIEW */',
       '/* This is NOT vendor C source and C syntax is illustrative only. */',
       '/* Unknown operations retain their assembly. Register values are NOT SSA. */',
       '/* Function boundaries are candidate call targets, not proved CFG boundaries. */',
       '/* Use corresponding .annotated.S for exact instruction encodings. */','']
    for addr,s,old in lines:
        if addr in e_set:
            out.extend(['',f'/* Candidate entry, direct callers={len(direct[addr])}, data-pointer refs={len(fn_ptrs.get(addr, []))} */',
                 f'/* {names[addr]} @ 0x{addr:08x} */',f'{names[addr]}:'])
        elif addr in target_labels:
            out.append(f'loc_0x{addr:08x}:')
        m,args=split_parts(s)
        interp=pseudo(m,args)
        note=' | '.join(mmio.get(addr,[]) + ([old] if old and ('[CALL]' in old or '[STR]' in old) else []))
        out.append(f'    {interp:<94} /* 0x{addr:08x}: {s[:78]}'+(' | '+note[:110] if note else '')+' */')
    (OUT/f'{canonical}.pseudo.c.txt').write_text('\n'.join(out)+'\n')
    # Emit table of data-pointer candidates and discovered MMIO accesses.
    hits=collections.defaultdict(list)
    for at,remarks in mmio.items():
        for remark in remarks:
            if 'MMIO ' in remark:hits[remark].append(at)
    index=[f'# Extended cross-references — {canonical}', '',
       'Address references are derived by local constant propagation. Reads/writes whose '
       'base address is dynamically computed may not be detected.','',
       '## Exact MMIO access sites', '',
       '| Register | Instruction sites |','|---|---|']
    for v,addr in sorted(hits.items()):
        index.append(f'| {v} | '+', '.join(f'`0x{x:08x}`' for x in addr[:40])+' |')
    if not hits:index.append('| — | No exact MMIO sites recovered |')
    index.extend(['','## Entrypoints referenced from `npu_data.bin`','',
       '| Firmware address | Data offsets | Direct callers |','|---|---|---:|'])
    for e,offsets in sorted(fn_ptrs.items()):
        index.append(f'| `{e:08x}` | '+', '.join(f'`0x{x:03x}`' for x in offsets[:15])+f' | {len(direct.get(e,[]))} |')
    (OUT/f'{canonical}.xref.md').write_text('\n'.join(index)+'\n')
    return {'name':canonical,'line_count':len(lines),'pseudo_entries':len(entry),
       'pointer_entries':len(fn_ptrs),'mmio_access_sites':sum(len(x) for x in hits.values())}


def main():
    inv=json.loads((OUT/'inventory.json').read_text())
    all_stats=[]
    for info in inv['images']:
        canon=info['name']
        # LLVM generic decode on AN SoCs (different ISA) is not a full decompiler.
        result=analyze(canon)
        all_stats.append(result)
        print(result,flush=True)
    (OUT/'lift_stats.json').write_text(json.dumps(all_stats,indent=2)+'\n')
if __name__=='__main__':main()
