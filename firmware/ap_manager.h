#pragma once
#include <WiFi.h>
#include <esp_wifi.h>

#define MAX_CLIENTS 10

struct ClientInfo {
  uint8_t mac[6];
  String  macStr;
  String  ip;
};

class APManager {
public:
  int    refresh();               // re-reads station list, returns count
  bool   kickAll();               // deauthenticates ALL clients from your AP
  String getMAC(int i);
  String getIP(int i);
  int    getCount() { return _count; }

private:
  ClientInfo _clients[MAX_CLIENTS];
  int        _count = 0;
  String     arpLookup(uint8_t* mac);
};
