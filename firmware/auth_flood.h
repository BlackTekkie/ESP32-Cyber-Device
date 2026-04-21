#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include <Arduino.h>

// Auth Flood - fills AP association table with fake clients
// Sends 802.11 authentication frames from random MACs
// Crashes router by exhausting its client table

#define MAX_AUTH_TARGETS 20

struct AuthTarget {
  String  ssid;
  String  bssid;
  uint8_t bssidBytes[6];
  int     channel;
  bool    selected;
};

class AuthFlood {
public:
  void begin();
  void tick();
  void stop();
  bool isRunning()     { return _running; }
  int  getScanCount()  { return _count; }
  int  getSentCount()  { return _sent; }
  void scan();
  void scanStart();
  bool isScanning() { return _scanning; }
  void startSelected();
  void startAll();
  void select(int i, bool on);
  void selectAll();
  void clearSelection();
  AuthTarget& getTarget(int i) { return _targets[i]; }

private:
  AuthTarget _targets[MAX_AUTH_TARGETS];
  int      _count  = 0;
  bool     _scanning = false;
  bool     _running= false;
  int      _sent   = 0;
  uint32_t _lastTx = 0;
  int      _curTgt = 0;
  void _sendAuth(uint8_t* bssid, int channel);
};

extern AuthFlood authFlood;
