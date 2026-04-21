#pragma once
#include <WiFi.h>
#include <Arduino.h>
#include "cyber_storage.h"

#define MAX_WARDRIVE_APS 200

struct WardrivedAP {
  String  ssid;
  String  bssid;
  int     rssi;
  int     channel;
  int     enc;       // WIFI_AUTH_* enum value
  uint32_t firstSeen;
  uint32_t lastSeen;
  int     seenCount;
};

class Wardriving {
public:
  void   begin();
  void   start();
  void   stop();
  void   tick();
  bool   isRunning()  { return _running; }
  int    getCount()   { return _count; }
  int    getTotalSeen(){ return _totalSeen; }
  WardrivedAP& getAP(int i) { return _aps[i]; }
  void   clear();
  String getEncStr(int enc);

private:
  WardrivedAP _aps[MAX_WARDRIVE_APS];
  int         _count     = 0;
  int         _totalSeen = 0;
  bool        _running   = false;
  uint32_t    _lastScan  = 0;
  uint32_t    _startTime = 0;

  int  _findBSSID(const String& bssid);
  void _doScan();
  bool _scanStarted = false;
};

extern Wardriving wardriving;
