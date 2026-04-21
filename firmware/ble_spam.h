#pragma once
#include <NimBLEDevice.h>
#include <Arduino.h>

class BLESpam {
public:
  void begin();
  bool start();
  void stop();
  void tick();
  bool     isRunning() { return _running; }
  int      getSent()   { return _sent; }
  String   getLastSSID(){ return _lastName; }

private:
  bool     _running  = false;
  int      _sent     = 0;
  int      _idx      = 0;
  uint32_t _lastTx   = 0;
  String   _lastName = "";
};

extern BLESpam bleSpam;
