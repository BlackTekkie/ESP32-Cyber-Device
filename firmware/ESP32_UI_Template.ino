/**
 * ESP32 Cyber Device v2.0 — Main Sketch
 * =======================================
 * This is the entry point of the whole project. In Arduino, setup() runs once
 * on boot and loop() runs forever after that.
 *
 * This file does three main things:
 *  1. Sets up all the hardware and modules on startup (setup function)
 *  2. Handles touch input on the physical TFT screen (loop function)
 *  3. Processes commands that arrive from the web browser (handleWebCmd)
 *
 * Every feature (beacon spam, evil portal, BLE attacks etc.) lives in its own
 * separate .cpp/.h file. This file just creates one object for each feature
 * and calls begin() once and tick() every loop iteration.
 */

// Tell GCC compiler to optimise for speed - reduces latency in attack tick functions
#pragma GCC optimize("O2")
#pragma GCC optimize("unroll-loops")

// ── Pin Definitions ──────────────────────────────────────────────────────────
#define DEVICE_PIN  "1234"   // Default PIN to unlock the web UI - CHANGE THIS before deployment
#define TFT_BL      32       // GPIO pin that controls the TFT backlight (HIGH = on)
#define BATTERY_PIN 34       // GPIO pin connected to the voltage divider for battery reading

// ── System Includes ──────────────────────────────────────────────────────────
#include <esp_system.h>      // ESP32 system functions (restart, heap info etc.)

// Fix: ESP32 Arduino 3.x puts FS class inside fs:: namespace but WebServer.h
// expects it in global scope. Force FS.h first and expose the namespace.
#include <FS.h>
#include <LittleFS.h>
using namespace fs;

// ── Our own module headers ────────────────────────────────────────────────────
// Each .h file declares a class for one piece of functionality
#include "display.h"         // ILI9341 TFT touchscreen drawing functions
#include "wifi_viewer.h"     // Scans for nearby WiFi networks
#include "ble_viewer.h"      // Scans for nearby Bluetooth LE devices
#include "network_scanner.h" // ARP scan to find connected clients on our AP
#include "ap_manager.h"      // Manages clients connected to our hotspot
#include "port_scanner.h"    // TCP port scanner for a given IP address
#include "channel_scanner.h" // Analyses which WiFi channels are busy/free
#include "web_ui.h"          // The HTTP server that serves the web dashboard
#include "cyber_storage.h"   // LittleFS flash storage for credentials and settings
#include "dns_spoof.h"       // DNS server that redirects all DNS queries to us
#include "ota_update.h"      // Over-the-air firmware update via Arduino OTA
// LittleFS already included above
#include "ui_states.h"       // Enum definitions for the TFT display state machine

// ── WiFi Attack Module Includes ───────────────────────────────────────────────
#include "beacon_spam_list.h"    // Broadcasts user-supplied SSIDs as fake networks
#include "beacon_spam_random.h"  // Floods spectrum with randomly-named fake APs
#include "evil_portal.h"         // Fake captive portal login page (port 8080)
#include "rick_roll_beacon.h"    // Broadcasts Rick Astley lyrics as WiFi SSIDs
#include "probe_flood.h"         // Floods the air with WiFi probe request frames
#include "ap_clone_spam.h"       // Clones and rebroadcasts nearby AP identities
#include "karma_attack.h"        // Responds to probe requests with matching fake APs
#include "ble_spam.h"            // Apple iOS BLE popup spam (Continuity Protocol)
#include "bad_ble.h"             // BLE HID keyboard injection (BadUSB over Bluetooth)
#include "ble_spam_all.h"        // Multi-platform BLE spam (Apple + Android + Windows)
#include "eapol_sniffer.h"       // Captures WPA2 4-way handshake frames
#include "pineapple_detector.h"  // Detects rogue APs and WiFi Pineapple devices
#include "chromecast_attack.h"   // Discovers and Rick-Rolls Chromecast devices
#include "ssid_confusion.h"      // Creates confusingly similar SSIDs to real networks
#include "deauth_attack.h"       // Sends 802.11 deauthentication frames
#include "auth_flood.h"          // Floods target AP with auth requests
#include "pmkid_capture.h"       // Passively captures WPA2 PMKID hashes
#include "wifi_cracker.h"        // On-device dictionary attack against PMKID hashes
#include "ble_attacks_extra.h"   // AirDrop spam, tracker detector, controller spam
#include "wardriving.h"
#include "station_scanner.h"
#include "probe_sniffer.h"          // Passive AP logger with CSV export

// ── Global State Variable ─────────────────────────────────────────────────────
// Tracks which screen the physical TFT display is currently showing
// The web UI is separate and has its own state - this is just for the screen
UiState currentState = UI_MENU; // No PIN — start on menu directly

// ── Module Instances ──────────────────────────────────────────────────────────
// One object per module - these are created globally so every function can use them
// Creating them globally means they exist for the entire lifetime of the program
Display        display;         // Handles all drawing on the 2.8" TFT screen
WiFiViewer     wifiViewer;      // Manages WiFi scanning
BLEViewer      bleViewer;       // Manages BLE scanning
NetworkScanner netScanner;      // Scans for IP hosts on our AP subnet
APManager      apManager;       // Lists and manages connected clients
PortScanner    portScanner;     // TCP port scanner
ChannelScanner channelScanner;  // Channel utilisation analysis
WebUI          webUI;           // HTTP server + web dashboard

// ── Attack Module Instances ───────────────────────────────────────────────────
// Same pattern - one global object per attack module
BeaconSpamList    beaconSpamList;    // Custom SSID beacon spammer
BeaconSpamRandom  beaconSpamRandom;  // Random SSID beacon spammer
EvilPortal        evilPortal;        // Captive portal attack
RickRollBeacon    rickRollBeacon;    // Rick Roll SSID broadcaster
ProbeFlood        probeFlood;        // WiFi probe flood
APCloneSpam       apCloneSpam;       // AP cloning attack
KarmaAttack       karmaAttack;       // Karma AP impersonation attack
BLESpam           bleSpam;           // iOS BLE spam
BadBLE            badBLE;            // Bluetooth keyboard injection
BLESpamAll        bleSpamAll;        // Multi-platform BLE spam
EapolSniffer      eapolSniffer;      // WPA2 handshake capture
PineappleDetector pineappleDetector; // Rogue AP detection
ChromecastAttack  chromecastAttack;  // Chromecast discovery & attack
SSIDConfusion     ssidConfusion;     // SSID confusion attack
DeauthAttack      deauthAttack;      // 802.11 deauthentication
Wardriving        wardriving;        // Passive AP logger
AuthFlood         authFlood;         // Authentication flood
PMKIDCapture_t    pmkidCapture;      // PMKID hash capture
StationScanner    stationScanner;    // Passive client device discovery
ProbeSniffer      probeSniffer;      // Passive probe request capture
BLEAttacksExtra   bleAttacksExtra;   // Extra BLE attacks (AirDrop, trackers)

// Flag to track whether the evil portal is currently active
// Used so the main loop knows to call evilPortal.handle() each cycle
bool evilPortalActive = false;

// ── Touch Debounce ────────────────────────────────────────────────────────────
// Without debouncing, a single finger press registers as many presses
// We ignore any new touch that happens within TOUCH_MS milliseconds of the last one
bool     lastTouch   = false;   // Was the screen being touched last loop iteration?
uint32_t lastTouchMs = 0;       // millis() timestamp of the last valid touch
#define  TOUCH_MS 180           // Minimum time between recognised touches (ms)


// ── Menu State ────────────────────────────────────────────────────────────────
int selectedIndex = 0;     // Which menu item is currently highlighted on the TFT
#define MENU_COUNT 8       // How many items are in the main TFT menu

// ── WiFi Scan Data ────────────────────────────────────────────────────────────
// These arrays store the results of the last WiFi scan
// Max 20 networks - increase MAX_NETS if you want to scan more but it uses more RAM
#define MAX_NETS 20
String ssidList[MAX_NETS];                        // Network names (SSIDs)
int    rssiList[MAX_NETS];                        // Signal strengths in dBm (e.g. -65)
int    encList[MAX_NETS];                         // Encryption types (0=open, 4=WPA2)
int    chanList[MAX_NETS];                        // WiFi channels (1-13)
int    wifiCount=0;   // How many networks were found in the last scan
int    wifiStrong=0;  // How many networks have signal > -70 dBm (considered "strong")
int    wifiSel=0;     // Index of the currently selected network in the TFT list
int    wifiScroll=0;  // How far the list is scrolled down (for lists longer than screen)
bool   wifiDetail=false; // True when showing the detail popup for a selected network

// ── BLE Scan Data ─────────────────────────────────────────────────────────────
// Same pattern as WiFi data but for Bluetooth LE devices
#define MAX_BLE 20
String bleNameList[MAX_BLE];  // Device names (e.g. "AirPods", "Unknown")
String bleAddrList[MAX_BLE];  // MAC addresses (e.g. "AA:BB:CC:DD:EE:FF")
int    bleRssiList[MAX_BLE];  // Signal strengths in dBm
int    bleCount=0;    // How many BLE devices were found
int    bleSel=0;      // Currently selected device index
int    bleScroll=0;   // List scroll position
bool   bleDetail=false; // True when showing detail popup

// ── Network Scan Data ─────────────────────────────────────────────────────────
// Stores the IP and MAC addresses of devices connected to our hotspot
#define MAX_HOSTS 10
String netIPList[MAX_HOSTS];  // IP addresses (e.g. "192.168.4.2")
String netMACList[MAX_HOSTS]; // MAC addresses
int    netCount=0;

// ── AP Manager Data ───────────────────────────────────────────────────────────
// Tracks which devices are currently associated with our access point
String apIPList[MAX_HOSTS];
String apMACList[MAX_HOSTS];
int    apCount=0;

// ── Port Scan Data ────────────────────────────────────────────────────────────
// Stores results from the last port scan
#define MAX_PORTS 20
int    portPorts[MAX_PORTS]; // Port numbers that were found open (e.g. 80, 22)
String portSvcs[MAX_PORTS];  // Service names for those ports (e.g. "HTTP", "SSH")
int    portCount=0;          // How many open ports were found
String portTarget="";        // The IP address that was scanned

// ── Channel Analysis Data ─────────────────────────────────────────────────────
// Stores how many networks are using each WiFi channel (1-13)
// Index 0 is unused, index 1 = channel 1, index 13 = channel 13
int chNets[15] = {0};

// =============================================================================
//  HELPER FUNCTIONS
// =============================================================================

/**
 * readBattery() - Returns battery percentage (0-100)
 *
 * The battery voltage goes through a 100kΩ+100kΩ voltage divider before
 * reaching GPIO34. This halves the voltage, so we multiply by 2 to recover
 * the real battery voltage.
 *
 * LiPo battery voltage ranges:
 *   4.2V = 100% (fully charged)
 *   3.0V = 0%   (empty - don't discharge below this)
 *
 * We average 8 ADC readings to reduce noise from the ADC.
 */
int readBattery() {
  int raw = 0;
  for(int i=0; i<8; i++) raw += analogRead(BATTERY_PIN); // read 8 times and sum
  raw /= 8;  // divide by 8 to get the average ADC reading

  // ADC reading 0-4095 maps to 0-3.3V (with ADC_11db attenuation)
  float vgpio = (raw / 4095.0f) * 3.3f;

  // Undo the voltage divider: actual battery voltage is 2x what GPIO34 sees
  float vbat = vgpio * 2.0f;

  // Convert voltage to percentage: 3.0V=0%, 4.2V=100%
  // Formula: ((vbat - 3.0) / (4.2 - 3.0)) * 100 = ((vbat - 3.0) / 1.2) * 100
  return constrain((int)(((vbat - 3.0f) / 1.2f) * 100.0f), 0, 100);
}

/**
 * jsonSafe() - Sanitises a string so it won't break our hand-built JSON
 *
 * We build JSON strings manually (without a library) in some places.
 * This function makes sure the string doesn't contain characters that
 * would break the JSON syntax, like double-quotes or backslashes.
 *
 * Also caps the string at 64 chars to prevent very long SSIDs from
 * overflowing our JSON buffers.
 */
String jsonSafe(const String& s){
  String out;
  int maxLen = min((int)s.length(), 64); // cap at 64 chars
  out.reserve(maxLen);
  for(int i=0; i<maxLen; i++){
    char ch = s[i];
    if(ch==0x22)      out += 0x27;  // replace " (double-quote) with ' (single-quote)
    else if(ch==0x5C) out += 0x2F;  // replace \ (backslash) with / (forward-slash)
    else if(ch<32)    continue;      // skip control characters (newlines, tabs etc.)
    else              out += ch;     // normal character - keep it
  }
  return out;
}

/**
 * push() - Sends the current device state to the web browser
 *
 * Every time something changes (a scan completes, an attack starts/stops),
 * we call push() to update the JSON payload that the browser polls every 2 seconds.
 *
 * The 'st' parameter is the state name string the browser uses to know
 * which screen to show (e.g. "wifi", "attacks", "lock").
 */
void push(String st) {
  // Pack all our current data arrays into the web UI state payload
  webUI.pushState(st,
    ssidList, rssiList, encList, chanList, wifiCount,      // WiFi scan results
    bleNameList, bleAddrList, bleRssiList, bleCount,       // BLE scan results
    netIPList, netMACList, netCount,                        // Network scan results
    apIPList, apMACList, apCount,                           // AP client list
    portPorts, portSvcs, portCount, portTarget,             // Port scan results
    chNets, 13,                                             // Channel utilisation
    wifiSel, bleSel,                                        // Selected items
    readBattery()                                           // Battery percentage
  );
}

/**
 * doWiFiScan() - Scans for nearby WiFi networks and stores the results
 *
 * Shows the "Scanning..." screen on the TFT while the scan runs.
 * WiFi.scanNetworks() blocks for a few seconds while the radio sweeps channels.
 * After scanning, we also run channel analysis to find the least congested channel.
 */
void doWiFiScan() {
  push("scanning"); // tell the browser we're scanning (shows spinner)

  wifiCount = wifiViewer.scan(); // this blocks for ~3 seconds while scanning
  wifiStrong = 0;

  // Copy results out of the scanner into our local arrays
  for (int n=0; n<wifiCount; n++) {
    ssidList[n] = wifiViewer.getSSID(n);        // network name
    rssiList[n] = wifiViewer.getRSSI(n);        // signal strength in dBm
    encList[n]  = wifiViewer.getEncryption(n);  // encryption type
    chanList[n] = wifiViewer.getChannel(n);     // WiFi channel number

    // Count how many networks have good signal (above -70 dBm)
    if (rssiList[n] > -70) wifiStrong++;
  }

  // Analyse which channels are being used so we can suggest the best channel
  channelScanner.analyse(ssidList, rssiList, chanList, wifiCount);
  for (int c=0; c<15; c++)
    chNets[c] = channelScanner.getChannel(c).networkCount;

  // Reset selection to the first network
  wifiSel=0; wifiScroll=0; wifiDetail=false;
}

/**
 * doBLEScan() - Scans for nearby Bluetooth LE devices
 *
 * BLE scan takes about 4 seconds. Results are stored in bleNameList etc.
 * Note: phones use random MAC addresses for privacy so they often don't show up.
 */
void doBLEScan() {
  push("scanning"); // show spinner in browser

  bleCount = bleViewer.scan(); // blocks for ~4 seconds

  // Copy results into our local arrays
  for (int n=0; n<bleCount; n++) {
    bleNameList[n] = bleViewer.getName(n);    // device name if advertised
    bleAddrList[n] = bleViewer.getAddress(n); // MAC address
    bleRssiList[n] = bleViewer.getRSSI(n);    // signal strength
  }

  // Reset selection
  bleSel=0; bleScroll=0; bleDetail=false;
}

/**
 * doNetScan() - Scans for devices connected to our WiFi hotspot
 *
 * Uses ARP to discover which IP addresses are in use on 192.168.4.0/24.
 * Only finds devices that are currently connected to ESP32-Cyber WiFi.
 */
void doNetScan() {
  push("scanning");
  netCount = netScanner.scan(); // ARP scan of the subnet
  for (int n=0; n<netCount; n++) {
    netIPList[n]  = netScanner.getIP(n);  // e.g. "192.168.4.2"
    netMACList[n] = netScanner.getMAC(n); // e.g. "AA:BB:CC:DD:EE:FF"
  }
}

/**
 * doAPRefresh() - Gets the list of clients currently connected to our AP
 *
 * Uses esp_wifi_ap_get_sta_list() internally to get client MAC addresses,
 * then correlates with DHCP leases to get IP addresses.
 */
void doAPRefresh() {
  apCount = apManager.refresh();
  for (int n=0; n<apCount; n++) {
    apIPList[n]  = apManager.getIP(n);
    apMACList[n] = apManager.getMAC(n);
  }
}

/**
 * doPortScan() - Scans common TCP ports on a target IP address
 *
 * Tries to connect to 21 common ports (HTTP, SSH, FTP, etc.) with a 50ms timeout.
 * 50ms is plenty for local devices (192.168.4.x) since they respond in <5ms.
 * Note: this blocks the web server for ~1 second while scanning.
 */
void doPortScan(String target) {
  push("scanning");
  portTarget = target;

  // Show "scanning ports..." on the TFT display
  display.drawPortScan(target, portPorts, portSvcs, 0, true);

  // Run the actual scan (synchronous - blocks loop for ~1 second)
  portCount = portScanner.scan(target, 20); // 20ms timeout — keeps ESP32 responsive

  // Copy results into our arrays
  for (int n=0; n<portCount; n++) {
    portPorts[n] = portScanner.getResult(n).port;    // port number e.g. 80
    portSvcs[n]  = portScanner.getResult(n).service; // service name e.g. "HTTP"
  }
}

// =============================================================================
//  DISPLAY ATTACK STATUS
// =============================================================================

/**
 * drawAttackStatus() - Shows current attack state on the TFT screen.
 * Keeps the physical display in sync when attacks are started/stopped from web UI.
 */
void drawAttackStatus(const char* attackName, bool running) {
  display.drawAttackStatus(attackName, running);
  Serial.printf("[ATTACK] %s: %s\n", attackName, running?"STARTED":"STOPPED");
}

// =============================================================================
//  WEB COMMAND HANDLER
// =============================================================================

/**
 * handleWebCmd() - Processes a command received from the web browser
 *
 * The web UI sends commands as JSON via POST /cmd. The WebUI class parses
 * these into WebCommandData structs and queues them. This function is called
 * from the main loop when a new command is ready.
 *
 * Each case in the switch handles one type of command - scanning, attack
 * start/stop, settings changes, etc.
 */
void handleWebCmd(WebCommandData c) {
  Serial.printf("[CMD] received cmd=%d\n", (int)c.cmd);

  switch (c.cmd) {

    // ── PIN VERIFICATION ────────────────────────────────────────────────────
    case CMD_PIN:
      // PIN disabled — always approve
      webUI.setPinResult(true);
      currentState=UI_MENU;
      display.drawMenu(selectedIndex, readBattery());
      push("menu");
      break;

    // ── NAVIGATION ──────────────────────────────────────────────────────────
    case CMD_GOTO_MENU:
      // User clicked "back" or home - return to the main menu
      currentState=UI_MENU;
      display.drawMenu(selectedIndex, readBattery());
      push("menu");
      break;

    // ── SCANS ───────────────────────────────────────────────────────────────
    case CMD_SCAN_WIFI:
      // User clicked "Rescan" in the WiFi tab
      currentState=UI_WIFI;
      display.drawScreen(UI_WIFI);
      doWiFiScan(); // this blocks for ~3 seconds
      // Update both the TFT display and the web UI
      display.drawWiFiList(ssidList,rssiList,encList,wifiCount,wifiSel,wifiScroll);
      display.drawWiFiStats(wifiCount,wifiStrong);
      push("wifi"); // send results to browser
      break;

    case CMD_SCAN_BLE:
      // User clicked "Rescan" in the BLE tab
      currentState=UI_BLUETOOTH;
      display.drawScreen(UI_BLUETOOTH);
      doBLEScan(); // blocks for ~4 seconds
      display.drawBLEList(bleNameList,bleAddrList,bleRssiList,bleCount,bleSel,bleScroll);
      display.drawBLEStats(bleCount);
      push("ble");
      break;

    case CMD_SCAN_NET:
      // User clicked "Refresh" in the Network tab - ARP scan
      currentState=UI_NETWORK;
      doNetScan();
      display.drawNetworkList(netIPList,netMACList,netCount);
      push("net");
      break;

    case CMD_AP_REFRESH:
      // User refreshed the AP client list
      doAPRefresh();
      display.drawAPManager(apIPList,apMACList,apCount);
      push("tools");
      break;

    case CMD_AP_KICK_ALL:
      // User clicked "Kick All" - deauthenticate all connected clients
      apManager.kickAll();
      delay(500); // give clients time to disconnect before refreshing
      doAPRefresh();
      display.drawAPManager(apIPList,apMACList,apCount);
      push("tools");
      break;

    case CMD_PORT_SCAN:
      // User submitted a port scan - c.strVal contains the target IP
      doPortScan(c.strVal.length()>0 ? c.strVal : "192.168.4.2");
      display.drawPortScan(portTarget,portPorts,portSvcs,portCount,false);
      push("tools");
      break;

    case CMD_CHANNEL_ANALYSE:
      // Analyse which channels the nearby networks are using
      channelScanner.analyse(ssidList,rssiList,chanList,wifiCount);
      for (int i=0;i<15;i++) chNets[i]=channelScanner.getChannel(i).networkCount;
      display.drawChannelMap(chNets,
        channelScanner.getFreestChannel(),   // least congested channel
        channelScanner.getBusiestChannel()); // most congested channel
      push("tools");
      break;

    case CMD_GOTO_SETTINGS:
      currentState=UI_SETTINGS;
      display.drawSettingsScreen();
      push("settings");
      break;

    case CMD_SELECT_WIFI:
      // User tapped a specific network in the browser list - show its detail
      if (c.index>=0 && c.index<wifiCount) {
        wifiSel=c.index;
        currentState=UI_WIFI;
        display.drawWiFiDetail(ssidList[wifiSel],rssiList[wifiSel],
                               encList[wifiSel],chanList[wifiSel]);
        push("wifi");
      }
      break;

    case CMD_SELECT_BLE:
      // User tapped a specific BLE device in the browser list
      if (c.index>=0 && c.index<bleCount) {
        bleSel=c.index;
        currentState=UI_BLUETOOTH;
        display.drawBLEDetail(bleNameList[bleSel],bleAddrList[bleSel],bleRssiList[bleSel]);
        push("ble");
      }
      break;

    case CMD_BACK:
      currentState=UI_MENU;
      display.drawMenu(selectedIndex,readBattery());
      push("menu");
      break;

    case CMD_REBOOT:
      // User clicked the reboot button - show screen and restart
      display.drawScreen(UI_REBOOT);
      delay(1200); // give the browser time to receive the response before we restart
      ESP.restart();
      break;

    // ── WIFI ATTACK: BEACON SPAM (LIST) ─────────────────────────────────────
    case CMD_ATK_BEACON_LIST: {
      if (c.strVal.length() > 0) {
        // SSIDs provided — always stop any current run then start fresh.
        // This way troll tools (which always send SSIDs) always START,
        // never accidentally toggle-off an existing session.
        beaconSpamList.stop();
        beaconSpamList.clearSSIDs();
        String sv = c.strVal;
        while (sv.length() > 0) {
          int cm = sv.indexOf(',');
          String s = (cm < 0) ? sv : sv.substring(0, cm);
          s.trim();
          if (s.length() > 0) beaconSpamList.addSSID(s.c_str());
          if (cm < 0) break;
          sv = sv.substring(cm + 1);
        }
        beaconSpamList.start();
      } else {
        // No SSIDs provided — treat as a toggle (used by the Stop button)
        if (beaconSpamList.isRunning()) beaconSpamList.stop();
      }
      if(beaconSpamList.isRunning()) drawAttackStatus("BEACON_LIST", true);
      else drawAttackStatus("BEACON_LIST", false);
      push("attacks");
      break;
    }

    // ── WIFI ATTACK: BEACON SPAM (RANDOM) ───────────────────────────────────
    case CMD_ATK_BEACON_RANDOM:
      // Toggle random beacon spam - 20 simultaneous fake SSIDs
      if (beaconSpamRandom.isRunning()) beaconSpamRandom.stop();
      else beaconSpamRandom.start(20);
      drawAttackStatus("BEACON_RAND", beaconSpamRandom.isRunning());
      push("attacks");
      break;

    // ── WIFI ATTACK: EVIL PORTAL ─────────────────────────────────────────────
    case CMD_ATK_EVIL_PORTAL:
      if (evilPortalActive) {
        // Disable: AP reverts to ESP32-Cyber, "/" serves dashboard again
        webUI.setPortalMode(false);
        evilPortalActive = false;
        push("attacks");
      } else {
        int tplId = c.index >= 0 ? c.index : 0;
        // Enable: renames AP to fake SSID, hooks "/" to portal page
        // Dashboard STAYS ONLINE — attacker accesses it at http://192.168.4.1/dash
        webUI.setPortalMode(true, tplId);
        evilPortalActive = true;
        push("attacks");
      }
      break;

    // ── WIFI ATTACK: RICK ROLL BEACON ───────────────────────────────────────
    case CMD_ATK_RICK_ROLL:
      // Broadcasts Rick Astley lyrics as WiFi SSIDs
      if (rickRollBeacon.isRunning()) rickRollBeacon.stop();
      else rickRollBeacon.start();
      push("attacks");
      break;

    // ── WIFI ATTACK: PROBE FLOOD ─────────────────────────────────────────────
    case CMD_ATK_PROBE_FLOOD:
      // Floods the air with probe request frames for random SSIDs
      if (probeFlood.isRunning()) probeFlood.stop();
      else probeFlood.start();
      push("attacks");
      break;

    // ── WIFI ATTACK: AP CLONE ────────────────────────────────────────────────
    case CMD_ATK_AP_CLONE_SCAN:
      apCloneSpam.scan(); // first scan for nearby APs to clone
      push("attacks");
      break;

    case CMD_ATK_AP_CLONE:
      if(apCloneSpam.isRunning()) apCloneSpam.stop();
      else apCloneSpam.start();
      push("attacks");
      break;

    // ── WIFI ATTACK: KARMA ───────────────────────────────────────────────────
    case CMD_ATK_KARMA_SCAN:
      karmaAttack.scan(); // scan so user can select target networks
      push("attacks");
      break;

    case CMD_ATK_KARMA:
      // Karma responds to ALL probe requests if nothing selected,
      // or only selected networks if user picked specific ones
      if(karmaAttack.isRunning()) karmaAttack.stop();
      else karmaAttack.start();
      drawAttackStatus("KARMA", karmaAttack.isRunning());
      push("attacks");
      break;

    // ── BLE ATTACK: iOS POPUP SPAM ───────────────────────────────────────────
    case CMD_ATK_BLE_SPAM:
      // Sends Apple Continuity Protocol advertisements to trigger iOS popups
      if(bleSpam.isRunning()) bleSpam.stop();
      else bleSpam.start();
      drawAttackStatus("BLE_SPAM_IOS", bleSpam.isRunning());
      push("attacks");
      break;

    // ── BLE ATTACK: MULTI-PLATFORM SPAM ─────────────────────────────────────
    case CMD_ATK_BLE_SPAM_ALL:
      // c.index: 0=Apple only, 1=Android only, 2=Windows only, 3=All platforms
      if(bleSpamAll.isRunning()) bleSpamAll.stop();
      else bleSpamAll.start(c.index >= 0 ? c.index : 3);
      push("attacks");
      break;

    // ── DEFENCE: EAPOL/WPA2 HANDSHAKE SNIFFER ───────────────────────────────
    case CMD_ATK_EAPOL:
      // Listens for WPA2 4-way handshake frames on all channels
      if(eapolSniffer.isRunning()) eapolSniffer.stop();
      else eapolSniffer.start();
      push("attacks");
      break;



    // ── DEFENCE: ROGUE AP / PINEAPPLE DETECTOR ──────────────────────────────
    case CMD_ATK_PINEAPPLE:
      // Scans for suspicious APs that look like evil twins or Pineapples
      if(pineappleDetector.isRunning()) pineappleDetector.stop();
      else pineappleDetector.start();
      push("defend");
      break;

    // ── BT ATTACK: CHROMECAST ────────────────────────────────────────────────
    case CMD_ATK_CC_SCAN:
      chromecastAttack.scan(); // discover Chromecast devices on the network
      push("batk");
      break;

    case CMD_ATK_CC_RICKROLL:
      // Cast a Rick Roll to the selected Chromecast (or all if index < 0)
      if(c.index < 0) chromecastAttack.castAll("https://www.youtube.com/watch?v=dQw4w9WgXcQ");
      else            chromecastAttack.castRickRoll(c.index);
      push("batk");
      break;

    // ── WIFI ATTACK: SSID CONFUSION ──────────────────────────────────────────
    case CMD_ATK_SSID_CONF:
      // Creates SSIDs that are visually similar to nearby real networks
      // e.g. "BT Hub" → "BT Hub " (trailing space) or "BT-Hub" etc.
      if(ssidConfusion.isRunning()) ssidConfusion.stop();
      else ssidConfusion.start();
      push("watk");
      break;

    // ── WIFI ATTACK: DEAUTH ──────────────────────────────────────────────────
    case CMD_DEAUTH_SCAN:
      deauthAttack.scan(); // starts async scan - results pushed in loop()
      push("watk");
      break;

    case CMD_DEAUTH_ALL:
      deauthAttack.startAll();
      drawAttackStatus("DEAUTH_ALL", true);
      push("watk");
      break;

    case CMD_DEAUTH_SEL: {
      // Deauth specific selected networks - c.strVal is comma-separated indices
      // e.g. "0,2,5" = deauth networks at index 0, 2, and 5
      deauthAttack.clearSelection();
      String sv = c.strVal;
      int pos = 0;
      while(pos < (int)sv.length()){
        int comma = sv.indexOf(',', pos);
        if(comma < 0) comma = sv.length();
        int idx = sv.substring(pos, comma).toInt();
        deauthAttack.select(idx, true); // mark this network as selected
        pos = comma + 1;
      }
      deauthAttack.startSelected();
      push("watk");
      break;
    }

    case CMD_DEAUTH_STOP:
      deauthAttack.stop();
      drawAttackStatus("DEAUTH", false);
      push("watk");
      break;

    // ── WARDRIVING ───────────────────────────────────────────────────────────
    case CMD_WARDRIVE_START:
      wardriving.start(); // begin logging discovered APs
      push("wardrive");
      break;

    case CMD_WARDRIVE_STOP:
      wardriving.stop();
      push("wardrive");
      break;

    case CMD_WARDRIVE_CLEAR:
      wardriving.clear(); // wipe the logged AP list
      push("wardrive");
      break;

    // ── WIFI ATTACK: AUTH FLOOD ──────────────────────────────────────────────
    case CMD_AUTH_FLOOD_SCAN:
      authFlood.scan(); push("watk"); break;

    case CMD_AUTH_FLOOD_ALL:
      // Stop wardriving and deauth first to avoid radio conflicts
      wardriving.stop();
      deauthAttack.stop();
      authFlood.startAll(); push("watk"); break;

    case CMD_AUTH_FLOOD_SEL: {
      // Parse comma-separated target indices same as deauth
      authFlood.clearSelection();
      String sv=c.strVal; int pos=0;
      while(pos<(int)sv.length()){
        int cm=sv.indexOf(',',pos); if(cm<0)cm=sv.length();
        authFlood.select(sv.substring(pos,cm).toInt(),true); pos=cm+1;
      }
      authFlood.startSelected(); push("watk"); break;
    }
    case CMD_AUTH_FLOOD_STOP:
      authFlood.stop(); push("watk"); break;

    // ── WIFI ATTACK: PMKID CAPTURE ───────────────────────────────────────────
    case CMD_EAPOL_SET_CH:
      // c.index: 0 = hop all channels, 1-13 = lock to specific channel
      eapolSniffer.setTargetChannel(c.index);
      Serial.printf("EAPOL: channel %s\n",
        c.index==0 ? "hopping all" : ("locked to ch"+String(c.index)).c_str());
      break;

    case CMD_EAPOL_TARGETED:
      // Deauth + capture mode
      // c.index = target channel, c.strVal = target BSSID
      // Make sure sniffer is running first
      if(!eapolSniffer.isRunning()) eapolSniffer.start();
      eapolSniffer.startTargeted(c.strVal, (uint8_t)c.index);
      push("attacks");
      break;

    case CMD_PMKID_START:
      // Start passively capturing PMKID hashes from WPA2 beacon frames
      pmkidCapture.start(); push("watk"); break;

    case CMD_PMKID_STOP:
      pmkidCapture.stop(); push("watk"); break;

    // ── BT ATTACK: AIRDROP SPAM ──────────────────────────────────────────────
    case CMD_AIRDROP_START:
      bleAttacksExtra.startAirdropSpam(); push("batk"); break;

    case CMD_AIRDROP_STOP:
      bleAttacksExtra.stopAirdropSpam(); push("batk"); break;

    // ── BT ATTACK: CONTROLLER SPAM ───────────────────────────────────────────
    case CMD_CTRL_SPAM:
      // c.index = controller type (0=PS5, 1=Xbox, 2=Nintendo Switch etc.)
      bleAttacksExtra.startControllerSpam(c.index); push("batk"); break;

    case CMD_CTRL_STOP:
      bleAttacksExtra.stopControllerSpam(); push("batk"); break;

    // ── BT DEFENCE: TRACKER DETECTOR ─────────────────────────────────────────
    case CMD_SCAN_TRACKERS:
      // Scan for AirTag and Tile tracking devices via their BLE signatures
      bleAttacksExtra.scanTrackers(); push("batk"); break;

    // ── BT ATTACK: BLE NAME SPOOF ─────────────────────────────────────────────
    case CMD_NAME_SPOOF_START:
      bleAttacksExtra.startNameSpoof(c.strVal, c.index); push("batk"); break;

    case CMD_NAME_SPOOF_STOP:
      bleAttacksExtra.stopNameSpoof(); push("batk"); break;

    // ── BT ATTACK: BADBLE KEYBOARD INJECTION ─────────────────────────────────
    case CMD_BADBLE_START:
      // Stop all other BLE attacks first - they all use ble_gap_adv_start
      // and will conflict with BadBLE's advertising
      bleSpam.stop();
      bleSpamAll.stop();
      bleAttacksExtra.stopAirdropSpam();
      bleAttacksExtra.stopControllerSpam();
      // Start advertising as a Bluetooth keyboard
      // c.strVal = device name to use (e.g. "Magic Keyboard", "Microsoft Keyboard")
      badBLE.startAdvertising(c.strVal.length() ? c.strVal : "Magic Keyboard");
      push("batk");
      break;

    case CMD_BADBLE_STOP:
      badBLE.stop();
      push("batk");
      break;

    case CMD_BADBLE_RUN:
      // Execute a payload on the connected host
      // c.index = payload type (see BadBLEPayload enum in bad_ble.h)
      // c.strVal = custom text for payloads that type user-defined text
      badBLE.runPayload((BadBLEPayload)c.index, c.strVal);
      push("batk");
      break;

    // ── WIFI CRACKER ─────────────────────────────────────────────────────────
    case CMD_WIFI_CRACK_START: {
      // c.strVal contains the crack target in pipe-separated format:
      // "ssid|bssid_hex|client_hex|pmkid_hex"
      // e.g. "MyWiFi|AABBCCDDEEFF|112233445566|abcdef0123456789abcdef0123456789"
      String sv = c.strVal;
      int p1=sv.indexOf('|'),           // position of first pipe
          p2=sv.indexOf('|',p1+1),      // position of second pipe
          p3=sv.indexOf('|',p2+1);      // position of third pipe
      if(p1>0 && p2>0 && p3>0){
        String ssid   = sv.substring(0,p1);     // extract SSID
        String bssid  = sv.substring(p1+1,p2);  // extract AP MAC (no colons)
        String client = sv.substring(p2+1,p3);  // extract client MAC (no colons)
        String pmkid  = sv.substring(p3+1);     // extract PMKID hash
        wifiCracker.startCrack(ssid,bssid,client,pmkid); // start cracking task
      }
      push("attacks"); break;
    }

    case CMD_WIFI_CRACK_STOP:
      wifiCracker.stop();
      push("attacks"); break;

    // ── EVIL PORTAL: CLEAR CREDENTIALS ──────────────────────────────────────
    case CMD_EP_CLEAR:
      evilPortal.clearCreds(); // wipe captured credentials from RAM
      push("attacks");
      break;

    // ── PACKET COUNTER ───────────────────────────────────────────────────────
    case CMD_PACKET_COUNT_START:
    case CMD_PACKET_COUNT_STOP:
      // The packet counter runs entirely client-side in the browser
      // No ESP32-side handling needed
      break;

    // ── SETTINGS: DNS SPOOF ──────────────────────────────────────────────────
    case CMD_DNS_START:
      // Start DNS server that returns our IP for all queries
      // Any device connected to our AP will be redirected to us for all websites
      dnsSpoof.start(WiFi.softAPIP());
      push("settings");
      break;

    case CMD_DNS_STOP:
      dnsSpoof.stop();
      push("settings");
      break;

    // ── SETTINGS: CLEAR CREDENTIALS ──────────────────────────────────────────
    case CMD_CLEAR_CREDS:
      cyberStorage.clearCreds(); // delete the credentials file from LittleFS
      push("settings");
      break;

    // ── STOP ALL ATTACKS ─────────────────────────────────────────────────────
    case CMD_ATK_BEACON_STOP:
      // Stop beacon list specifically (used by troll tools and beacon spam)
      beaconSpamList.stop();
      Serial.println("[ATTACK] BEACON_LIST: STOPPED (explicit stop cmd)");
      push("attacks");
      break;

    case CMD_STOP_ALL:
      // Emergency stop - calls stop() on every attack module
      // Used by the "Stop All" button in the web UI
      beaconSpamList.stop();
      beaconSpamRandom.stop();
      rickRollBeacon.stop();
      probeFlood.stop();
      bleSpam.stop();
      bleSpamAll.stop();
      deauthAttack.stop();
      authFlood.stop();
      eapolSniffer.stop();
      pmkidCapture.stop();
      ssidConfusion.stop();
      karmaAttack.stop();
      wardriving.stop();
      chromecastAttack.stop();
      pineappleDetector.stop();
      bleAttacksExtra.stopAirdropSpam();
      bleAttacksExtra.stopControllerSpam();
      bleAttacksExtra.stopNameSpoof();
      badBLE.stop();
      stationScanner.stop();
      probeSniffer.stop();
      push("menu"); break;

    default: break; // unknown command - ignore it
  }
}

// =============================================================================
//  SETUP — runs once when the ESP32 boots
// =============================================================================
void setup() {
  Serial.begin(115200); // start serial output for debugging (open at 115200 baud)

  // Turn on the TFT backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH); // HIGH = backlight on

  // Configure the ADC for battery reading
  analogReadResolution(12);   // 12-bit ADC = readings from 0 to 4095
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db); // 11dB attenuation = 0-3.3V range
  analogSetAttenuation(ADC_11db); // also set globally

  // Initialise the TFT display and show the boot splash screen
  display.begin();
  display.drawBoot();
  delay(300); // brief pause so the boot screen is visible

  // IMPORTANT: BLE must be initialised BEFORE setting up the WiFi AP
  // Both BLE and WiFi share the same radio hardware (coexistence)
  // Initialising in the wrong order can cause crashes
  bleViewer.begin();
  delay(200);

  // Set up WiFi in WIFI_AP_STA mode (Access Point + Station simultaneously)
  // This lets us scan for networks (STA) while also hosting our AP
  wifiViewer.begin();
  webUI.begin(); // this starts the hotspot "ESP32-Cyber" and the HTTP server

  // Initialise each attack module (just sets up internal state, doesn't start attacking)
  beaconSpamList.begin();
  beaconSpamRandom.begin();
  rickRollBeacon.begin();
  probeFlood.begin();
  apCloneSpam.begin();
  karmaAttack.begin();
  bleSpam.begin();
  badBLE.begin();
  bleSpamAll.begin();
  eapolSniffer.begin();
  pineappleDetector.begin();
  chromecastAttack.begin();
  ssidConfusion.begin();
  deauthAttack.begin();
  wardriving.begin();
  authFlood.begin();
  pmkidCapture.begin();
  wifiCracker.begin();
  bleAttacksExtra.begin();
  stationScanner.begin();
  probeSniffer.begin();

  // Explicitly stop everything on boot - some modules might auto-start otherwise
  beaconSpamList.stop(); beaconSpamRandom.stop(); rickRollBeacon.stop();
  probeFlood.stop(); bleSpam.stop(); bleSpamAll.stop(); deauthAttack.stop();
  authFlood.stop(); eapolSniffer.stop(); pineappleDetector.stop();
  pmkidCapture.stop(); wardriving.stop(); chromecastAttack.stop();
  karmaAttack.stop(); ssidConfusion.stop(); apCloneSpam.stop();

  // Load persistent settings from flash storage
  cyberStorage.begin(); // mount the LittleFS filesystem

  // No PIN — boot straight to menu
  currentState = UI_MENU;
  display.drawMenu(0, readBattery());
  push("menu");
}

// =============================================================================
//  LOOP — runs continuously after setup() completes
// =============================================================================
void loop() {

  // ── ATTACK TICKS ──────────────────────────────────────────────────────────
  // Each attack module has a tick() function that does a small amount of work
  // each loop iteration. This is "cooperative multitasking" - none of them
  // block for long, so everything stays responsive.
  //
  // For example, beaconSpamRandom.tick() sends one beacon frame and returns.
  // Next loop iteration it sends another. This gives ~10,000 frames per second.
  beaconSpamList.tick();       // send next batch of custom SSID beacons
  beaconSpamRandom.tick();     // send next random SSID beacon
  rickRollBeacon.tick();        // send next Rick Roll SSID beacon
  probeFlood.tick();            // send next probe request frame
  apCloneSpam.tick();           // send next cloned AP beacon
  karmaAttack.tick();           // process probe requests and serve portal
  bleSpam.tick();               // send next Apple BLE advertisement
  badBLE.tick();                // process BadBLE keyboard connection/payloads
  bleSpamAll.tick();            // send next multi-platform BLE advertisement
  eapolSniffer.tick();          // check for new EAPOL handshake frames
  pineappleDetector.tick();     // check for suspicious APs
  ssidConfusion.tick();         // send next confusion beacon
  deauthAttack.tick();          // send next deauth frame + process async scan
  if(deauthAttack.scanJustDone()){
    // Push scan results immediately (don't wait for 5s sync)
    String dsj = "[";
    for(int i=0; i<deauthAttack.getScanCount(); i++){
      DeauthTarget& dt = deauthAttack.getTarget(i);
      if(i>0) dsj+=",";
      dsj+="{\"ssid\":\""  + jsonSafe(dt.ssid)  + "\",";
      dsj+="\"bssid\":\"" + jsonSafe(dt.bssid) + "\",";
      dsj+="\"ch\":"         + String(dt.channel) + ",";
      dsj+="\"rssi\":"       + String(dt.rssi)    + ",";
      dsj+="\"enc\":\"WPA2\"}";
    }
    dsj+="]";
    webUI.setDeauthScanPayload(dsj);
    push("watk");
  }
  wardriving.tick();            // log any newly discovered APs
  authFlood.tick();             // send next auth request flood
  pmkidCapture.tick();          // check for PMKID frames
  wifiCracker.tick();           // (no-op - cracker runs in FreeRTOS task)
  bleAttacksExtra.tickAirdrop();    // process AirDrop spam
  bleAttacksExtra.tickController(); // process controller spam

  // ── PAYLOAD SYNC (every 5 seconds) ────────────────────────────────────────
  // Some data changes slowly (pineapple results, wardrive list etc.) so we
  // only update the web UI payload for these every 5 seconds rather than
  // every loop iteration (which would be thousands of times per second)
  static uint32_t lastPineSync = 0;
  if(millis() - lastPineSync > 5000){
    lastPineSync = millis();

    int batPct = readBattery();
    webUI.setBatPct(batPct);
    // Low battery warning — show on TFT and push to web UI
    static bool lowBatWarned = false;
    if(batPct < 15 && !lowBatWarned) {
      lowBatWarned = true;
      Serial.printf("WARNING: Low battery %d%%\n", batPct);
      // Flash the TFT display to alert user
      for(int i=0;i<3;i++){
        digitalWrite(TFT_BL, LOW); delay(200);
        digitalWrite(TFT_BL, HIGH); delay(200);
      }
    }
    if(batPct >= 20) lowBatWarned = false; // reset when charged

    // ── Pineapple/Rogue AP detection results ────────────────────────────────
    String pj = "[";
    for(int i=0; i<pineappleDetector.getFoundCount(); i++){
      SuspiciousAP a = pineappleDetector.get(i);
      if(i>0) pj+=",";
      pj+="{";
      pj+="\"ssid\":\""   + jsonSafe(a.ssid)   + "\",";
      pj+="\"bssid\":\""  + jsonSafe(a.bssid)  + "\",";
      pj+="\"rssi\":"      + String(a.rssi)    + ",";
      pj+="\"ch\":"        + String(a.channel) + ",";
      pj+="\"reason\":\"" + jsonSafe(a.reason) + "\"";
      pj+="}";
    }
    pj+="]";
    webUI.setPinePayload(pj);

    // ── Chromecast device list ────────────────────────────────────────────────
    String cj = "[";
    for(int i=0; i<chromecastAttack.getFoundCount(); i++){
      ChromecastDevice d = chromecastAttack.get(i);
      if(i>0) cj+=",";
      cj+="{\"name\":\"" + jsonSafe(d.name) + "\",\"ip\":\"" + jsonSafe(d.ip) + "\"}";
    }
    cj+="]";
    webUI.setCCPayload(cj);

    // ── Deauth attack scan results ────────────────────────────────────────────
    String dsj = "[";
    for(int i=0; i<deauthAttack.getScanCount(); i++){
      DeauthTarget& t = deauthAttack.getTarget(i);
      if(i>0) dsj+=",";
      dsj+="{\"ssid\":\""  + jsonSafe(t.ssid)  + "\",";
      dsj+="\"bssid\":\"" + jsonSafe(t.bssid) + "\",";
      dsj+="\"ch\":"       + String(t.channel) + ",";
      dsj+="\"rssi\":"     + String(t.rssi)    + ",";
      dsj+="\"enc\":\"WPA2\"}";
    }
    dsj+="]";
    webUI.setDeauthScanPayload(dsj);

    // ── Deauth running state ──────────────────────────────────────────────────
    String dst = "{\"running\":";
    dst += deauthAttack.isRunning() ? "true" : "false";
    dst += ",\"count\":" + String(deauthAttack.getDeauthCount()) + "}";
    webUI.setDeauthStatePayload(dst);

    // ── Wardriving AP log ─────────────────────────────────────────────────────
    String wdj = "{\"running\":";
    wdj += wardriving.isRunning() ? "true" : "false";
    wdj += ",\"count\":"  + String(wardriving.getCount());
    wdj += ",\"total\":"  + String(wardriving.getTotalSeen());
    wdj += ",\"aps\":[";
    for(int i=0; i<wardriving.getCount(); i++){
      WardrivedAP& a = wardriving.getAP(i);
      if(i>0) wdj+=",";
      wdj+="{\"ssid\":\""  + jsonSafe(a.ssid)  + "\",";
      wdj+="\"bssid\":\"" + jsonSafe(a.bssid) + "\",";
      wdj+="\"ch\":"       + String(a.channel) + ",";
      wdj+="\"enc\":\""   + jsonSafe(wardriving.getEncStr(a.enc)) + "\",";
      wdj+="\"rssi\":"     + String(a.rssi) + ",";
      wdj+="\"seen\":"     + String(a.seenCount) + "}";
    }
    wdj+="]}";
    webUI.setWardrivePayload(wdj);

    // ── BadBLE keyboard state ─────────────────────────────────────────────────
    // The browser polls this to show "Connected & ready" etc.
    String bbj = "{\"running\":";
    bbj += badBLE.isRunning() ? "true" : "false";
    bbj += ",\"connected\":"; bbj += badBLE.isConnected() ? "true" : "false";
    bbj += ",\"busy\":";      bbj += badBLE.isBusy()      ? "true" : "false";
    bbj += ",\"status\":\"" + jsonSafe(badBLE.getStatus()) + "\"}";
    webUI.setBadBLEPayload(bbj);

    // ── Auth flood scan results ───────────────────────────────────────────────
    String afj = "[";
    for(int i=0; i<authFlood.getScanCount(); i++){
      AuthTarget& t = authFlood.getTarget(i);
      if(i>0) afj+=",";
      afj+="{\"ssid\":\""  + t.ssid  + "\",";
      afj+="\"bssid\":\"" + t.bssid + "\",";
      afj+="\"ch\":"       + String(t.channel) + ",";
      afj+="\"rssi\":0}";
    }
    afj+="]";
    webUI.setAuthFloodPayload(afj);

    // ── PMKID capture results ─────────────────────────────────────────────────
    // These are the WPA2 hashes we've passively captured
    String pmj = "{\"count\":" + String(pmkidCapture.getCaptureCount()) + ",\"caps\":[";
    for(int i=0; i<pmkidCapture.getCaptureCount(); i++){
      PMKIDCapture& p = pmkidCapture.getCapture(i);
      if(i>0) pmj+=",";
      pmj+="{\"pmkid\":\""  + jsonSafe(p.pmkid)  + "\",";
      pmj+="\"bssid\":\"" + jsonSafe(p.bssid)  + "\",";
      pmj+="\"ch\":"          + String(p.channel) + "}";
    }
    pmj+="]}";
    webUI.setPMKIDPayload(pmj, pmkidCapture.getHashcatFormat());

    // ── Tracker detector results ──────────────────────────────────────────────
    String trj = "{\"count\":" + String(bleAttacksExtra.getTrackerCount()) + ",\"trackers\":[";
    for(int i=0; i<bleAttacksExtra.getTrackerCount(); i++){
      TrackerResult& t = bleAttacksExtra.getTracker(i);
      if(i>0) trj+=",";
      trj+="{\"type\":\""    + jsonSafe(t.type)    + "\",";
      trj+="\"address\":\"" + jsonSafe(t.address) + "\",";
      trj+="\"rssi\":"          + String(t.rssi)  + "}";
    }
    trj+="]}";
    webUI.setTrackerPayload(trj);
  }

  // ── EVIL PORTAL TOUCH TO STOP ─────────────────────────────────────────────
  // Evil portal runs on port 8080 via AsyncWebServer (handles itself)
  // But if the user taps the TFT screen while portal is active, stop it
  if (evilPortalActive) {
    uint16_t tx=0, ty=0;
    if (display.getTouch(tx, ty)) {
      evilPortal.stop();
      evilPortalActive = false;
      currentState = UI_MENU;
      display.drawMenu(selectedIndex, readBattery());
      push("menu");
    }
  }

  // ── WEB SERVER HANDLER ────────────────────────────────────────────────────
  // Process any pending HTTP requests from the browser
  // Must be called every loop or the web server will be unresponsive
  {
    webUI.handle();

    // Check if the browser sent us a command (e.g. start attack, scan, etc.)
    if (webUI.hasCommand())
      handleWebCmd(webUI.getCommand()); // process it
  }

  // ── TOUCH INPUT PROCESSING ────────────────────────────────────────────────
  uint16_t x=0, y=0;
  bool touching = display.getTouch(x, y); // check if screen is being touched right now

  // Only process "fresh" touches - not held touches or bounced touches
  // A touch is "fresh" if:
  //   1. The screen IS being touched now (touching == true)
  //   2. The screen was NOT being touched last loop (lastTouch == false)
  //   3. At least TOUCH_MS (180ms) has passed since the last valid touch
  bool fresh = touching && !lastTouch && (millis()-lastTouchMs > TOUCH_MS);

  if (fresh) {
    lastTouchMs = millis(); // record when this touch happened

    // Handle the touch differently depending on which screen is showing
    if (currentState == UI_MENU) {
      // ── MAIN MENU ──────────────────────────────────────────────────────────
      // Figure out which menu item was tapped based on Y coordinate
      // Menu items start at Y=28, each item is 28px tall with 5px gap
      int sY=28, h=28, sp=5;
      for (int i=0; i<MENU_COUNT; i++) {
        int itemTop    = sY + i*(h+sp);
        int itemBottom = itemTop + h;
        if (y >= itemTop && y < itemBottom) {
          selectedIndex = i;
          switch(i) {
            case 0: // WiFi Scanner
              currentState=UI_WIFI; display.drawScreen(UI_WIFI);
              doWiFiScan();
              display.drawWiFiList(ssidList,rssiList,encList,wifiCount,wifiSel,wifiScroll);
              display.drawWiFiStats(wifiCount,wifiStrong);
              push("wifi"); break;

            case 1: // Bluetooth Scanner
              currentState=UI_BLUETOOTH; display.drawScreen(UI_BLUETOOTH);
              doBLEScan();
              display.drawBLEList(bleNameList,bleAddrList,bleRssiList,bleCount,bleSel,bleScroll);
              display.drawBLEStats(bleCount);
              push("ble"); break;

            case 2: // Network Scanner
              currentState=UI_NETWORK;
              doNetScan();
              display.drawNetworkList(netIPList,netMACList,netCount);
              push("net"); break;

            case 3: // AP Manager - who is connected to our hotspot
              currentState=UI_AP_MANAGER;
              doAPRefresh();
              display.drawAPManager(apIPList,apMACList,apCount);
              push("tools"); break;

            case 4: // Port Scanner
              currentState=UI_PORT_SCAN;
              display.drawPortScan("192.168.4.2",portPorts,portSvcs,0,false);
              push("tools"); break;

            case 5: // Channel Map - which channels are busy
              currentState=UI_CHANNEL;
              channelScanner.analyse(ssidList,rssiList,chanList,wifiCount);
              for(int c=0;c<15;c++) chNets[c]=channelScanner.getChannel(c).networkCount;
              display.drawChannelMap(chNets,
                channelScanner.getFreestChannel(),
                channelScanner.getBusiestChannel());
              push("tools"); break;

            case 6: // Settings
              currentState=UI_SETTINGS;
              display.drawSettingsScreen();
              push("settings"); break;

            case 7: // Reboot
              display.drawScreen(UI_REBOOT);
              delay(1200);
              ESP.restart(); break;
          }
          break; // stop checking other menu items once we found the tapped one
        }
      }

    } else if (currentState == UI_WIFI) {
      // ── WIFI LIST SCREEN ────────────────────────────────────────────────────
      if (wifiDetail) {
        // Currently showing a detail popup - tap anywhere to dismiss it
        wifiDetail=false;
        display.drawWiFiList(ssidList,rssiList,encList,wifiCount,wifiSel,wifiScroll);
        display.drawWiFiStats(wifiCount,wifiStrong);
        push("wifi");
      } else if (y > 305) {
        // Tap the very bottom of screen = go back to menu
        currentState=UI_MENU;
        display.drawMenu(selectedIndex,readBattery());
        push("menu");
      } else {
        // Tap a network in the list - select it or show detail if already selected
        int row = (y-24)/24 + wifiScroll; // calculate which row was tapped
        if (row>=0 && row<wifiCount) {
          if (row==wifiSel) {
            // Tapped the already-selected network - show detail popup
            wifiDetail=true;
            display.drawWiFiDetail(ssidList[row],rssiList[row],encList[row],chanList[row]);
          } else {
            // Tapped a different network - just select it
            wifiSel=row;
            // Auto-scroll so the selected item is visible
            int maxVisible=(320-24-16)/24;
            if (wifiSel<wifiScroll) wifiScroll=wifiSel;
            else if (wifiSel>=wifiScroll+maxVisible) wifiScroll=wifiSel-maxVisible+1;
            display.drawWiFiList(ssidList,rssiList,encList,wifiCount,wifiSel,wifiScroll);
            display.drawWiFiStats(wifiCount,wifiStrong);
          }
          push("wifi");
        }
      }

    } else if (currentState == UI_BLUETOOTH) {
      // ── BLUETOOTH LIST SCREEN ───────────────────────────────────────────────
      // Same pattern as WiFi list but for BLE devices
      if (bleDetail) {
        bleDetail=false;
        display.drawBLEList(bleNameList,bleAddrList,bleRssiList,bleCount,bleSel,bleScroll);
        display.drawBLEStats(bleCount);
        push("ble");
      } else if (y > 305) {
        currentState=UI_MENU;
        display.drawMenu(selectedIndex,readBattery());
        push("menu");
      } else {
        int row=(y-24)/24+bleScroll;
        if (row>=0 && row<bleCount) {
          if (row==bleSel) {
            bleDetail=true;
            display.drawBLEDetail(bleNameList[row],bleAddrList[row],bleRssiList[row]);
          } else {
            bleSel=row;
            int mv=(320-24-16)/24;
            if (bleSel<bleScroll) bleScroll=bleSel;
            else if (bleSel>=bleScroll+mv) bleScroll=bleSel-mv+1;
            display.drawBLEList(bleNameList,bleAddrList,bleRssiList,bleCount,bleSel,bleScroll);
            display.drawBLEStats(bleCount);
          }
          push("ble");
        }
      }

    } else {
      // ── ALL OTHER SCREENS ───────────────────────────────────────────────────
      // For any other screen (port scan, channel map, settings etc.),
      // tapping anywhere returns to the main menu
      currentState=UI_MENU;
      display.drawMenu(selectedIndex, readBattery());
      push("menu");
    }
  }

  // Remember whether the screen was touched this iteration
  // so we can detect the rising edge (not touching → touching) next time
  lastTouch = touching;
}
