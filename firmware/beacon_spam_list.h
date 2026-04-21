#pragma once
#include <WiFi.h>
#include <esp_wifi.h>

#define MAX_SPAM_SSIDS 20

class BeaconSpamList {
public:
  void begin();
  void addSSID(const char* ssid);
  void clearSSIDs();
  bool start();
  void stop();
  void tick();
  bool isRunning()    { return _running; }
  int  getCount()     { return _count; }
  int  getSentCount() { return _sent; }

private:
  char     _ssids[MAX_SPAM_SSIDS][33];
  int      _count   = 0;
  bool     _running = false;
  int      _sent    = 0;
  int      _idx     = 0;
  int      _chIdx   = 0;
  uint32_t _lastTx  = 0;
};
