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
  </style>
</head>
<body>
  <h1>EzClock</h1>
  <div class="row">
    <label>Color: <input id="color" type="color" value="#6633FF"></label>
    <button onclick="setColor()">Set</button>
  </div>
  <script>
    async function setColor(){
      const hex = document.getElementById('color').value;
      try { await fetch('/api/color?hex=' + encodeURIComponent(hex)); } catch(e){}
    }
  </script>
</body>
</html>
)HTML";
