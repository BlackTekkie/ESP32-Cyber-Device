#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include <Arduino.h>

#define MAX_PROBE_RESULTS 40

struct ProbeResult {
  String  clientMAC;  // device that sent the probe
  String  ssid;       // network they're looking for
  int     rssi;
  uint32_t lastSeen;
  int     count;      // how many times seen probing for this SSID
};

class ProbeSniffer {
public:
  void begin();
  void start();
  void stop();
  void tick();

  bool        isRunning()     { return _running; }
  int         getCount()      { return _count; }
  ProbeResult getResult(int i){ return (i>=0&&i<_count)?_results[i]:ProbeResult{}; }
  void        clear()         { _count=0; }

private:
  bool        _running  = false;
  int         _count    = 0;
  uint32_t    _lastHop  = 0;
  uint8_t     _channel  = 1;
  ProbeResult _results[MAX_PROBE_RESULTS];

  static void         snifferCb(void* buf, wifi_promiscuous_pkt_type_t type);
  static ProbeSniffer* _instance;
  void handleFrame(const uint8_t* data, int len, int rssi);
  String macToStr(const uint8_t* m);
};
