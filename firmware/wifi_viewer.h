#pragma once
#include <WiFi.h>

#define MAX_NETWORKS 20

class WiFiViewer {
public:
  void begin();
  int scan();
  String getSSID(int i);
  int    getRSSI(int i);
  int    getEncryption(int i);
  int    getChannel(int i);
  bool   hasWPS(int i);           // true if network likely has WPS enabled
  String getManufacturer(int i);  // OUI-based manufacturer identification
private:
  int networkCount = 0;
};
