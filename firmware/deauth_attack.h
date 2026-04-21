#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include <Arduino.h>

#define MAX_DEAUTH_TARGETS 20

struct DeauthTarget {
  String ssid;
  String bssid;
  uint8_t bssidBytes[6];
  int     channel;
  int     rssi;
  bool    selected;
};

class DeauthAttack {
public:
  void begin();
  void tick();
  void stop();
  bool isRunning()     { return _running; }
  int  getScanCount()  { return _count; }
  bool scanJustDone() { bool v=_scanJustDone; _scanJustDone=false; return v; }
  int  getDeauthCount(){ return _deauthCount; }

  // Scan for targets
  void scan();
  void scanStart();
  bool isScanning() { return _scanning; }

  // Start deauth on selected targets
  void startSelected();
  // Start deauth on all scanned APs
  void startAll();
  // Deauth a single AP by index
  void startSingle(int idx);

  // Target selection
  void select(int idx, bool on);
  void selectAll();
  void clearSelection();

  DeauthTarget& getTarget(int i) { return _targets[i]; }

private:
  DeauthTarget _targets[MAX_DEAUTH_TARGETS];
  int          _count       = 0;
  bool         _scanJustDone = false;
  bool         _running     = false;
  int          _deauthCount = 0;
  uint32_t     _lastSend    = 0;
  int          _currentTarget = 0;
  bool         _scanning    = false;

  void _sendDeauth(uint8_t* bssid, int channel);
  void _parseBSSID(const String& s, uint8_t* out);
};

extern DeauthAttack deauthAttack;
