#pragma once
#include <pgmspace.h>

static const char WEB_UI[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>EzClock</title>
  <style>
    body { font-family: system-ui, Segoe UI, Arial, sans-serif; margin: 2rem; color: #222; }
    .row { margin: .75rem 0; }
    input[type=color] { width: 3rem; height: 2rem; border: 1px solid #ccc; }
    button { padding: .4rem .8rem; }
    fieldset { border: 1px solid #ddd; padding: 1rem; border-radius: 8px; }
    legend { padding: 0 .5rem; color: #555; }
    label { display: inline-block; min-width: 7rem; }
    input[type=text], input[type=password] { padding: .3rem .5rem; }
  </style>
</head>
<body>
  <h1>EzClock</h1>
  <div class="row">
    <strong>Color (HSV)</strong>
    <div class="row">
      <label for="h">Hue</label>
      <input id="h" type="range" min="0" max="360" value="270" oninput="updatePreview()">
      <span id="h_val">270</span>
    </div>
    <div class="row">
      <label for="s">Saturation</label>
      <input id="s" type="range" min="0" max="100" value="60" oninput="updatePreview()">
      <span id="s_val">60</span>
    </div>
    <div class="row">
      <label for="v">Value</label>
      <input id="v" type="range" min="0" max="100" value="100" oninput="updatePreview()">
      <span id="v_val">100</span>
    </div>
    <div class="row" style="display:flex;align-items:center;gap:1rem;">
      <div id="preview" style="width:42px;height:24px;border:1px solid #aaa;border-radius:4px;"></div>
      <code id="hex">#6633FF</code>
      <button onclick="setColor()">Set</button>
      <button onclick="saveDefaultColor()" title="Save current color as default and persist">Save as default</button>
    </div>
    <div class="row" style="display:flex;gap:.5rem;flex-wrap:wrap;">
      <button onclick="preset(0,100,100)">Red</button>
      <button onclick="preset(120,100,100)">Green</button>
      <button onclick="preset(240,100,100)">Blue</button>
      <button onclick="preset(330,40,100)">Pink</button>
      <button onclick="preset(60,100,100)">Yellow</button>
    </div>
    <div class="row" style="display:flex;align-items:center;gap:.5rem;">
      <label for="fade">Smoothing (ms)</label>
      <input id="fade" type="number" min="0" max="5000" step="50" value="300" style="width:6rem;" />
      <button onclick="saveFade()">Apply</button>
    </div>
  </div>
  <fieldset class="row">
    <legend>Wi‑Fi</legend>
    <div class="row" style="display:flex;align-items:center;gap:.5rem;flex-wrap:wrap;">
      <label for="ssid">SSID</label>
      <input id="ssid" type="text" placeholder="YourWiFi" oninput="syncSsidInput()">
      <select id="ssidList" onchange="selectSsid(this.value)"><option value="">-- scanning... --</option></select>
      <button type="button" onclick="scanSsids()">Scan</button>
    </div>
    <div class="row"><label for="pwd">Password</label> <input id="pwd" type="password" placeholder="••••••••"></div>
    <div class="row"><button onclick="saveWifi()">Save Wi‑Fi</button></div>
    <small>Device stays in AP+STA; after saving, it will try to connect to your Wi‑Fi.</small>
  </fieldset>
  <fieldset class="row">
    <legend>Time</legend>
    <div class="row"><label for="tz">Timezone</label> <input id="tz" type="text" placeholder="UTC0 or CET-1CEST,M3.5.0,M10.5.0/3"></div>
    <div class="row"><button onclick="saveTz()">Save Timezone</button></div>
    <small>POSIX TZ format. Examples: UTC0, CET-1CEST,M3.5.0,M10.5.0/3</small>
  </fieldset>
  <script>
    function hsvToRgb(h, s, v){
      s /= 100; v /= 100;
      const c = v * s;
      const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
      const m = v - c;
      let rp=0,gp=0,bp=0;
      if (0<=h && h<60){ rp=c; gp=x; bp=0; }
      else if (60<=h && h<120){ rp=x; gp=c; bp=0; }
      else if (120<=h && h<180){ rp=0; gp=c; bp=x; }
      else if (180<=h && h<240){ rp=0; gp=x; bp=c; }
      else if (240<=h && h<300){ rp=x; gp=0; bp=c; }
      else { rp=c; gp=0; bp=x; }
      const r = Math.round((rp + m) * 255);
      const g = Math.round((gp + m) * 255);
      const b = Math.round((bp + m) * 255);
      return {r,g,b};
    }

    function rgbToHex(r,g,b){
      return '#' + [r,g,b].map(x => x.toString(16).padStart(2,'0')).join('').toUpperCase();
    }

    function hexToRgb(hex){
      if (hex.startsWith('#')) hex = hex.slice(1);
      if (hex.length !== 6) return null;
      const v = parseInt(hex, 16);
      return { r: (v>>16)&0xFF, g: (v>>8)&0xFF, b: v&0xFF };
    }

    function rgbToHsv(r,g,b){
      r/=255; g/=255; b/=255;
      const max=Math.max(r,g,b), min=Math.min(r,g,b);
      const d = max-min;
      let h=0, s = max===0 ? 0 : d/max, v = max;
      if (d !== 0){
        switch(max){
          case r: h = (g-b)/d + (g<b?6:0); break;
          case g: h = (b-r)/d + 2; break;
          case b: h = (r-g)/d + 4; break;
        }
        h *= 60;
      }
      return { h: Math.round(h), s: Math.round(s*100), v: Math.round(v*100) };
    }

    function getHSV(){
      const h = parseInt(document.getElementById('h').value,10);
      const s = parseInt(document.getElementById('s').value,10);
      const v = parseInt(document.getElementById('v').value,10);
      return {h,s,v};
    }

    function setHSV(h,s,v){
      document.getElementById('h').value = h;
      document.getElementById('s').value = s;
      document.getElementById('v').value = v;
      updatePreview();
    }

    function preset(h,s,v){
      console.log('[UI] preset', {h,s,v});
      setHSV(h,s,v);
      setColor();
    }

    function updatePreview(){
      const {h,s,v} = getHSV();
      document.getElementById('h_val').textContent = h;
      document.getElementById('s_val').textContent = s;
      document.getElementById('v_val').textContent = v;
      const {r,g,b} = hsvToRgb(h,s,v);
      const hex = rgbToHex(r,g,b);
      const prev = document.getElementById('preview');
      prev.style.background = hex;
      document.getElementById('hex').textContent = hex;
    }

    async function setColor(){
      const {h,s,v} = getHSV();
      const {r,g,b} = hsvToRgb(h,s,v);
      const hex = rgbToHex(r,g,b);
      console.log('[UI] setColor HSV', {h,s,v}, 'HEX', hex);
      try {
        const res = await fetch('/api/color?hex=' + encodeURIComponent(hex));
        console.log('[UI] /api/color status', res.status);
      } catch(e) {
        console.error('[UI] /api/color error', e);
      }
    }

    async function saveDefaultColor(){
      const {h,s,v} = getHSV();
      const {r,g,b} = hsvToRgb(h,s,v);
      const hex = rgbToHex(r,g,b);
      console.log('[UI] saveDefaultColor', hex);
      const body = new URLSearchParams({ hex });
      try {
        const res = await fetch('/api/color/default', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body });
        console.log('[UI] /api/color/default', res.status);
        alert(res.ok ? 'Default color saved.' : 'Failed to save default color');
      } catch(e) {
        console.error('[UI] /api/color/default error', e);
        alert('Error while saving default color');
      }
    }

    async function saveFade(){
      const ms = document.getElementById('fade').value;
      const body = new URLSearchParams({ ms });
      try {
        const res = await fetch('/api/fade', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body });
        console.log('[UI] /api/fade', res.status);
        alert(res.ok ? 'Smoothing updated.' : 'Failed to update smoothing');
      } catch(e) {
        console.error('[UI] /api/fade error', e);
        alert('Error while updating smoothing');
      }
    }

    async function loadStatus(){
      try {
        const res = await fetch('/api/status');
        if (!res.ok) return;
        const js = await res.json();
        if (js && js.led){
          const rgb = hexToRgb(js.led.hex);
          if (rgb){
            const hsv = rgbToHsv(rgb.r, rgb.g, rgb.b);
            setHSV(hsv.h, hsv.s, hsv.v);
          }
          if (typeof js.led.fade === 'number'){
            document.getElementById('fade').value = js.led.fade;
          }
        }
      } catch(e) {
        console.warn('[UI] loadStatus error', e);
      }
    }

  // initialize preview on load and fetch status
    updatePreview();
    loadStatus();
  // Kick off initial Wi‑Fi scan
  scanSsids();

    async function saveWifi(){
      const ssid = document.getElementById('ssid').value.trim();
      const password = document.getElementById('pwd').value;
      console.log('[UI] saveWifi', {ssid, hasPassword: password.length>0});
      const body = new URLSearchParams({ssid, password});
      try {
        const res = await fetch('/api/wifi', { method: 'POST', headers: { 'Content-Type':'application/x-www-form-urlencoded' }, body });
        const text = await res.text();
        console.log('[UI] /api/wifi', res.status, text);
        alert(res.ok ? 'Wi‑Fi saved. Device will try to connect.' : 'Failed to save Wi‑Fi: ' + text);
      } catch(e) {
        console.error('[UI] /api/wifi error', e);
        alert('Error while saving Wi‑Fi');
      }
    }

    function syncSsidInput(){
      const v = document.getElementById('ssid').value;
      const sel = document.getElementById('ssidList');
      // If input matches an option, select it; else select none
      let matched = false;
      for (const opt of sel.options){
        if (opt.value === v){ sel.value = v; matched = true; break; }
      }
      if (!matched) sel.value = '';
    }

    function selectSsid(v){
      document.getElementById('ssid').value = v;
    }

    async function scanSsids(){
      const sel = document.getElementById('ssidList');
      sel.innerHTML = '<option value="">-- scanning... --</option>';
      try {
        const res = await fetch('/api/wifi/scan');
        if (!res.ok){ sel.innerHTML = '<option value="">scan failed</option>'; return; }
        const js = await res.json();
        const list = Array.isArray(js.list) ? js.list : [];
        // Sort by RSSI descending
        list.sort((a,b)=> (b.rssi||-999) - (a.rssi||-999));
        // Build options
        let opts = '<option value="">-- select SSID --</option>';
        for (const ap of list){
          const ssid = ap.ssid || '';
          if (!ssid) continue;
          const rssi = ap.rssi;
          const enc = ap.enc;
          const strength = (rssi>=-55 ? '🟢' : rssi>=-70 ? '🟡' : '🟠');
          opts += `<option value="${ssid}">${strength} ${ssid} (${rssi} dBm)</option>`;
        }
        sel.innerHTML = opts || '<option value="">(no networks)</option>';
        // Re-apply selection if input already has value
        syncSsidInput();
      } catch(e) {
        console.warn('[UI] scanSsids error', e);
        sel.innerHTML = '<option value="">scan error</option>';
      }
    }

    async function saveTz(){
      const tz = document.getElementById('tz').value.trim();
      console.log('[UI] saveTz', tz);
      const body = new URLSearchParams({ tz });
      try {
        const res = await fetch('/api/timezone', { method: 'POST', headers: { 'Content-Type':'application/x-www-form-urlencoded' }, body });
        const text = await res.text();
        console.log('[UI] /api/timezone', res.status, text);
        alert(res.ok ? 'Timezone saved.' : 'Failed to save timezone: ' + text);
      } catch(e) {
        console.error('[UI] /api/timezone error', e);
        alert('Error while saving timezone');
      }
    }
  </script>
</body>
</html>
)HTML";
