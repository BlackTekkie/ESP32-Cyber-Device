#pragma once
#include <WiFi.h>
#include <esp_wifi.h>

#define MAX_SUSPICIOUS 20

struct SuspiciousAP {
  String ssid;
  String bssid;
  int    rssi;
  int    channel;
  String reason;
};

class PineappleDetector {
public:
  void begin();
  bool start();
  void stop();
  void tick();
  bool isRunning()       { return _running; }
  int  getFoundCount()   { return _count; }
  SuspiciousAP get(int i){ return (i>=0&&i<_count)?_found[i]:SuspiciousAP{}; }

private:
  bool         _running  = false;
  int          _count    = 0;
  uint32_t     _lastScan = 0;

  SuspiciousAP _found[MAX_SUSPICIOUS];
  bool         _alreadyFound(const String& bssid);
  void         scanAndAnalyse();
};
