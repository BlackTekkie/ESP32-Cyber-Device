#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

// PMKID Capture - passive WPA2 hash capture
// Captures PMKIDs from EAPOL msg1 (no client needed)
// Output: hashcat 22000 format ready to crack offline
//
// Hashcat command:
//   hashcat -m 22000 capture.hc22000 wordlist.txt
//
// PMKID = HMAC-SHA1-128(PMK, "PMK Name" || AP_MAC || STA_MAC)
// Found in EAPOL 4-way handshake message 1, RSN IE

#define MAX_PMKID_CAPS 20

struct PMKIDCapture {
  String pmkid;      // 32 hex chars
  String bssid;      // AP MAC
  String clientMac;  // Client MAC  
  String ssid;       // Network name
  uint8_t channel;
  uint32_t timestamp;
};

class PMKIDCapture_t {
public:
  void   begin();
  void   start();
  void   stop();
  void   tick();
  bool   isRunning()     { return _running; }
  int    getCaptureCount(){ return _count; }
  PMKIDCapture& getCapture(int i) { return _caps[i]; }
  void   clear()         { _count=0; }
  String getHashcatFormat(); // returns full 22000 format string
  String getHashcatLine(int i);

private:
  PMKIDCapture _caps[MAX_PMKID_CAPS];
  int      _count   = 0;
  bool     _running = false;
  static void _cb(void* buf, wifi_promiscuous_pkt_type_t type);
  static PMKIDCapture_t* _instance;
  void   _process(const uint8_t* data, int len, uint8_t ch);
  bool   _seen(const String& pmkid);
};

extern PMKIDCapture_t pmkidCapture;
