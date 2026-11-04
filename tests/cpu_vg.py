#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""(JIANT 0.6.83) the audio path's CPU under valgrind (tests/cpu_vg.c): instructions per sample, section by section,
against tests/cpu_baseline_vg.txt (peak/*: instructions in the worst single block of the section's rounds). callgrind's
counts are exact (the same on every run of the same build), so the
tolerance is small: a section more than TOL over its baseline fails; one far under only notes it.
   python3 tests/cpu_vg.py BINARY [BASELINE]      (run_tests.sh, when valgrind is there)
   BUDGET_UPDATE=1 rewrites the baseline (on purpose: look at the diff)."""
import glob, os, re, subprocess, sys, tempfile

TOL = 0.08                                    # +8 %: a real change, never noise
SAMPLES = 256 * 32                            # cpu_vg.c NB blocks of CTL samples

binary = sys.argv[1]
base_path = sys.argv[2] if len(sys.argv) > 2 else "tests/cpu_baseline_vg.txt"
out = tempfile.mkdtemp(prefix="cpu_vg_")
prefix = os.path.join(out, "cg.out")
r = subprocess.run(["valgrind", "--tool=callgrind", "--collect-atstart=yes", "--instr-atstart=no",
                    f"--callgrind-out-file={prefix}", binary], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
if r.returncode:
    print("cpu_vg: the run failed\n" + r.stderr[-2000:])
    sys.exit(1)
now, fx_n = {}, {}
for f in glob.glob(prefix + "*"):
    txt = open(f, errors="replace").read()
    m = re.search(r"^desc: Trigger: Client Request: (\S+)", txt, re.M)
    t = re.search(r"^(?:totals|summary): (\d+)", txt, re.M)
    if m and t and m.group(1).startswith("peak/"):    # (0.9.0) one block each: the worst of its rounds (.rNN)
        k = re.sub(r"\.r\d+$", "", m.group(1))
        now[k] = max(now.get(k, 0), int(t.group(1)))
    elif m and t and m.group(1).startswith("fx/"):    # (0.9.17) rounds of NB blocks (.rNN): their mean
        k = re.sub(r"\.r\d+$", "", m.group(1))
        fx_n[k] = fx_n.get(k, 0) + 1
        now[k] = now.get(k, 0) + (int(t.group(1)) / SAMPLES - now.get(k, 0)) / fx_n[k]
    elif m and t:
        now[m.group(1)] = int(t.group(1)) / SAMPLES
if not now:
    print("cpu_vg: no sections measured")
    sys.exit(1)

base = {}
if os.path.exists(base_path):
    for line in open(base_path):
        p = line.split()
        if len(p) == 2 and not line.startswith("#"):
            base[p[0]] = float(p[1])

if os.environ.get("BUDGET_UPDATE"):
    with open(base_path, "w") as fo:
        fo.write("# JIANT audio path CPU under valgrind (tests/cpu_vg.c, tests/cpu_vg.py): instructions per sample,\n"
                 "# x86-64 gcc -O2. Exact counts; the check allows +8 %. Rewritten by BUDGET_UPDATE=1.\n")
        for k in sorted(now):
            fo.write(f"{k} {now[k]:.0f}\n")
    print(f"cpu_vg: {len(now)} sections written to {base_path}")
    sys.exit(0)

bad = 0
for k in sorted(now):
    b = base.get(k)
    if b is None:
        print(f"cpu_vg: {k:12s} {now[k]:8.0f} instructions / sample   no baseline (BUDGET_UPDATE=1 adds it)")
        continue
    d = now[k] / b - 1
    state = "ok"
    if d > TOL:
        state = f"OVER BUDGET (+{d * 100:.1f} %, limit +{TOL * 100:.0f} %)"
        bad += 1
    elif d < -0.25:
        state = f"ok, {-d * 100:.0f} % under (BUDGET_UPDATE=1 to keep it)"
    unit = "instructions, the worst block" if k.startswith("peak/") else "instructions / sample"
    print(f"cpu_vg: {k:12s} {now[k]:8.0f} {unit}  (baseline {b:.0f}, {d * 100:+.1f} %)  {state}")
print("cpu_vg: " + ("FAILED" if bad else "all within budget"))
sys.exit(1 if bad else 0)
