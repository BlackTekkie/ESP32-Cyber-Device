#pragma once
#include <WiFi.h>
#include <WiFiUdp.h>

#define MAX_CHROMECASTS 10

struct ChromecastDevice {
  String ip;
  String name;
  int    port;
};

class ChromecastAttack {
public:
  void begin();
  int  scan();           // discover Chromecasts on network via mDNS/SSDP
  bool cast(int idx, const String& url);  // cast a URL to device
  bool castRickRoll(int idx);
  bool castAll(const String& url);
  void stop();
  bool isRunning()       { return _running; }
  int  getFoundCount()   { return _count; }
  ChromecastDevice get(int i){ return (i>=0&&i<_count)?_devices[i]:ChromecastDevice{}; }

private:
  bool             _running = false;
  int              _count   = 0;
  ChromecastDevice _devices[MAX_CHROMECASTS];
  WiFiUDP          _udp;

  bool sendCastCommand(const String& ip, int port, const String& url);
  int  discoverSSDP();
};
