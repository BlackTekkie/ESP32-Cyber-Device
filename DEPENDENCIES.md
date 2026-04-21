# Dependencies

Exact library and framework versions required to build the firmware. Using different versions — especially a different arduino-esp32 version — may break certain features. See notes below.

---

## Arduino IDE

**Arduino IDE 2.3.2** or later  
Download: https://www.arduino.cc/en/software

---

## Board Package — arduino-esp32

**Version: 3.3.7**

Install via Arduino IDE Board Manager:
1. Go to **File → Preferences**
2. Add this URL to "Additional Boards Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Go to **Tools → Board → Boards Manager**
4. Search "esp32 by Espressif Systems"
5. Install **version 3.3.7** specifically — do not just click "Install Latest"

> ⚠️ **Important version note:** arduino-esp32 3.x is based on ESP-IDF 5.x, which blocks deauthentication and disassociation (0xC0/0xA0) frame injection via `esp_wifi_80211_tx`. All other features work correctly on 3.3.7. If you need deauth to work, you would need to use **2.0.17** — but this breaks the BLE stack and WebServer module APIs.

---

## Libraries

Install all of these via **Tools → Manage Libraries** in Arduino IDE.

### TFT_eSPI by Bodmer
**Version: 2.5.43**

Search: `TFT_eSPI`  
Author: Bodmer

After installing, you **must** configure it by editing the `User_Setup.h` file inside the library folder. See [SETUP.md](SETUP.md) for the exact configuration to copy in.

> The default `User_Setup.h` will not work — it's configured for a different pinout.

---

### NimBLE-Arduino
**Version: 1.4.2**

Search: `NimBLE-Arduino`  
Author: h2zero

Used for all Bluetooth Low Energy functionality — BLE spam, BadBLE keyboard injection, BLE scanner, BLE name spoof.

> Note: Do not use the Arduino built-in BluetoothSerial or ESP32 BLE Arduino library — NimBLE is required for the raw GAP API calls this firmware uses.

---

### ArduinoJson
**Version: 6.21.4**

Search: `ArduinoJson`  
Author: Benoit Blanchon

Used for parsing HTTP command payloads and serialising device state to JSON for the dashboard.

> Use version 6.x. Version 7.x has breaking API changes that will cause compilation errors.

---

## Board Settings

When you have the ESP32 board package installed, select these settings in Tools menu:

| Setting | Value |
|---|---|
| Board | ESP32 Dev Module |
| Upload Speed | 921600 |
| CPU Frequency | 240MHz (WiFi/BT) |
| Flash Frequency | 80MHz |
| Flash Mode | QIO |
| Flash Size | 4MB (32Mb) |
| Partition Scheme | Default 4MB with spiffs |
| Core Debug Level | None |
| PSRAM | Disabled |
| Port | Your ESP32's COM port |

---

## Checking Your Versions

After installing everything, you can verify versions in Arduino IDE:

- **Board package:** Tools → Board → Boards Manager → search "esp32"
- **Libraries:** Tools → Manage Libraries → filter by "Installed"

If you see compilation errors after a fresh install, double-check every version matches the table above before debugging anything else.
