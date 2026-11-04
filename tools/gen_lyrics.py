#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""VOICE words (JIANT 0.8.5): the phoneme codes, the English dictionary and the lyric generator's word banks as
constant tables (firmware/src/sing.c reads them), from tools/lyrics/:
  en_dict.txt   the English words the device knows (a subset of the CMU Pronouncing Dictionary, BSD-2-Clause): each
                its letters, then its phonemes (a byte each, 0x80 | syllable start 0x40 | code), sorted, front-coded
                (the letters shared with the word before), with an index every EN_BLK words (binary search, then at
                most EN_BLK compared); (0.9.0) Huffman-coded (en_dict_pack: the shared count, the letters, the
                phonemes, each their own canonical code), a block of EN_BLK from a byte (8: was 16 as bytes)
  es.txt en.txt ja.txt   the generator's banks: words by theme and category, each with its syllables (Spanish by the
                same rules sing.c reads it with, sy_es below; English from the dictionary; Japanese in morae)
Spanish and Japanese text is read by rules on the device; English only through the dictionary (and rough letter rules
for a word it lacks). The phoneme list here is the order of sing.c's PHON[] (a static assert checks the count).
  tools/gen_lyrics.py OUT.h"""
import re
import sys
from pathlib import Path

LY = Path(__file__).resolve().parent / "lyrics"

# code, name shown (at most 2 letters + 0)
PHONEMES = [
    ("SIL", "_"),
    ("A", "a"), ("E", "e"), ("I", "i"), ("O", "o"), ("U", "u"), ("UJ", "u"),            # ES / JA (UJ: Japanese u)
    ("IH", "ih"), ("EH", "eh"), ("AE", "ae"), ("AA", "aa"), ("AO", "ao"), ("UH", "uh"), ("AH", "ah"),
    ("ER", "er"), ("AX", "ax"),                                                           # EN
    ("Y", "y"), ("W", "w"),
    ("L", "l"), ("RT", "r"), ("RR", "rr"), ("RA", "r"),                                   # tap, trill, EN approximant
    ("M", "m"), ("N", "n"), ("NY", "ny"), ("NG", "ng"), ("NM", "N"),                      # NM: Japanese moraic n
    ("P", "p"), ("T", "t"), ("K", "k"), ("B", "b"), ("D", "d"), ("G", "g"),
    ("F", "f"), ("S", "s"), ("SH", "sh"), ("H", "h"), ("X", "x"), ("TH", "th"), ("V", "v"), ("Z", "z"),
    ("ZH", "zh"), ("DH", "dh"),
    ("CH", "ch"), ("TS", "ts"), ("J", "j"),
]
PH = {c: i for i, (c, _) in enumerate(PHONEMES)}
VOWELS = {PH[c] for c in ("A", "E", "I", "O", "U", "UJ", "IH", "EH", "AE", "AA", "AO", "UH", "AH", "ER", "AX")}
assert len(PHONEMES) < 64

# ------------------------------------------------------------------------------------------ English (CMU) ---
ARPA = {"AA": ["AA"], "AE": ["AE"], "AO": ["AO"], "EH": ["EH"], "ER": ["ER"], "IH": ["IH"], "IY": ["I"], "UH": ["UH"],
        "UW": ["U"], "AW": ["AA", "UH"], "AY": ["AA", "IH"], "EY": ["E", "IH"], "OW": ["O", "UH"], "OY": ["AO", "IH"],
        "AH": ["AH"], "B": ["B"], "CH": ["CH"], "D": ["D"], "DH": ["DH"], "F": ["F"], "G": ["G"], "HH": ["H"], "JH": ["J"], "K": ["K"],
        "L": ["L"], "M": ["M"], "N": ["N"], "NG": ["NG"], "P": ["P"], "R": ["RA"], "S": ["S"], "SH": ["SH"], "T": ["T"],
        "TH": ["TH"], "V": ["V"], "W": ["W"], "Y": ["Y"], "Z": ["Z"], "ZH": ["ZH"]}
EN_ONSET2 = {("P", "L"), ("B", "L"), ("K", "L"), ("G", "L"), ("F", "L"), ("S", "L"), ("P", "RA"), ("B", "RA"),
             ("T", "RA"), ("D", "RA"), ("K", "RA"), ("G", "RA"), ("F", "RA"), ("TH", "RA"), ("SH", "RA"), ("S", "P"),
             ("S", "T"), ("S", "K"), ("S", "M"), ("S", "N"), ("S", "W"), ("T", "W"), ("K", "W"), ("D", "W"), ("G", "W"),
             ("P", "Y"), ("B", "Y"), ("K", "Y"), ("G", "Y"), ("F", "Y"), ("H", "Y"), ("M", "Y"), ("V", "Y"), ("S", "F")}
EN_ONSET3 = {("S", "P", "L"), ("S", "P", "RA"), ("S", "T", "RA"), ("S", "K", "RA"), ("S", "K", "W"), ("S", "K", "Y"),
             ("S", "P", "Y")}


def en_phones(arpa):
    """CMU phones -> [(code, syllable start)]: a syllable a vowel, consonants between two by the longest legal onset
    (maximal onset); a diphthong's second part stays in its syllable (an offglide)"""
    seq = []                                         # (code name, 1 = a syllable's vowel)
    for a in arpa:
        base = re.sub(r"\d", "", a)
        parts = ARPA[base]
        if base == "AH":
            parts = ["AX"] if a.endswith("0") else ["AH"]
        for j, c in enumerate(parts):
            seq.append((c, PH[c] in VOWELS and j == 0))
    nuc = [i for i, (c, v) in enumerate(seq) if v]
    starts = {0}
    for a, b in zip(nuc, nuc[1:]):
        cons = [i for i in range(a + 1, b) if PH[seq[i][0]] not in VOWELS]
        if not cons:
            starts.add(b)
            continue
        names = tuple(seq[i][0] for i in cons)
        k = len(names)
        if k >= 3 and names[-3:] in EN_ONSET3:
            starts.add(cons[-3])
        elif k >= 2 and names[-2:] in EN_ONSET2:
            starts.add(cons[-2])
        else:
            starts.add(cons[-1])
    if not nuc:
        return [], 0
    return [(PH[c], i in starts) for i, (c, _) in enumerate(seq)], len(nuc)


# (0.9.0) the dictionary Huffman-coded (-5.4 KB): three canonical codes (sing.c ly_sym), at most HC_MAX bits a code
HC_MAX = 15
LY_END = 0x7F                                        # the end of a word's letters / phonemes (no phoneme byte & 0x7F)


def huff_code(count):
    """symbol -> (length, code), canonical (by length, then symbol), lengths at most HC_MAX; and the table sing.c
    reads: the number of codes of each length 1..HC_MAX, then the symbols in code order"""
    import heapq
    if len(count) == 1:
        length = {next(iter(count)): 1}
    else:
        heap = [(c, i, (s,)) for i, (s, c) in enumerate(sorted(count.items()))]
        heapq.heapify(heap)
        length = dict.fromkeys(count, 0)
        n = len(heap)
        while len(heap) > 1:
            a, b = heapq.heappop(heap), heapq.heappop(heap)
            for s in a[2] + b[2]:
                length[s] += 1
            n += 1
            heapq.heappush(heap, (a[0] + b[0], n, a[2] + b[2]))
    assert max(length.values()) <= HC_MAX, "a code over HC_MAX bits"
    order = sorted(count, key=lambda s: (length[s], s))
    code, c, prev = {}, 0, length[order[0]]
    for s in order:
        c <<= length[s] - prev
        prev = length[s]
        code[s] = (length[s], c)
        c += 1
    table = [sum(1 for s in order if length[s] == n) for n in range(1, HC_MAX + 1)] + order
    return code, table


EN_BLK = 8                                           # (0.9.0) the dictionary's words a block (an index entry)
LUT_BITS = {"P": 6, "L": 6, "H": 7}                 # the codes' lookup tables: 99.6 %, 96 %, 94 % of the symbols


def huff_lut(code, k):
    """the 2^k entries of a code's lookup table (sing.c ly_sym): the next k bits -> length << 8 | symbol of the code
    they start with, 0 when it is longer than k bits"""
    lut = [0] * (1 << k)
    for s, (n, c) in code.items():
        if n <= k:
            for v in range(c << (k - n), (c + 1) << (k - n)):
                lut[v] = n << 8 | s
    return lut


def huff_decode(table, bits, pos):
    """sing.c ly_sym: the symbol at bit pos of bits (a list of 0 / 1); (symbol, next pos)"""
    code = first = idx = 0
    for n in range(HC_MAX):
        code |= bits[pos]
        pos += 1
        cnt = table[n]
        if code - first < cnt:
            return table[HC_MAX + idx + code - first], pos
        idx += cnt
        first = (first + cnt) << 1
        code <<= 1
    raise AssertionError("no code")


def en_dict_pack(recs):
    """recs: [(shared, rest, phoneme bytes)] in order -> (blob, idx, tables, codes): each EN_BLK-th word (pre 0) starts
    a block at a
    byte (idx: its offset); a word is its shared count (code P), its letters then LY_END (code L, 'a' .. 'z' as 0 ..
    25), its phoneme bytes & 0x7F then LY_END (code H)"""
    from collections import Counter
    cp, cl, ch = Counter(), Counter(), Counter()
    for pre, rest, ph in recs:
        cp[pre] += 1
        cl.update(ord(c) - 97 for c in rest)
        cl[LY_END] += 1
        ch.update(b & 0x7F for b in ph)
        ch[LY_END] += 1
    (kp, tp), (kl, tl), (kh, th) = huff_code(cp), huff_code(cl), huff_code(ch)
    blob, idx, bits = bytearray(), [], []

    def put(code, s):
        n, c = code[s]
        bits.extend((c >> (n - 1 - i)) & 1 for i in range(n))

    def flush():
        bits.extend([0] * (-len(bits) % 8))
        for i in range(0, len(bits), 8):
            blob.append(int("".join(map(str, bits[i:i + 8])), 2))
        bits.clear()
    for k, (pre, rest, ph) in enumerate(recs):
        if k % EN_BLK == 0:
            flush()
            idx.append(len(blob))
        put(kp, pre)
        for c in rest:
            put(kl, ord(c) - 97)
        put(kl, LY_END)
        for b in ph:
            put(kh, b & 0x7F)
        put(kh, LY_END)
    flush()
    blob += bytes(4)                                 # (sing.c ly_fill reads up to 4 bytes ahead)
    return blob, idx, (tp, tl, th), (kp, kl, kh)


def en_dict_unpack(blob, idx, tables, n):
    """the words back from en_dict_pack's blob, as sing.c sy_en_find reads them: [(word, phoneme bytes)]"""
    bits = [(b >> (7 - i)) & 1 for b in blob for i in range(8)]
    tp, tl, th = tables
    out, w = [], ""
    for k in range(n):
        pos = idx[k // EN_BLK] * 8 if k % EN_BLK == 0 else pos
        pre, pos = huff_decode(tp, bits, pos)
        w = w[:pre]
        while True:
            s, pos = huff_decode(tl, bits, pos)
            if s == LY_END:
                break
            w += chr(97 + s)
        ph = []
        while True:
            s, pos = huff_decode(th, bits, pos)
            if s == LY_END:
                break
            ph.append(0x80 | s)
        out.append((w, bytes(ph)))
    return out


def load_en_dict():
    d = {}
    for ln in (LY / "en_dict.txt").read_text().splitlines():
        if not ln or ln.startswith("#"):
            continue
        w, *ph = ln.split()
        d[w] = ph
    return d


# ------------------------------------------------------------------------------------------------ Spanish ---
def es_tokens(w):
    """letters -> [(code, kind)], kind S strong vowel, W weak vowel, C consonant (sing.c sy_es_tok)"""
    out, i, n = [], 0, len(w)
    isv = lambda c: c != "" and c in "aeiou"
    while i < n:
        c, nx, nx2 = w[i], w[i + 1] if i + 1 < n else "", w[i + 2] if i + 2 < n else ""
        if c in "aeo":
            out.append((PH["AEO"[0] if c == "a" else "E" if c == "e" else "O"], "S"))
        elif c in "iu":
            out.append((PH["I" if c == "i" else "U"], "W"))
        elif c == "y":
            out.append((PH["Y"], "C") if isv(nx) else (PH["I"], "W"))
        elif c == "h":
            pass
        elif c == "c":
            if nx == "h":
                out.append((PH["CH"], "C"))
                i += 1
            else:
                out.append((PH["S" if nx in ("e", "i") else "K"], "C"))
        elif c == "q":
            out.append((PH["K"], "C"))
            if nx == "u":
                i += 1
        elif c == "g":
            if nx in ("e", "i"):
                out.append((PH["X"], "C"))
            else:
                out.append((PH["G"], "C"))
                if nx == "u" and nx2 in ("e", "i"):
                    i += 1
        elif c == "j":
            out.append((PH["X"], "C"))
        elif c == "x":
            out += [(PH["K"], "C"), (PH["S"], "C")]
        elif c in "zs":
            out.append((PH["S"], "C"))
        elif c in "vb":
            out.append((PH["B"], "C"))
        elif c in "dfkptmw":
            out.append((PH[c.upper()], "C"))
        elif c == "l":
            if nx == "l":
                out.append((PH["Y"], "C"))
                i += 1
            else:
                out.append((PH["L"], "C"))
        elif c == "n":
            if nx == "y" and isv(nx2):
                out.append((PH["NY"], "C"))
                i += 1
            else:
                out.append((PH["N"], "C"))
        elif c == "r":
            if nx == "r":
                out.append((PH["RR"], "C"))
                i += 1
            elif not out or out[-1][0] in (PH["N"], PH["L"], PH["S"]):
                out.append((PH["RR"], "C"))
            else:
                out.append((PH["RT"], "C"))
        i += 1
    return out


ES_CL1 = {PH[c] for c in ("P", "B", "F", "K", "G", "T", "D")}
ES_CL2 = {PH["L"], PH["RT"]}


def es_cluster(a, b):
    return a in ES_CL1 and b in ES_CL2 and not (a == PH["D"] and b == PH["L"])


def syllabify(tok):
    """[(code, kind)] -> [(code, syllable start)], syllables (sing.c sy_syl): vowel groups split between two strong
    vowels; a group's nucleus its first strong vowel (or its last); weak vowels before it glides, after it offglides;
    the consonants between two groups: one to the next, two to the next when a cluster (pl br tr ..), else split"""
    groups, i = [], 0
    while i < len(tok):
        if tok[i][1] == "C":
            i += 1
            continue
        j = i + 1
        while j < len(tok) and tok[j][1] != "C" and not (tok[j][1] == "S" and tok[j - 1][1] == "S"):
            j += 1
        groups.append((i, j))
        i = j
    out = [list(t) for t in tok]
    starts = {0}
    for g, (a, b) in enumerate(groups):
        nu = next((k for k in range(a, b) if tok[k][1] == "S"), b - 1)
        for k in range(a, nu):
            out[k][0] = PH["Y"] if tok[k][0] == PH["I"] else PH["W"]
        if g:
            pa = groups[g - 1][1]
            cons = list(range(pa, a))
            if not cons:
                starts.add(a)
            elif len(cons) == 1:
                starts.add(cons[0])
            elif len(cons) == 2:
                starts.add(cons[0] if es_cluster(tok[cons[0]][0], tok[cons[1]][0]) else cons[1])
            else:
                starts.add(cons[-2] if es_cluster(tok[cons[-2]][0], tok[cons[-1]][0]) else cons[-1])
    return [(c, k in starts) for k, (c, _) in enumerate(out)], len(groups)


def es_nsyl(w):
    return syllabify(es_tokens(w))[1]


# ----------------------------------------------------------------------------------------------- Japanese ---
def ja_nmora(w):
    """morae of romaji (sing.c sy_ja): a vowel, a consonant (+ y) + vowel, n before a consonant / at the end, - (the
    vowel again); a doubled consonant is no mora of its own (a longer closure on the next)"""
    n, i, m = len(w), 0, 0
    v = lambda c: c != "" and c in "aiueo"
    while i < n:
        c = w[i]
        nx = w[i + 1] if i + 1 < n else ""
        if v(c) or c == "-":
            m += 1
            i += 1
        elif c == "n" and not v(nx) and nx != "y":
            m += 1
            i += 2 if nx in ("n", "'") and not v(w[i + 2] if i + 2 < n else "") else 1
        elif c == nx and c != "n" or (c == "t" and nx == "c"):
            i += 1
        else:
            j = i
            while j < n and not v(w[j]) and w[j] != "-":
                j += 1
            m += 1
            i = j + 1 if j < n and v(w[j]) else j
    return m


# ------------------------------------------------------------------------------------------------- banks ---
THEMES = ["LOVE", "NIGHT", "SEA", "SKY", "ANY", "ROAD", "DREAM", "FIRE", "FN"]   # (ANY: none of its own)
CATS = ["N", "V", "A", "D", "P", "C"]


def load_bank(lang, nsyl_of):
    rows = {}
    for ln in (LY / f"{lang}.txt").read_text().splitlines():
        if not ln.strip() or ln.startswith("#"):
            continue
        head, words = ln.split(":", 1)
        theme, cat = head.split()
        for w in words.split():
            fem = w.endswith("/f")
            w = w.split("/")[0]
            ns = nsyl_of(w)
            if not 1 <= ns <= 15:
                raise SystemExit(f"gen_lyrics: {lang} {w}: {ns} syllables")
            rows.setdefault((THEMES.index(theme), CATS.index(cat)), []).append((w, ns, fem))
    return rows


def c_bytes(data, per=24):
    s = ",".join(str(b) for b in data)
    out, line = [], ""
    for tok in s.split(","):
        if len(line) + len(tok) + 1 > 116:
            out.append(line)
            line = ""
        line += tok + ","
    out.append(line)
    return "\n    ".join(out)


def c_strs(items):
    out, line = [], ""
    for tok in items:
        if line and len(line) + len(tok) + 2 > 116:
            out.append(line)
            line = ""
        line += tok + ", "
    out.append(line)
    return "\n    ".join(out)


def main():
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("felucca_lyrics.h")
    en = load_en_dict()
    L = ["/* generated by tools/gen_lyrics.py from tools/lyrics/: do not edit */"]
    L.append("enum { " + ", ".join(f"PH_{c}" for c, _ in PHONEMES) + ", PH_COUNT };")
    L.append("static const char PH_NAME[PH_COUNT][3] = {" + ", ".join(f'"{n}"' for _, n in PHONEMES) + "};")
    # the English dictionary
    recs, nsyl_en, plain = [], {}, 0
    words = sorted(en)
    prev = ""
    for k, w in enumerate(words):
        ph, ns = en_phones(en[w])
        if not ns or not re.fullmatch(r"[a-z]{1,23}", w):
            raise SystemExit(f"gen_lyrics: {w}: no vowel, or not a-z")
        if len(ph) > 32:                             # (sing.c SW_MAX)
            raise SystemExit(f"gen_lyrics: {w}: over 32 phonemes")
        nsyl_en[w] = ns
        pre = 0
        if k % EN_BLK:                               # each EN_BLK-th whole (the index's), the rest front-coded
            while pre < min(len(w), len(prev), 31) and w[pre] == prev[pre]:
                pre += 1
        recs.append((pre, w[pre:], bytes(0x80 | (0x40 if st else 0) | c for c, st in ph)))
        p16 = 0                                      # (0.8.9's bytes, a whole word each 16th: the print's comparison)
        while k % 16 and p16 < min(len(w), len(prev), 31) and w[p16] == prev[p16]:
            p16 += 1
        plain += 1 + len(w) - p16 + len(ph) + 2 * (k % 16 == 0)
        prev = w
    blob, idx, (tp, tl, th), codes = en_dict_pack(recs)
    if en_dict_unpack(blob, idx, (tp, tl, th), len(words)) != [(w, r[2]) for w, r in zip(words, recs)]:
        raise SystemExit("gen_lyrics: the dictionary does not decode back")
    assert len(blob) < 65536
    L.append(f"#define LY_EN_N {len(words)}u")
    L.append(f"#define LY_END {LY_END}u")
    L.append(f"static const uint8_t LY_EN_DICT[{len(blob)}] = {{\n    {c_bytes(blob)}}};")
    L.append(f"static const uint16_t LY_EN_IDX[{len(idx)}] = {{\n    {c_bytes(idx)}}};")
    L.append(f"#define LY_EN_BLK {EN_BLK}u")
    heads = words[::EN_BLK]                          # LY_EN_LET[c]: the first block whose first word starts at or after c
    let = [sum(1 for h in heads if h[0] < chr(97 + c)) for c in range(27)]
    L.append(f"static const uint16_t LY_EN_LET[27] = {{\n    {c_bytes(let)}}};")
    nlut = 0
    for nm, t, code in (("P", tp, codes[0]), ("L", tl, codes[1]), ("H", th, codes[2])):
        lut = huff_lut(code, LUT_BITS[nm])
        nlut += 2 * len(lut)
        L.append(f"#define LY_EN_K{nm} {LUT_BITS[nm]}u")
        L.append(f"static const uint8_t LY_EN_HC{nm}[{len(t)}] = {{\n    {c_bytes(t)}}};")
        L.append(f"static const uint16_t LY_EN_LUT{nm}[{len(lut)}] = {{\n    {c_bytes(lut)}}};")
    # the words and their phoneme bytes as text, for the host tests only (tests/voice_test.c: every word looked up)
    L.append("#ifdef LY_EN_REF")
    L.append("static const char *const LY_EN_REF_W[LY_EN_N] = {\n    " +
             c_strs(f'"{w}"' for w in words) + "};")
    L.append("static const char *const LY_EN_REF_PH[LY_EN_N] = {\n    " +
             c_strs('"' + "".join(f"\\x{b:02x}" for b in r[2]) + '"' for r in recs) + "};")
    L.append("#endif")
    # the banks
    nsyl = {"es": es_nsyl, "en": lambda w: nsyl_en[w] if w in nsyl_en else 0, "ja": ja_nmora}
    counts = {}
    for lang in ("es", "en", "ja"):
        rows = load_bank(lang, nsyl[lang])
        txt, offs, meta, grp = bytearray(), [], [], [[[0, 0] for _ in CATS] for _ in THEMES]
        n = 0
        for t in range(len(THEMES)):
            for c in range(len(CATS)):
                r = rows.get((t, c), [])
                grp[t][c] = [n, len(r)]
                for w, ns, fem in r:
                    offs.append(len(txt))
                    txt += w.encode() + b"\0"
                    meta.append(ns | (16 if fem else 0))
                    n += 1
        counts[lang] = n
        U = lang.upper()
        L.append(f"#define LY_{U}_NW {n}u")
        L.append(f"static const char LY_{U}_TXT[{len(txt)}] = {{\n    {c_bytes(txt)}}};")
        offs = offs[::16]                            # (0.9.0) each 16th word's place (sing.c ly_word: then the 0s)
        L.append(f"static const uint16_t LY_{U}_OFF[{len(offs)}] = {{\n    {c_bytes(offs)}}};")
        L.append(f"static const uint8_t LY_{U}_META[{n}] = {{\n    {c_bytes(meta)}}};")
        rows_c = ",\n    ".join("{" + ", ".join(f"{{{a}, {b}}}" for a, b in t) + "}" for t in grp)
        L.append(f"static const uint16_t LY_{U}_GRP[{len(THEMES)}][{len(CATS)}][2] = {{\n    {rows_c}}};")
    out.write_text("\n".join(L) + "\n")
    print(f"lyrics: {len(PHONEMES)} phonemes; English dictionary {len(words)} words, "
          f"{len(blob) + 2 * len(idx) + len(tp) + len(tl) + len(th) + nlut} B (Huffman; {plain} B as bytes); "
          f"banks ES {counts['es']} EN {counts['en']} JA {counts['ja']} words -> {out}")


if __name__ == "__main__":
    main()
