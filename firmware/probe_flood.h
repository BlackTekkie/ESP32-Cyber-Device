#pragma once
#include <WiFi.h>
#include <esp_wifi.h>

class ProbeFlood {
public:
  void begin();
  bool start();
  void stop();
  void tick();
  bool isRunning()    { return _running; }
  int  getSentCount() { return _sent; }

private:
  bool     _running = false;
  int      _sent    = 0;
  uint32_t _lastTx  = 0;
  // No member function declarations - helpers are static free functions in .cpp
};
