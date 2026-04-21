#pragma once
#include <WiFi.h>
#include <esp_wifi.h>

#define MAX_BSR_SSIDS 30

class BeaconSpamRandom {
public:
  void begin();
  bool start(int count = 20);
  void stop();
  void tick();
  bool   isRunning()    { return _running; }
  int    getSentCount() { return _sent; }
  String getLastSSID()  { return _lastSSID; }

private:
  static const int CACHE = 30;
  char     _ssids[CACHE][33];
  int      _cacheCount = 0;
  bool     _running    = false;
  int      _sent       = 0;
  int      _chIdx      = 0;
  uint8_t  _channel    = 1;
  uint32_t _lastTx     = 0;
  String   _lastSSID   = "";

  void generateSSIDs(int count);
};
