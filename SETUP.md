# Setup Guide

How to flash the firmware and use the ESP32 Cyber Device.

---

## What You Need

**Hardware:**
- ESP32-WROOM-32U development board (or the assembled custom PCB)
- USB Micro-B cable (for flashing and power)
- A phone or laptop to access the dashboard

**Software:**
- Arduino IDE 2.3.2+ — https://www.arduino.cc/en/software
- All libraries from [DEPENDENCIES.md](DEPENDENCIES.md) installed

---

## Step 1: Install Arduino IDE and Board Package

1. Download and install Arduino IDE from https://www.arduino.cc/en/software
2. Open Arduino IDE
3. Go to **File → Preferences**
4. In "Additional Boards Manager URLs" add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
5. Go to **Tools → Board → Boards Manager**
6. Search "esp32 by Espressif Systems"
7. Find **version 3.3.7** in the dropdown and install it

---

## Step 2: Install Libraries

Go to **Tools → Manage Libraries** and install each of these:

| Library | Version | Search Term |
|---|---|---|
| TFT_eSPI | 2.5.43 | `TFT_eSPI` |
| NimBLE-Arduino | 1.4.2 | `NimBLE-Arduino` |
| ArduinoJson | 6.21.4 | `ArduinoJson` |

For each one, make sure to select the exact version from the dropdown rather than just clicking Install.

---

## Step 3: Configure TFT_eSPI

TFT_eSPI requires a custom pin configuration file. After installing the library:

1. Find the library folder. On Windows it's usually:
   ```
   C:\Users\YourName\Documents\Arduino\libraries\TFT_eSPI\
   ```
2. Open the file `User_Setup.h` in that folder with a text editor
3. Delete everything in it and paste in this configuration:

```cpp
#define USER_SETUP_ID 1
#define ILI9341_DRIVER

#define TFT_CS   17
#define TFT_DC   16
#define TFT_RST  -1
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_MISO 19
#define TFT_BL   32
#define TOUCH_CS 21

#define SPI_FREQUENCY        40000000
#define SPI_READ_FREQUENCY   20000000
#define SPI_TOUCH_FREQUENCY   2500000

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define SMOOTH_FONT
```

4. Save the file

> If you skip this step the display will either not work or show garbled output. This is the most common setup mistake.

---

## Step 4: Open the Sketch

1. Clone or download this repository
2. Open Arduino IDE
3. Go to **File → Open**
4. Navigate to `firmware/ESP32_UI_Template/`
5. Open `ESP32_UI_Template.ino`

The sketch directory should contain all the `.cpp` and `.h` files alongside the `.ino` file. If Arduino IDE only shows the `.ino` file, make sure you opened the folder correctly — all source files need to be in the same directory.

---

## Step 5: Select Board and Port

In the Tools menu:

| Setting | Value |
|---|---|
| Board | ESP32 Dev Module |
| Upload Speed | 921600 |
| CPU Frequency | 240MHz (WiFi/BT) |
| Flash Size | 4MB (32Mb) |
| Partition Scheme | Default 4MB with spiffs |
| Port | Your ESP32's COM port |

To find the correct COM port:
- **Windows:** Device Manager → Ports (COM & LPT) — look for "Silicon Labs CP210x" or "CH340"
- **Mac:** `/dev/cu.SLAB_USBtoUART` or similar
- **Linux:** `/dev/ttyUSB0` or `/dev/ttyACM0`

If no port appears, you may need to install a driver:
- **CP2102 chip:** https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
- **CH340 chip:** https://sparks.gogo.co.nz/ch340.html

---

## Step 6: Flash

1. Hold the **BOOT button** on the ESP32 board
2. Click the **Upload** button (→) in Arduino IDE
3. Release the BOOT button when you see "Connecting..." in the output
4. Wait for "Hard resetting via RTS pin..." — this means it's done

If you get a "Failed to connect" error, try again. Some boards need the BOOT button held more precisely. Some need the EN (reset) button pressed briefly while BOOT is held.

---

## Step 7: Connect and Use

1. The ESP32 will reboot after flashing. The TFT display should show a boot screen.
2. On your phone or laptop, open Wi-Fi settings
3. Connect to the network: **`ESP32-Cyber`** (no password)
4. Open a browser and go to: **`http://192.168.4.1`**
5. Accept the legal disclaimer — read it, it matters
6. The dashboard loads. All features are accessible from the tabs at the top.

> If the page doesn't load, make sure you typed `http://` not `https://`. Some browsers auto-add https and the ESP32 doesn't have TLS.

---

## Using the Dashboard

**Tabs:**
- **Home** — live stats (networks found, BLE devices, battery %, connected clients)
- **WiFi** — Wi-Fi scanner results
- **BLE** — BLE device scanner
- **Network** — ARP scanner, port scanner, channel analyser
- **WiFi Attacks** — evil portal, karma, beacon spam, deauth, auth flood, SSID confusion
- **BT Attacks** — BLE spam, BadBLE keyboard injection, game controller spam, AirDrop spam
- **Wardrive** — wardriving AP log, export CSV
- **Defence** — EAPOL sniffer, rogue AP detector
- **Logs** — timestamped event log

**Key things to know:**
- Only one attack runs at a time — starting a new one stops the current one automatically
- After stopping an attack, the dashboard refreshes state immediately
- The evil portal's admin view is at `http://192.168.4.1/admin` while the portal is running
- Wardriving data persists in flash — it survives power off. Use the Clear button to reset it.
- Serial Monitor at 115200 baud shows debug output including TX OK/FAIL for frame injection

---

## Troubleshooting

**Dashboard won't load:**
- Make sure you're connected to `ESP32-Cyber`
- Try `http://192.168.4.1` not `https://192.168.4.1`
- Try a different browser

**TFT display shows nothing / white screen:**
- Check your `User_Setup.h` configuration matches Step 3 exactly
- Verify the display is wired to the correct GPIO pins
- The display requires 3.3V logic — never connect to 5V

**Compilation errors on "FS not declared":**
Add these three lines at the very top of `ESP32_UI_Template.ino` before all other includes:
```cpp
#include <FS.h>
#include <LittleFS.h>
using namespace fs;
```

**Deauth attack shows "unsupport frame type 0c0":**
This is expected on arduino-esp32 3.x. ESP-IDF 5.x blocks deauth/disassoc frame injection. There is no fix that doesn't break other features. The limitation is documented in the README.

**Random reboots during operation:**
Check that your ArduinoJson document size is at least 32768 bytes. If you modified the firmware, make sure no `StaticJsonDocument` allocations are smaller than 32KB.

**BLE features not transmitting:**
Make sure NimBLE-Arduino 1.4.2 is installed (not a different BLE library). Check Serial Monitor for NimBLE initialisation errors.

---

## Serial Monitor Debug Output

Connect at **115200 baud**. Useful messages to watch for:

```
[ATTACK] BEACON_LIST: STARTED       ← beacon spam running
[BeaconRandom] TX OK — 60 frames    ← random beacon transmitting
[EvilPortal] Credential captured    ← credential captured
[EAPOL] M1 captured from AP         ← handshake M1 received
[Deauth] TX FAIL — unsupport frame  ← expected on 3.x core
```

If you see `[BeaconRandom] TX FAIL` or similar after every frame, check that `en_sys_seq=true` is being passed to `esp_wifi_80211_tx`. The firmware already has this fixed — if you're seeing it in modified code, that's the likely cause.

---

## Building the Hardware

If you want to build the physical device rather than just running the firmware on a development board:

**PCB:**
- Gerber files are in `hardware/pcb/`
- Upload to JLCPCB: 2 layers, 1.6mm, HASL lead-free, green soldermask, white silkscreen, min qty 5
- Component values and GPIO assignments are in [DEPENDENCIES.md](DEPENDENCIES.md)

**Enclosure:**
- STL files are in `hardware/enclosure/`
- Order from JLC3DP using Multi-Jet Fusion (MJF) in PA12 black nylon for best result
- FDM printing works but layer lines are visible and the part is weaker

**Power note:**
The PCB's USB-C charging circuit (TP4056 → ME2108A33P) currently doesn't reliably power the full device under Wi-Fi load — the boost converter hits its 500mA limit during transmit bursts. The device runs from the ESP32's USB Micro-B port from a USB power bank. This is a known hardware issue documented in the project report. A future revision will use the TPS63020 (1.8A capable).
