# SPDX-License-Identifier: GPL-3.0-only
"""Real Chromium, AudioWorklet and keyboard audio, with the generated WASM."""
from pathlib import Path
from functools import partial
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from threading import Thread
import shutil
from playwright.sync_api import sync_playwright

root=Path(__file__).resolve().parents[2]
site=root/"build/browser-sample-site"
if site.exists(): shutil.rmtree(site)
shutil.copytree(root/"build/emu",site/"emu")
bankdir=site/"webapp/editor";bankdir.mkdir(parents=True)
for name in ("factory-voices.json","factory-samples.json"):
    shutil.copy(root/"web"/name,bankdir/name)
server=ThreadingHTTPServer(("127.0.0.1",0),partial(SimpleHTTPRequestHandler,directory=str(site)))
Thread(target=server.serve_forever,daemon=True).start()
try:
    with sync_playwright() as p:
        browser=p.chromium.launch(executable_path=shutil.which("google-chrome") or shutil.which("chromium"),args=["--no-sandbox","--autoplay-policy=no-user-gesture-required"])
        page=browser.new_page(viewport={"width":412,"height":900})
        errors=[];page.on("pageerror",lambda e:errors.append(str(e)))
        page.add_init_script("""const Original=window.AudioWorkletNode;
window.AudioWorkletNode=class extends Original {constructor(...a){super(...a);window.__testSampleNode=this;}};""")
        page.goto(f"http://127.0.0.1:{server.server_port}/emu/")
        assert page.locator("#sample-bank").is_visible()
        page.click("#go")
        page.wait_for_function('document.querySelector("#sample-status").textContent.includes("T1 SAMPLE / KIT KEYS")',timeout=30000)
        assert "7 samples" in page.locator("#sample-status").inner_text()
        page.wait_for_timeout(4200)
        page.evaluate("""() => {const n=window.__testSampleNode;const a=n.context.createAnalyser();a.fftSize=512;n.connect(a);window.__sampleAnalyser=a;}""")
        for key in ("b","m",",","l"):
            page.keyboard.down(key)
            peak=0
            for _ in range(8):
                page.wait_for_timeout(15)
                peak=max(peak,page.evaluate("""() => {const x=new Float32Array(512);__sampleAnalyser.getFloatTimeDomainData(x);return Math.max(...x.map(Math.abs));}"""))
            page.keyboard.up(key)
            assert peak>0.00001,(key,peak)
            print("Chromium physical key",key,"peak",peak,flush=True)
            page.wait_for_timeout(850)
        assert not errors,errors
        page.reload()
        page.click("#go")
        page.wait_for_function('document.querySelector("#sample-status").textContent.includes("7 samples")',timeout=30000)
        page.click("#sample-play")
        page.wait_for_function('document.querySelector("#sample-status").textContent.includes("T1 SAMPLE / KIT KEYS")',timeout=5000)
        print("PASS Chromium: fresh bank loads, real keyboard audio reaches analyser, saved bank reloads and direct kit button works",flush=True)
        browser.close()
finally:
    server.shutdown()
