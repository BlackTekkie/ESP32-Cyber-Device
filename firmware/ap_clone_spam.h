#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include <Arduino.h>

#define MAX_CLONE_APS 20

struct CloneAP {
  char    ssid[33];
  uint8_t bssid[6];
  int     rssi;
  bool    selected;
};

class APCloneSpam {
public:
  void begin();
  int  scan();                         // returns count found
  void select(int idx, bool on);       // toggle one AP
  void selectAll();
  void clearSelection();
  bool start();
  void stop();
  void tick();

  bool     isRunning()       { return _running; }
  bool     isScanning()      { return _scanning; }
  int      getFoundCount()   { return _count; }
  int      getSelectedCount(){ int n=0;for(int i=0;i<_count;i++)if(_aps[i].selected)n++;return n; }
  int      getSentCount()    { return _sent; }
  CloneAP& getAP(int i)      { return _aps[i]; }

private:
  CloneAP  _aps[MAX_CLONE_APS];
  int      _count   = 0;
  bool     _running = false;
  bool     _scanning= false;
  int      _sent    = 0;
  int      _chIdx   = 0;
  uint32_t _lastTx  = 0;
  uint32_t _lastLog = 0;
};
