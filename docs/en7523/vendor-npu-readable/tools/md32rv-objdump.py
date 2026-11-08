#!/usr/bin/env python3
"""
md32rv-objdump.py — md32rv-aware wrapper around GNU objdump.

Reads a raw binary or ELF file containing md32rv (Airoha EN7523 NPU custom
RISC-V) code and disassembles it, replacing stock objdump's '.insn' output
for custom opcodes with proper md32rv mnemonics.

Usage:
  ./md32rv-objdump.py [objdump-flags...] <file>
  ./md32rv-objdump.py -d res/src/UNION_EN7523_KERNEL_5_4_demo/md32rv-sdk_unopen/npu_rv32.bin

Decoders cover everything silicon-verified through batch 25 (2026-05-30):
  opcode 0x33  custom ALU / bit-field manipulation family
               - unified md.bfinsr<shift> family (f3=0, f7=4k+3, k=0..31)
               - unified md.bfextr<shift> family (f3=2, f7=4k+3)
               - sh1add/sh2add/max (f7=0x10), clrb/setb/setb3/extb (f7=0x40)
               - clri/andi/setbi/togbi/extui/maskl (f7=0x04)
               - sext.h (f7=0x03 f3=1 rs2=16), clz (f7=0x08 f3=1), ctz (f7=0x08 f3=2)
  opcode 0x0b  CUSTOM-0 uncached load/store family (all 8 f3 values)
               lb/sb/sh/lbu/sw/lh/lhu/lw — address = rs1, imm12 ignored
  opcode 0x67  conditional branches with 5-bit signed immediate
               beqi/bnei/blti/bgei/bltui/bgeui plus standard JALR
  opcode 0x0F  cache ops (funct3=4) — refined per batch 23/25
               line vs global flush+invalidate; ICACHE vs DCACHE
  opcode 0x7b  CUSTOM-3 SM4 crypto (sm4ed/sm4ks)
"""

import re
import subprocess
import sys
import os
from pathlib import Path

# ---------------------------------------------------------------------------
# md32rv instruction decoding
# ---------------------------------------------------------------------------

def sext(x, bits):
    """Sign-extend `x` from `bits` to 32 bits."""
    x &= (1 << bits) - 1
    return x - (1 << bits) if (x >> (bits - 1)) else x


REG_NAMES = ["x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
             "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
             "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
             "x24", "x25", "x26", "x27", "x28", "x29", "x30", "x31"]

REG_ABI = ["zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
           "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
           "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
           "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"]

USE_ABI = True  # can be toggled by -M numeric


def reg(r, abi=None):
    n = r & 0x1f
    names = REG_ABI if (USE_ABI if abi is None else abi) else REG_NAMES
    return names[n]


# ---- opcode 0x33: custom ALU / bit-manip -----
#
# Three families based on (f7, f3):
#   * Binary reg-reg     f7=0x10  f3=0/1/2  sh1add/sh2add/max
#   * Bit set/clear/test f7=0x40  f3=0/3/4/5 clrb/setb3/setb/extb
#   * Immediate bit-field f7=0x04 f3=0/2/3/4/5/6  clri/andi/setbi/togbi/extui/maskl
#   * Unary primitives   f7=0x03 f3=1 sext.h ; f7=0x08 f3=1 clz, f3=2 ctz
#   * Unified bfinsr     f7=4k+3 f3=0  parametric bit-field insert
#   * Unified bfextr     f7=4k+3 f3=2  parametric bit-field extract
#
# Unified formulas (silicon-verified batch 22, 25):
#   bfinsr (f3=0): shift = (f7-3)/4
#                  width = min(rs2_field + shift, 32)
#                  rd[width-1:0] = ((rs1<<shift) | rd_old[shift-1:0]) & mask
#   bfextr (f3=2): shift = (f7-3)/4
#                  rd = (rs1 >> shift) & ((1 << rs2_field) - 1)
#
# All shifts emit consistent md.bfinsrN / md.bfextrN names.  The earlier
# legacy mnemonics (cshl1/15/31, mask12shl2, bfins.h, bfext.w/h) were
# removed — they were misleading aliases for specific shifts of these
# unified families.


# The unified families use the bfinsrN / bfextrN naming for ALL shifts.
# The earlier legacy names (cshl1/15/31, mask12shl2, bfins.h, bfext.w/h)
# were removed in favor of the consistent unified naming.


def decode_op33(word):
    op = word & 0x7f
    if op != 0x33:
        return None
    rd = (word >> 7) & 0x1f
    f3 = (word >> 12) & 0x7
    rs1 = (word >> 15) & 0x1f
    rs2_field = (word >> 20) & 0x1f  # register OR 5-bit immediate, depending on f7/f3
    f7 = (word >> 25) & 0x7f

    # Binary reg-reg (rs2 = register operand)
    if f7 == 0x10:
        if f3 == 0:
            return (f"md.sh1add\t{reg(rd)}, {reg(rs1)}, {reg(rs2_field)}", True)
        elif f3 == 1:
            return (f"md.sh2add\t{reg(rd)}, {reg(rs1)}, {reg(rs2_field)}", True)
        elif f3 == 2:
            return (f"md.max\t{reg(rd)}, {reg(rs1)}, {reg(rs2_field)}", True)
        # f3 == 3 is illegal
        elif f3 == 4:
            return (f"md.mv\t{reg(rd)}, {reg(rs1)}", True)
        elif f3 == 6:
            return (f"md.copysign\t{reg(rd)}, {reg(rs1)}, {reg(rs2_field)}", True)

    # Bit set/clear/test family — rs2 is a REGISTER providing bit index
    if f7 == 0x40:
        if f3 == 0:
            return (f"md.clrb\t{reg(rd)}, {reg(rs1)}, {reg(rs2_field)}", True)
        elif f3 == 2:
            return (f"md.andb\t{reg(rd)}, {reg(rs1)}, {reg(rs2_field)}", True)
        elif f3 == 3:
            return (f"md.setb3\t{reg(rd)}, {reg(rs1)}, {reg(rs2_field)}", True)
        elif f3 == 4:
            return (f"md.setb\t{reg(rd)}, {reg(rs1)}, {reg(rs2_field)}", True)
        elif f3 == 5:
            return (f"md.extb\t{reg(rd)}, {reg(rs1)}, {reg(rs2_field)}", True)

    # Immediate bit-field — rs2_field is a 5-bit IMMEDIATE bit index N
    if f7 == 0x04:
        N = rs2_field
        if f3 == 0:
            return (f"md.clri\t{reg(rd)}, {reg(rs1)}, {N}", True)
        elif f3 == 2:
            return (f"md.andi\t{reg(rd)}, {reg(rs1)}, (1<<{N})", True)
        elif f3 == 3:
            return (f"md.setbi\t{reg(rd)}, {reg(rs1)}, {N}", True)
        elif f3 == 4:
            return (f"md.togbi\t{reg(rd)}, {reg(rs1)}, {N}", True)
        elif f3 == 5:
            return (f"md.extui\t{reg(rd)}, {reg(rs1)}, {N}", True)
        elif f3 == 6:
            if N == 16:
                return (f"md.zext.h\t{reg(rd)}, {reg(rs1)}", True)
            return (f"md.maskl\t{reg(rd)}, {reg(rs1)}, {N}", True)

    # Parametric sign-extend (f7=0x03 f3=1, rs2-field = source bit-width)
    if f7 == 0x03 and f3 == 1:
        return (f"md.sext\t{reg(rd)}, {reg(rs1)}, {rs2_field}", True)
    # clz / ctz
    if f7 == 0x08 and rs2_field == 0:
        if f3 == 1:
            return (f"md.clz\t{reg(rd)}, {reg(rs1)}", True)
        if f3 == 2:
            return (f"md.ctz\t{reg(rd)}, {reg(rs1)}", True)

    # Unified bit-field-insert family (f3=0, f7=4k+3)
    if f3 == 0 and (f7 & 0x3) == 0x3:
        shift = (f7 - 3) // 4
        if 0 <= shift <= 31:
            return (f"md.bfinsr{shift}\t{reg(rd)}, {reg(rs1)}, {rs2_field}", True)

    # Unified bit-field-extract family (f3=2, f7=4k+3)
    if f3 == 2 and (f7 & 0x3) == 0x3:
        shift = (f7 - 3) // 4
        if 0 <= shift <= 31:
            return (f"md.bfextr{shift}\t{reg(rd)}, {reg(rs1)}, {rs2_field}", True)

    return None


# ---- opcode 0x0b: CUSTOM-0 uncached load/store family ----
#
# All 8 f3 values populated, silicon-verified batches 16b → 25:
#   f3=0 md.lb   rd = sext8(byte[rs1])
#   f3=1 md.sb   byte[rs1] = rs2 & 0xFF
#   f3=2 md.sh   halfword[rs1] = rs2 & 0xFFFF
#   f3=3 md.lbu  rd = byte[rs1] (zero-ext)
#   f3=4 md.sw   word[rs1] = rs2
#   f3=5 md.lh   rd = sext16(halfword[rs1])
#   f3=6 md.lhu  rd = halfword[rs1] (zero-ext)
#   f3=7 md.lw   rd = word[rs1]
# Address is exactly rs1; imm12 is silicon-IGNORED but kept syntactically
# present for symmetry with standard RISC-V load/store mnemonics.

OP0B_LOAD_NAMES = {0: "md.lb", 3: "md.lbu", 5: "md.lh", 6: "md.lhu", 7: "md.lw"}
OP0B_STORE_NAMES = {1: "md.sb", 2: "md.sh", 4: "md.sw"}


def decode_op0b(word):
    op = word & 0x7f
    if op != 0x0b:
        return None
    f3 = (word >> 12) & 0x7
    rs1 = (word >> 15) & 0x1f
    rd = (word >> 7) & 0x1f
    # I-type imm12 for loads
    imm_i = sext((word >> 20) & 0xfff, 12)
    if f3 in OP0B_LOAD_NAMES:
        mnem = OP0B_LOAD_NAMES[f3]
        return (f"{mnem}\t{reg(rd)}, {imm_i}({reg(rs1)})", True)
    # S-type imm12 for stores (split across bits[31:25] and [11:7])
    if f3 in OP0B_STORE_NAMES:
        rs2 = (word >> 20) & 0x1f
        imm_s = sext(((word >> 25) & 0x7f) << 5 | ((word >> 7) & 0x1f), 12)
        mnem = OP0B_STORE_NAMES[f3]
        return (f"{mnem}\t{reg(rs2)}, {imm_s}({reg(rs1)})", True)
    return None


# ---- opcode 0x67: JALR + conditional branches with 5-bit immediate -----

def decode_op67(word, addr):
    op = word & 0x7f
    if op != 0x67:
        return None
    f3 = (word >> 12) & 0x7
    rs1 = (word >> 15) & 0x1f
    imm5 = sext((word >> 20) & 0x1f, 5)

    if f3 == 0:
        # Standard JALR — let objdump handle it
        return None

    # B-type branch offset
    b_off = ((word >> 8) & 0xf) << 1      # imm[4:1]
    b_off |= ((word >> 25) & 0x3f) << 5   # imm[10:5]
    b_off |= ((word >> 7) & 1) << 11      # imm[11]
    b_off |= sext(word >> 31, 1) << 12    # imm[12] sign
    b_off = sext(b_off, 13)
    target = (addr + b_off) & 0xffffffff

    mnemonics = {1: "md.beqi", 2: "md.bnei", 3: "md.blti", 4: "md.bgei",
                 5: "md.bltui", 6: "md.bgeui"}

    mnem = mnemonics.get(f3)
    if mnem is None:
        return None  # f3=7 is illegal

    return (f"{mnem}\t{reg(rs1)}, {imm5}, {target:#x}", True)


# ---- opcode 0x0F: MISC-MEM (cache ops) -----
#
# Silicon-verified COMPLETE classification (batches 23, 25, 27):
#
# Op-code N (low 5 bits of rs1's value) → behaviour:
#   N=8               ICACHE invalidate-all (boot prologue uses this)
#   N=20, 28, 30      DCACHE flush+invalidate, GLOBAL
#   N=21              DCACHE flush+invalidate, LINE-scoped
#   N=24              DCACHE invalidate-only, GLOBAL (boot uses for dcache wipe)
#   N=25              DCACHE invalidate-only, LINE-scoped
#   N=29              DCACHE flush+invalidate, LINE-scoped (firmware
#                     per-line range-flush loop)
#   N=0,4,12,16,22,23,26,27,31  TRUE no-ops — neither DCACHE nor ICACHE
#                     affected; reserved encoding space.
#
# IMPORTANT: the firmware encodes the op-code as the runtime VALUE of rs1
# (typically `li a0, N; .insn ...`), NOT as the rs1-field literal.  The
# binutils named forms (md.icache.inval.all etc.) which encode N as the
# 5-bit rs1-field literal are misleading on this silicon — they would
# read whatever value happens to be in that register at runtime.
#
# We disassemble by inspecting the rs1-field (matching binutils' convention)
# while annotating the firmware-style register-value path with a hint when
# it differs (rs1-field=10=a0 is the universal firmware encoding).

# Final cache-op classification — silicon-verified across all 32 op-codes
# in batches 10, 15, 23, 25, 27.  Op-code 8 is the ONLY ICACHE op (boot
# uses it); the remaining "no-op" codes are TRUE no-ops with no observable
# effect on either cache.
CACHE_OP_TABLE = {
    # op-code N : (mnemonic, scope_hint)
    8:  ("md.icache.inval.all",   "global ICACHE invalidate (firmware boot uses)"),
    20: ("md.dcache.flush_inv.all",  "GLOBAL flush+invalidate-D"),
    21: ("md.dcache.flush_inv.line", "LINE flush+invalidate-D"),
    24: ("md.dcache.inval.all",      "GLOBAL invalidate-only-D (firmware boot uses)"),
    25: ("md.dcache.inval.line",     "LINE invalidate-only-D"),
    28: ("md.dcache.flush_inv.all",  "GLOBAL flush+invalidate-D"),
    29: ("md.dcache.flush_inv.line", "LINE flush+invalidate-D (firmware range-flush)"),
    30: ("md.dcache.flush_inv.all",  "GLOBAL flush+invalidate-D"),
}

# True no-op op-codes (silicon-verified to affect neither DCACHE nor ICACHE)
CACHE_OP_NOPS = {0, 4, 12, 16, 22, 23, 26, 27, 31}


def decode_op0f(word):
    op = word & 0x7f
    if op != 0x0f:
        return None
    f3 = (word >> 12) & 0x7

    if f3 == 0:
        return None  # FENCE — let objdump handle
    if f3 == 1:
        return None  # FENCE.I — let objdump handle
    if f3 != 4:
        return None

    # Cache op.  The runtime cache-op-code comes from rs1's REGISTER VALUE.
    # When disassembling firmware that uses `li reg, N; .insn`-style, we
    # emit `md.cache.op <reg>` (the generic mnemonic — binutils' form).
    # When the encoding hard-codes the op-code in the rs1 FIELD (the literal
    # form binutils' named entries assemble to), we emit the resolved name.
    rs1_field = (word >> 15) & 0x1f
    rd = (word >> 7) & 0x1f
    imm12 = (word >> 20) & 0xfff

    # If imm12==0 and rd==0, this matches the binutils "named op" canonical
    # encoding (op-code is the rs1-field literal).  Otherwise it's the
    # firmware's register-value path.
    if imm12 == 0 and rd == 0:
        info = CACHE_OP_TABLE.get(rs1_field)
        if info:
            mnem, _scope = info
            return (f"{mnem}\t# rs1-field literal = {rs1_field}", True)
        if rs1_field in CACHE_OP_NOPS:
            return (f"md.cache.op\t{rs1_field}\t# TRUE no-op (verified batch 27)", True)

    # Generic fallback: firmware uses `li a0, opcode; .insn` — show rs1 reg.
    return (f"md.cache.op\t{reg(rs1_field)}", True)


# ---- opcode 0x7b: CUSTOM-3 (SM4 crypto) -----
#
# Standard Zksed extension at CUSTOM-3.  funct7's low 2 bits encode the
# byte-select (bs) operand; bit 2 (= funct7's bit 2) distinguishes sm4ed
# (bit 2 = 0) from sm4ks (bit 2 = 1).  Per the vendor binutils
# match_sm4_md32 helper.

def decode_op7b(word):
    op = word & 0x7f
    if op != 0x7b:
        return None
    f3 = (word >> 12) & 0x7
    if f3 != 7:
        return None
    f7 = (word >> 25) & 0x7f
    rd = (word >> 7) & 0x1f
    rs1 = (word >> 15) & 0x1f
    rs2 = (word >> 20) & 0x1f
    bs = f7 & 0x3
    if (f7 & 0x4) == 0:
        return (f"md.sm4ed\t{reg(rd)}, {reg(rs1)}, {reg(rs2)}, {bs}", True)
    else:
        return (f"md.sm4ks\t{reg(rd)}, {reg(rs1)}, {reg(rs2)}, {bs}", True)


# ---- main dispatcher -----

def md32rv_decode(word, addr=0):
    """Try to decode `word` as an md32rv custom instruction.
    Returns (text, is_branch) if recognized, else None."""
    result = decode_op33(word)
    if result is not None:
        return result
    result = decode_op0b(word)
    if result is not None:
        return result
    result = decode_op67(word, addr)
    if result is not None:
        return result
    result = decode_op0f(word)
    if result is not None:
        return result
    result = decode_op7b(word)
    if result is not None:
        return result
    return None


# ---------------------------------------------------------------------------
# objdump post-processor
# ---------------------------------------------------------------------------

LINE_RE = re.compile(
    r'^\s*([0-9a-f]+):\s+([0-9a-f ]+?)\s+(\.insn\b.*?)?(\t.*)?$'
)

# Also match lines without .insn — we may need to replace mis-decoded
# standard mnemonics with md32rv ones
LINE_RE2 = re.compile(
    r'^\s*([0-9a-f]+):\s+([0-9a-f ]+?)\s+(.*?)?(\t.*)?$'
)

NOP_RE = re.compile(r'^\s+\.\.\.$')


def process_objdump_line(line, binary_path):
    """Process a single line of objdump output. Returns (new_line, is_custom)."""
    m = LINE_RE2.match(line)
    if not m:
        return line, False

    addr_str = m.group(1)
    bytes_str = m.group(2)
    mnemonic = (m.group(3) or '').strip()
    rest = m.group(4) or ''

    addr = int(addr_str, 16)
    hex_bytes = bytes_str.replace(' ', '')
    nbytes = len(hex_bytes) // 2

    # We only handle 4-byte (32-bit) custom instructions
    if nbytes != 4:
        return line, False

    word = int(hex_bytes[:8], 16)

    result = md32rv_decode(word, addr)
    if result is None:
        return line, False

    text, is_branch = result

    # Format: addr:\t bytes \t mnemonic \t args
    # For custom instructions, discard the .insn trailing args
    if mnemonic.startswith('.insn'):
        new_line = f"    {addr_str}:\t{bytes_str:14s}\t{text}\n"
    else:
        new_line = f"    {addr_str}:\t{bytes_str:14s}\t{text}{rest}\n"
    return new_line, True


def run_objdump(args, binary_path):
    """Run objdump and process its output."""
    # Determine USE_ABI from args
    global USE_ABI
    USE_ABI = True
    for a in args:
        if a == '-M' or a == '--disassembler-options':
            pass
        elif 'numeric' in a.split(','):
            USE_ABI = False
        elif 'no-aliases' in a.split(','):
            pass

    objdump = os.environ.get("OBJDUMP", "riscv64-unknown-elf-objdump")

    # Filter out the binary path from args (it's passed separately)
    objdump_args = [a for a in args if a != binary_path]
    cmd = [objdump] + objdump_args + [binary_path]

    try:
        proc = subprocess.run(cmd, capture_output=True, text=True)
    except FileNotFoundError:
        print(f"Error: {objdump} not found. Install binutils-riscv64-unknown-elf or set OBJDUMP env.", file=sys.stderr)
        sys.exit(1)

    lines = proc.stdout.splitlines(keepends=True)
    for line in lines:
        new_line, _ = process_objdump_line(line, binary_path)
        sys.stdout.write(new_line)
    sys.stdout.flush()

    if proc.stderr:
        sys.stderr.write(proc.stderr)

    return proc.returncode


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    # Find the binary path (last non-flag argument)
    binary_path = None
    for a in reversed(args):
        if not a.startswith('-') and not os.path.exists(a):
            continue
        if os.path.exists(a):
            binary_path = a
            break

    if binary_path is None:
        # Try to find it
        for a in args:
            if os.path.exists(a):
                binary_path = a
                break

    if binary_path is None:
        print("Error: could not find binary file in arguments", file=sys.stderr)
        return 1

    try:
        return run_objdump(args, binary_path)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
