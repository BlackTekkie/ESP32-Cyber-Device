#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include <Arduino.h>

#define MAX_STATIONS 30

// A WiFi client (station) discovered by passive sniffing
struct Station {
  String  mac;        // client MAC address
  String  apBssid;    // which AP they're connected to (from data frames)
  String  apSSID;     // AP name (looked up from scan)
  int     rssi;       // signal strength
  uint32_t lastSeen;  // millis() timestamp
  int     channel;
  bool    isProbing;  // seen sending probe requests (not yet associated)
};

class StationScanner {
public:
  void begin();
  void start();
  void stop();
  void tick();

  bool     isRunning()        { return _running; }
  int      getCount()         { return _count; }
  Station  getStation(int i)  { return (i>=0&&i<_count)?_stations[i]:Station{}; }
  void     clear()            { _count=0; }

private:
  bool     _running   = false;
  int      _count     = 0;
  uint32_t _lastScan  = 0;
  Station  _stations[MAX_STATIONS];

  static void snifferCb(void* buf, wifi_promiscuous_pkt_type_t type);
  static StationScanner* _instance;

  void handleFrame(const uint8_t* data, int len, int channel, int rssi);
  String macToStr(const uint8_t* m);
  String findSSIDforBSSID(const String& bssid);
};
