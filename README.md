# EzClock ⏰

**EzClock** is a modular, Wi-Fi-connected clock firmware designed for ESP32 and ESP32-C3 boards. 
It is designed first to be used with XIAO-ESP32-C3 board. 
It provides a clean and extensible base for any “nerdy” or LED-based clock project — without being locked into YAML or a specific platform.

### ✨ Features
- 🕒 **Automatic time sync** via NTP
- 🔧 **Web configuration UI** (color, brightness, Wi-Fi, etc.)
- 🌐 **MQTT / Home Assistant discovery** support
- 📡 **OTA updates** (via browser or network)
- 🔌 **Hardware abstraction layer** for custom LED or display drivers

### 🧩 Modular architecture
The firmware separates the **core system** (connectivity, OTA, MQTT, NTP)  
from the **hardware layer**, making it easy to adapt to any display technology —  
LED strips, 7-segment digits, word clocks, matrices, or even analog steppers.

### 🚀 Goals
EzClock aims to be a **reusable base** for all your clock projects:
- Easy to flash and configure (compatible with [ESP Web Tools](https://esphome.github.io/esp-web-tools/))
- Hackable in C++ (no YAML pain)
- Ready for integration into Home Assistant or any MQTT automation system

### 🧠 Example projects
- WS2812 “segment” clock
- Word clock with color themes
- Minimal wall clock with brightness auto-adjust

---

💡 **EzClock** is designed for makers who like control, flexibility,  
and beautiful, connected clocks — without reinventing the Wi-Fi stack every time.


Platformio update OTA : C:\Users\alexis\.platformio\penv\Scripts\platformio.exe run -e esp32c3-ota --target upload --upload-port ezclock.local
