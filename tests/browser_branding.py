# SPDX-License-Identifier: GPL-3.0-only
"""Real generated pages: visual artifacts, phone overflow and editor navigation."""
from pathlib import Path
from urllib.parse import urlparse
import os, shutil
from playwright.sync_api import sync_playwright
root=Path(__file__).resolve().parents[1]
site=root/'build/branding-site'
shots=root/'build/branding-screenshots';shots.mkdir(parents=True,exist_ok=True)
with sync_playwright() as p:
 browser=p.chromium.launch(executable_path=os.environ.get('CHROMIUM_PATH') or shutil.which('google-chrome'),headless=True,args=['--no-sandbox'])
 page=browser.new_page(viewport={'width':1280,'height':900},color_scheme='light')
 errors=[];page.on('pageerror',lambda e:(errors.append(str(e)),print('PAGE ERROR',e)))
 def route(r):
  path=urlparse(r.request.url).path.lstrip('/')
  f=site/path
  if not f.is_file(): r.fulfill(status=404,body='');return
  body=f.read_bytes()
  if path=='webapp/editor/index.html':
   body=body.decode();at=body.rfind('})();');body=(body[:at]+'\nwindow.__test={makeMockDevice,getDevice:()=>dev};\n'+body[at:]).encode()
  mime='text/html' if path.endswith('html') else 'application/javascript' if path.endswith('js') else 'application/json' if path.endswith('json') else 'font/ttf' if path.endswith('ttf') else 'application/octet-stream'
  r.fulfill(status=200,body=body,content_type=mime)
 page.route('https://jiant.test/**',route)
 def capture(name,width):
  page.set_viewport_size({'width':width,'height':900});page.wait_for_timeout(200)
  result=page.evaluate('''() => ({width:document.documentElement.scrollWidth,screen:innerWidth,bg:getComputedStyle(document.body).backgroundColor})''')
  if result['width']>width+1:
   print('OVERFLOW',page.evaluate('''() => [...document.querySelectorAll('main *')].filter(e=>e.getBoundingClientRect().right>innerWidth+1).slice(0,10).map(e=>[e.tagName,e.id,e.className,e.getBoundingClientRect().right])'''))
  assert result['width']<=width+1,(name,result)
  assert result['bg']=='rgb(5, 4, 9)',(name,result)
  page.screenshot(path=str(shots/f'{name}-{width}.png'),full_page=True)
 page.goto('https://jiant.test/webapp/editor/index.html#projects')
 page.wait_for_function('!!window.__test')
 page.evaluate('''() => {const m=__test.makeMockDevice({auto:false});Object.defineProperty(navigator,'requestMIDIAccess',{value:async()=>m.access});}''')
 page.click('#connect');page.wait_for_function('__test.getDevice()?.dump && !document.querySelector("#connect").disabled',timeout=30000)
 for tab in ['projects','sound','tracks','sequencer']:
  page.click(f'button[data-tab="{tab}"]');page.wait_for_timeout(150)
  for width in [360,412,1280]: capture('editor-'+tab,width)
 page.click('button[data-tab="sound"]')
 assert page.locator('#smpfactory').is_visible()
 page.keyboard.press('Tab')
 page.goto('https://jiant.test/webapp/installer/index.html')
 page.wait_for_function('document.querySelector("#ver").options.length > 0')
 for width in [360,412,1280]:
  capture('installer',width)
  assert page.locator('#go').bounding_box()['height']>=44
 assert not errors,errors
 print('branding: editor Projects/Sound/Tracks/Sequencer and installer at 360/412/1280 px; no root overflow or JS errors PASS')
 browser.close()
