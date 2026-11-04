// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// The manual offline: its page, scripts and screens cached (network first for the page and scripts, so a new version
// shows at once; cache first for the screens). Only the manual's own folder; the emulator needs the network.
const CACHE = "jiant-manual-v1";
const CORE = ["./", "index.html", "manual.js", "data.js", "nav.js"];
self.addEventListener("install", (e) => { e.waitUntil(caches.open(CACHE).then((c) => c.addAll(CORE)).then(() => self.skipWaiting())); });
self.addEventListener("activate", (e) => {
  e.waitUntil(caches.keys().then((ks) => Promise.all(ks.filter((k) => k !== CACHE).map((k) => caches.delete(k)))).then(() => self.clients.claim()));
});
self.addEventListener("fetch", (e) => {
  const u = new URL(e.request.url);
  if (e.request.method !== "GET" || u.origin !== location.origin || !u.pathname.startsWith(new URL("./", location).pathname)) return;
  const put = (r) => { if (r.ok) { const c = r.clone(); caches.open(CACHE).then((x) => x.put(e.request, c)); } return r; };
  if (u.pathname.includes("/shots/"))
    e.respondWith(caches.match(e.request).then((m) => m || fetch(e.request).then(put)));
  else
    e.respondWith(fetch(e.request).then(put).catch(() => caches.match(e.request)));
});
