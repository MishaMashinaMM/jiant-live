#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""(JIANT 0.9.12) Decode a crash screen (a photo's numbers) or a CRASH record against the build's ELF: which
function the PC and RETS are in, what the EMU / DBG bits mean, what the short code says.

The crash screen (firmware/src/main.c fm1_fault), top to bottom:
  0.9.12 on   JIANT CRASH / "v0.9.12  CORE 0  2 CORES" / "CODE IL-01" / VEC .. / PC .. / EMU .. / DBG .. / RETS ..
  0.7.0-0.9.11  JIANT CRASH,   then five unlabelled 8-digit hex numbers: VEC PC EMU DBG RETS
  up to 0.6.x   FELUCCA CRASH, the same five numbers in the same order

  tools/crash_decode.py VEC PC EMU DBG RETS        the five numbers, in the screen's order (hex, 0x optional)
  tools/crash_decode.py --pc PC [--rets RETS]      just the addresses
  options: --elf build/felucca.elf (default; it must be the build of the version that crashed: check out that
           commit, `git log --grep '^0.6.80'`, and ./build.sh), --dis build/felucca.dis (the listing: shows the
           instructions around the PC when it is there)"""
import argparse
import os
import struct
import sys

# DBG_MSG (0x1EEE244) bits, from the JieLi AC79 SDK's cpu/wl82/debug.c debug_msg[]
DBG_BITS = {0: "wdt_timeout", 1: "prp_rd_mmu", 2: "prp_wr_limit", 3: "prp_wr_mmu", 4: "axi_rd_inv", 5: "axi_wr_inv",
            6: "c1_rd_mmu", 7: "c0_rd_mmu", 8: "c1_wr_mmu", 9: "c0_wr_mmu", 10: "c1_pc_limit", 11: "c1_wr_limit",
            12: "c0_pc_limit", 13: "c0_wr_limit", 14: "dmc_pnd", 15: "dbg_pnd", 16: "c1_fetch_bus_inv",
            17: "c1_rd_bus_inv", 18: "c1_wr_bus_inv", 19: "c0_fetch_bus_inv", 20: "c0_rd_bus_inv", 21: "c0_wr_bus_inv"}
# EMU_MSG (0x1EEF0D4, the faulting core's bank) bits, the SDK's emu_msg[]
EMU_BITS = {0: "misaligned access", 1: "illegal instruction", 2: "divide by zero", 3: "stack outside its limits",
            8: "ETM watchpoint", 16: "fpu invalid", 17: "fpu inf", 18: "fpu tiny", 19: "fpu huge", 20: "fpu inexact"}
# the short code (firmware/src/cpu_safe.c crash_code): the first cause found, EMU first
CODES = {"MA": "misaligned access (EMU 0)", "IL": "illegal instruction (EMU 1): a jump into data, a corrupted "
         "return address or a misread fetch", "DZ": "divide by zero (EMU 2)", "ST": "stack overflow (EMU 3)",
         "PC": "fetch outside the code (DBG 10 / 12: a wild jump)", "WR": "write into a protected area (DBG 2, 11, 13: "
         "a stack guard band or the vectors)", "MM": "MMU (DBG 1, 3, 6..9)", "BU": "bus: an access to unmapped "
         "memory (DBG 4, 5, 16..21: a NULL or wild pointer, a write into the flash window)",
         "WD": "a hang: the watchdog reset the unit (no exception)", "??": "no cause bit set (another vector)"}
VEC = {1: "the CPU exception (every guard ends here)"}


def elf_syms(path):
    d = open(path, "rb").read()
    if d[:4] != b"\x7fELF" or d[4] != 1:
        sys.exit(f"{path}: not a 32-bit ELF")
    e = "<" if d[5] == 1 else ">"
    shoff, = struct.unpack_from(e + "I", d, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from(e + "HHH", d, 0x2E)
    secs = [struct.unpack_from(e + "IIIIIIIIII", d, shoff + i * shentsize) for i in range(shnum)]
    names = secs[shstrndx]

    def sname(off):
        s = d[names[4] + off:d.index(b"\0", names[4] + off)]
        return s.decode()
    sections = [(sname(s[0]), s[3], s[5]) for s in secs]          # name, addr, size
    syms = []
    for s in secs:
        if s[1] != 2:                                             # SHT_SYMTAB
            continue
        strtab = secs[s[6]]
        for k in range(s[5] // 16):
            nm, val, size, info, _, shndx = struct.unpack_from(e + "IIIBBH", d, s[4] + k * 16)
            if (info & 15) not in (0, 2) or not nm or shndx == 0:   # NOTYPE / FUNC, defined
                continue
            name = d[strtab[4] + nm:d.index(b"\0", strtab[4] + nm)].decode(errors="replace")
            if name.startswith(("$", ".L")):
                continue
            syms.append((val, size, name, info & 15))
    syms.sort()
    return syms, sections


def where(addr, syms, sections):
    sec = next((n for n, a, sz in sections if a and a <= addr < a + sz), None)
    best = None
    for val, size, name, typ in syms:
        if val > addr:
            break
        if typ == 2 and (size == 0 or addr < val + size):
            best = (val, name)
        elif typ == 2 and best is None:
            best = (val, name)
    if not best:
        return f"0x{addr:08X}: no function ({sec or 'outside every section'})"
    return f"0x{addr:08X}: {best[1]} + 0x{addr - best[0]:X}  [{sec or '?'}]"


def region(addr):
    if 0x02000000 <= addr < 0x02100000:
        return "XIP flash (code and constants)"
    if 0x01C00000 <= addr < 0x01C80000:
        return "RAM" + (" (the RAM code: .ram_text / .c1_text / .hot_text)" if addr >= 0x01C6D000 else "")
    return "not code nor RAM: a wild value"


def bits(v, table):
    return [f"{b} {table.get(b, 'reserved')}" for b in range(32) if (v >> b) & 1]


def code_of(emu, dbg):
    for b, c in ((0, "MA"), (1, "IL"), (2, "DZ"), (3, "ST")):
        if (emu >> b) & 1:
            return c
    for mask, c in (((1 << 10) | (1 << 12), "PC"), ((1 << 2) | (1 << 11) | (1 << 13), "WR"),
                    ((1 << 1) | (1 << 3) | (15 << 6), "MM"), ((3 << 4) | (63 << 16), "BU")):
        if dbg & mask:
            return c
    return "??"


def listing(dis, addr, n=6):
    if not os.path.exists(dis):
        return []
    want = f"{addr:x}:"
    lines = open(dis, errors="replace").read().splitlines()
    for i, ln in enumerate(lines):
        if ln.strip().startswith(want):
            return lines[max(0, i - n):i + n]
    return []


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("nums", nargs="*", help="VEC PC EMU DBG RETS, as on the screen")
    ap.add_argument("--pc")
    ap.add_argument("--rets")
    ap.add_argument("--elf", default="build/felucca.elf")
    ap.add_argument("--dis", default="build/felucca.dis")
    a = ap.parse_args()
    h = lambda s: int(s, 16)
    vec = emu = dbg = None
    pc, rets = (h(a.pc) if a.pc else None), (h(a.rets) if a.rets else None)
    if a.nums:
        if len(a.nums) != 5:
            sys.exit("five numbers, in the screen's order: VEC PC EMU DBG RETS")
        vec, pc, emu, dbg, rets = (h(x) for x in a.nums)
    if pc is None:
        ap.error("the five numbers or --pc")
    syms, sections = elf_syms(a.elf)
    if vec is not None:
        print(f"VEC  0x{vec:X}: " + VEC.get(vec, "an interrupt with no handler (a stray vector)"))
    print(f"PC   {where(pc, syms, sections)}   {region(pc)}")
    if rets is not None:
        print(f"RETS {where(rets, syms, sections)}   (the caller: where the faulting function returns to)")
    if emu is not None:
        print(f"EMU  0x{emu:08X}: " + (", ".join(bits(emu, EMU_BITS)) or "none"))
        print(f"DBG  0x{dbg:08X}: " + (", ".join(bits(dbg, DBG_BITS)) or "none"))
        c = code_of(emu, dbg)
        print(f"CODE {c}: {CODES[c]}")
        if dbg & ((1 << 6) | (1 << 8) | (1 << 10) | (1 << 11) | (7 << 16)):
            print("     a core-1 bit is set: the second core misbehaved too (MENU > SYSTEM > CPU 1 CORE)")
    for ln in listing(a.dis, pc):
        print("   " + ln)


if __name__ == "__main__":
    main()
