// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// (JIANT 0.6.7) docs/cheatsheet/*.html (tools/gen_cheatsheets.py) -> docs/cheatsheet-*.png (the README) and .pdf (to
// download), with the Chromium Playwright brings:  node tools/render_cheatsheets.mjs
import { createRequire } from "module";
const require = createRequire(import.meta.url);
const { chromium } = require("playwright");   // (NODE_PATH=$(npm root -g) for a global install)
import { fileURLToPath } from "url";
import path from "path";
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const exe = process.env.CHROMIUM || "/opt/pw-browsers/chromium-1194/chrome-linux/chrome";
const browser = await chromium.launch({ executablePath: exe }).catch(() => chromium.launch());
for (const name of ["controls", "sound"]) {
  const page = await browser.newPage({ viewport: { width: 1600, height: 1000 }, deviceScaleFactor: 1.5 });
  await page.goto("file://" + path.join(root, "docs/cheatsheet", name + ".html"));
  await page.evaluate(() => document.fonts.ready);
  const h = await page.evaluate(() => document.documentElement.scrollHeight);
  await page.setViewportSize({ width: 1600, height: h });
  await page.screenshot({ path: path.join(root, `docs/cheatsheet-${name}.png`), fullPage: true });
  await page.pdf({ path: path.join(root, `docs/cheatsheet-${name}.pdf`), width: "1600px", height: h + 2 + "px",
                   printBackground: true, pageRanges: "1" });
  console.log(name, h, "px");
}
await browser.close();
