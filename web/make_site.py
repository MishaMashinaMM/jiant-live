#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""Make the site (GitHub Pages):

  index.html                  redirect to the installer (the old URL keeps working)
  firmware/felucca-VER.fwsc   the package (+ LICENSE, LICENSING.md, LICENSES/: the package holds
                              JieLi SDK files under Apache-2.0, see LICENSING.md)
  webapp/installer/index.html index_pkg.html, self-contained (fm1pkg.js, fm1ota.js, fm1backup.js, fm1diag.js and the
                              metadata inlined)
  404.html                    web/404.html: an old or mistyped link (/installer/, /webapp/, ...:1:1) to the page it meant
  webapp/editor/index.html    editor.html (+ fukiai.ttf, FUKIAI-LICENSE.txt)
  src/                        not touched

  web/make_site.py build/felucca-X.Y.fwsc X.Y OUT_DIR [PREV_DIR]
  web/make_site.py manual OUT_DIR   (JIANT 0.9.12) only the interactive manual, OUT_DIR/manual/: web/manual (its page,
                                    scripts and the firmware's screens, tools/gen_manual.py), checked complete first

PREV_DIR (JIANT 0.6.46): earlier versions offered next to the current one (firmware/versions.json, the installer's
version list): its versions.json ([{"version", "file"}]) and the packages it names, up to KEEP_OLD, one build per
version. Since 0.9.9-beta1 the workflow builds them from the commits listed in web/CLASSIC (never from the live site),
and each version keeps its own package identity (FM-1_900 for the 0.6 line, FM-1_909 later): the installer checks the
package it loads against the identity listed for that version.

The package must be one made by tools/fm1pkg_make.py (Felucca's own loader, no vendor files).
Its identity (FM-1_9xx) is read from the package; the device must report it after
the install.
"""
import json
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F
KEEP_OLD = 8                                    # (0.9.9-beta4) the milestones in web/CLASSIC
SAFE = re.compile(r"felucca-[A-Za-z0-9.-]+\.fwsc")


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def product_of(raw):
    """the package identity: one marker byte after each of the first 20 blocks (fm1pkg.js productOf)"""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


def base_of(version):
    """v0.6.46-abc1234 -> v0.6.46: the builds of one version (0.9.9-beta4: only a commit's 7 hex digits come off, so
    v0.9.9-beta3 and v0.9.9 stay two versions)"""
    return re.sub(r"-[0-9a-f]{7}$", "", version)


def earlier(prev, version, product):
    """[(entry, file)]: PREV_DIR's versions, newest first, one per version, not this one, at most KEEP_OLD, each
    package there and a Felucca package for this model (its own identity goes in the entry: an older line has another)"""
    out, seen = [], {base_of(version)}
    try:
        listed = json.loads((prev / "versions.json").read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return out
    for e in listed if isinstance(listed, list) else []:
        v, f = str(e.get("version", "")), str(e.get("file", ""))
        src = prev / f
        if not SAFE.fullmatch(f) or base_of(v) in seen or not src.is_file():
            continue
        raw = src.read_bytes()
        pf = product_of(raw)
        if pf.split("_")[0] != product.split("_")[0] or b"FELUCCA-LOADER-1" not in raw:
            continue
        seen.add(base_of(v))
        out.append(({"version": v, "file": f, "product": pf}, src))
        if len(out) >= KEEP_OLD:
            break
    return out


MANUAL_FILES = ("index.html", "manual.js", "data.js", "nav.js", "sw.js")


def manual(out):
    """(JIANT 0.9.12) the interactive manual at OUT/manual/ (the front page links it; the workflow puts it next to the
    emulator): its five files and the screens they name, nothing else from web/manual"""
    src, dst = HERE / "manual", Path(out) / "manual"
    names = set()
    for f in ("nav.js", "data.js"):
        names |= set(re.findall(r"""(?:"shot":"|\bshot:\s*')([\w-]+)""", (src / f).read_text(encoding="utf-8")))
    for m in re.findall(r"\bshots:\s*\[([^\]]*)\]", (src / "data.js").read_text(encoding="utf-8")):
        names |= set(re.findall(r"'([\w-]+)'", m))
    missing = [n for n in sorted(names) if not (src / "shots" / f"{n}.png").is_file()]
    if missing:
        raise SystemExit(f"web/manual/shots: missing {' '.join(missing)} (run tools/gen_manual.py)")
    if dst.exists():
        shutil.rmtree(dst)
    (dst / "shots").mkdir(parents=True)
    for f in MANUAL_FILES:
        shutil.copy(src / f, dst / f)
    for n in sorted(names):
        shutil.copy(src / "shots" / f"{n}.png", dst / "shots" / f"{n}.png")
    print(f"site: {dst}: the manual, {len(names)} screens")


def main(pkg, version, out, prev=None):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_9\d\d", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not a Felucca package (FM-1_9xx)")
    if b"FELUCCA-LOADER-1" not in raw:              # Felucca loader marker: never publish a package with vendor files
        raise SystemExit(f"{pkg}: no Felucca loader in it; the site ships only fm1pkg_make.py packages "
                         "(a package patched from an official one carries vendor files)")
    html = (HERE / "index_pkg.html").read_text(encoding="utf-8")
    lib = strip_module((HERE / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1ota.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1backup.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1diag.js").read_text(encoding="utf-8"))
    name = f"felucca-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    old = earlier(Path(prev), version, product) if prev else []
    versions = [{"version": version, "file": name, "product": product}] + [e for e, _ in old]
    editions = json.loads((HERE / "EDITIONS.json").read_text(encoding="utf-8"))
    for entry in versions:
        entry.update(editions.get(entry["version"], {}))
    # Public installer: SAMPLE first, ADDITIVE second, then stable/history.
    if not version.endswith("-preview"):
        priority = {"SAMPLE": 0, "ADDITIVE": 1}
        versions.sort(key=lambda e: priority.get(e.get("edition"), 2))
    selected = versions[0]
    meta = json.dumps({"version": selected["version"], "product": selected["product"],
                       "pkg": "../../firmware/" + selected["file"],
                       "channel": "preview" if version.endswith("-preview") else "public",
                       "versions": [{"version": e["version"], "pkg": "../../firmware/" + e["file"], "product": e["product"],
                                     "edition": e.get("edition", ""), "status": e.get("status", "")}
                                    for e in versions]})
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"index_pkg.html must contain {mark} once; update make_site.py")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta)
    inst, ed, fw = out / "webapp" / "installer", out / "webapp" / "editor", out / "firmware"
    for d in (inst, ed, fw):
        d.mkdir(parents=True, exist_ok=True)
    for f in fw.glob("felucca-*.fwsc"):            # the current package and the earlier ones kept, nothing else
        f.unlink()
    brand = (HERE / "jiant-ui.css").read_text(encoding="utf-8")
    (inst / "index.html").write_text(html.replace("/*JIANT_UI*/", brand), encoding="utf-8")
    shutil.copy(pkg, fw / name)
    for e, src in old:
        shutil.copy(src, fw / e["file"])
    (fw / "versions.json").write_text(json.dumps(versions, indent=1) + "\n", encoding="utf-8")
    lic = HERE.parent / "LICENSES"                  # the package holds JieLi SDK files (Apache-2.0): their
    (fw / "LICENSES").mkdir(exist_ok=True)          # licence travels next to it, with Felucca's own
    names = sorted(f.name for f in lic.glob("*.txt"))
    for n in names:
        shutil.copy(lic / n, fw / "LICENSES" / n)
    (fw / "LICENSES" / "index.html").write_text(    # the installer links this folder: Pages lists no folders
        '<!doctype html><meta charset="utf-8"><title>Felucca licences</title><h1>Licence texts</h1><ul>'
        + "".join(f'<li><a href="{n}">{n}</a></li>' for n in names)
        + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE (GPL-3.0)</a></p>\n',
        encoding="utf-8")
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(HERE.parent / doc, fw / doc)
    editor = (HERE / "editor.html").read_text(encoding="utf-8").replace("/*JIANT_UI*/", brand)
    (ed / "index.html").write_text(editor, encoding="utf-8")
    for f in ("fukiai.ttf", "FUKIAI-LICENSE.txt", "fm1backup.js", "factory-samples.json", "factory-voices.json", "FACTORY-VOICES.md", "FACTORY-SAMPLES.md"):
        if (HERE / f).exists():
            shutil.copy(HERE / f, ed / f)
    shutil.copy(HERE / "404.html", out / "404.html")
    (out / "index.html").write_text(
        '<!doctype html><meta charset="utf-8"><title>Felucca</title>'
        '<meta http-equiv="refresh" content="0; url=webapp/installer/">'
        '<a href="webapp/installer/">Felucca installer</a>\n', encoding="utf-8")
    print(f"site: {out}: webapp/installer ({len(html)} B), webapp/editor, firmware/{name} ({len(raw)} B, {product})"
          + (f", earlier: {' '.join(e['version'] for e, _ in old)}" if old else ""))


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "manual":
        manual(sys.argv[2])
        sys.exit(0)
    if len(sys.argv) not in (4, 5):
        sys.exit(__doc__)
    main(*sys.argv[1:5])

