#pragma once
#include <WiFi.h>

class RickRollBeacon {
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
  int      _idx     = 0;
  uint32_t _lastTx  = 0;
};
