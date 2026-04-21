# ESP32 Cyber Device

A handheld wireless security research platform built from scratch for my Final Year Project at South East Technological University, Carlow (BSc Hons Cybersecurity, 2025/2026).

Everything about this project is custom — the PCB was designed in EasyEDA, the enclosure was modelled in Tinkercad and printed in MJF nylon, and all firmware was written in C++. The web dashboard that controls everything is vanilla JavaScript served directly from the device.

![Device Photo](docs/screenshots/device.jpg)
<!-- Replace the above with an actual photo once you add it to the repo -->

---

## What It Does

The device creates its own Wi-Fi hotspot (`ESP32-Cyber`). Connect any phone or laptop to it and open a browser — the control dashboard loads at `192.168.4.1`. No apps, no installation, no internet needed. From there you have access to 30+ wireless security tools across Wi-Fi 802.11 and Bluetooth Low Energy.

### Confirmed Working Features

| Feature | Description |
|---|---|
| **Evil Portal** | Fake captive portal with DNS spoofing. iOS triggers the login page automatically. Captures credentials, MAC address, and User-Agent in real time. |
| **Karma Attack** | Listens for probe requests, auto-creates matching fake APs. Devices auto-connect thinking they found a known network. |
| **Beacon Spam** | Four variants: custom SSID list, random ISP-style names, Rick Roll lyrics as SSIDs, SSID confusion with homoglyphs of real nearby networks. |
| **Wardriving** | Async AP logging with CSV export. Persists in flash across reboots. |
| **BadBLE Keyboard Injection** | ESP32 advertises as a Bluetooth keyboard. Injects arbitrary keystrokes once paired. Confirmed working on Windows 11. |
| **Probe Sniffer** | Passively captures remembered SSIDs from nearby phones' probe requests. Completely passive — no transmitted frames. |
| **Station Scanner** | Passive 802.11 data frame capture to map client-AP associations. |
| **DNS Spoofing** | All DNS queries from connected devices resolve to 192.168.4.1. |
| **WiFi / BLE Scanner** | AP and BLE device discovery with OUI manufacturer identification and company ID type classification. |
| **Network / Port Scanner** | ARP discovery + TCP port scan of 21 common ports + channel analyser. |
| **BLE Name Spoof** | Advertises as any named Bluetooth device. |

### Partially Working

| Feature | Status |
|---|---|
| BLE Spam (iOS / Android / Windows) | Frames confirmed transmitting in nRF Connect. OS-level popup notifications inconsistent. |
| EAPOL Handshake Capture | Works for devices connecting to ESP32-Cyber's own AP. Not for other nearby networks. |

### Known Limitations

| Feature | Reason |
|---|---|
| Deauth Attack | ESP-IDF 5.x (arduino-esp32 3.x) explicitly blocks 0xC0/0xA0 frame types. Downgrade to 2.0.17 breaks BLE and WebServer modules. |
| USB-C as primary power | ME2108A33P boost converter (500mA max) has no headroom for 500mA ESP32 Wi-Fi peaks. Device runs from USB Micro-B port. |

---

## Hardware

| Component | Part | Cost |
|---|---|---|
| Microcontroller | ESP32-WROOM-32U DevKit V1 38-pin | €3.80 |
| Display | ILI9341 2.8" 240×320 with XPT2046 touch | €5.20 |
| Battery charger | TP4056 module with USB-C | €0.90 |
| Boost converter | ME2108A33P 3.7V → 5V | €0.80 |
| Battery | 1150mAh 3.7V LiPo JST PH 2.0mm | €4.50 |
| PCB (×5) | 2-layer 84×70mm, JLCPCB | €8.00 |
| Enclosure | MJF PA12 nylon, JLC3DP | €35.00 |
| **Total per unit** | | **~€28** |

**PCB:** 2-layer, 84mm × 70mm, designed in EasyEDA with manual trace routing. Gerber files are in `hardware/pcb/`.

**Enclosure:** MJF PA12 black nylon, two-piece (base + lid), designed in Tinkercad. STL files are in `hardware/enclosure/`.

---

## Project Structure

```
firmware/
  ESP32_UI_Template/      ← main Arduino sketch + all modules
hardware/
  pcb/                    ← EasyEDA Gerber files
  enclosure/              ← Tinkercad STL files
docs/
  screenshots/            ← device photos and dashboard screenshots
showcase/
  index.html              ← SETU showcase website
```

---

## Getting Started

See [SETUP.md](SETUP.md) for full flashing and usage instructions.

Short version:
1. Install Arduino IDE 2.x + arduino-esp32 3.3.7 board package
2. Install the libraries listed in [DEPENDENCIES.md](DEPENDENCIES.md)
3. Open `firmware/ESP32_UI_Template/ESP32_UI_Template.ino`
4. Select board: **ESP32 Dev Module**, 921600 baud, correct COM port
5. Flash
6. Connect to `ESP32-Cyber` Wi-Fi (no password)
7. Open browser → `192.168.4.1`

---

## Screenshots

| Dashboard Home | WiFi Attacks Tab | Evil Portal |
|---|---|---|
| ![Dashboard](docs/screenshots/dashboard.jpg) | ![Attacks](docs/screenshots/attacks.jpg) | ![Portal](docs/screenshots/portal.jpg) |

<!-- Add your actual screenshot files to docs/screenshots/ -->

---

## Attribution

Certain attack module implementations were adapted from open-source projects:

- **[ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder)** by justcallmekoko — EAPOL fixed-offset detection logic and deauthentication frame layout used as reference
- **[Ghost ESP](https://github.com/Spooks4576/Ghost_ESP)** by Spooks4576 — Apple Continuity Protocol BLE advertising payloads adapted for the BLE spam module

All other code — the firmware architecture, web dashboard, evil portal, karma attack, beacon spam, wardriving, network tools, enclosure design, and PCB design — is original work.

---

## Ethical Use

All development and testing was performed on personally owned hardware in a private residential environment. No attacks were directed at third-party infrastructure.

**Using this device against networks or devices you do not own or have explicit permission to test is illegal under the Computer Misuse Act 1990 and equivalent legislation in other jurisdictions.**

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

## Author

**Mateusz Kocon**  
BSc (Hons) Cybersecurity  
South East Technological University, Carlow  
Supervised by Joseph Kehoe  
Academic Year 2025/2026
