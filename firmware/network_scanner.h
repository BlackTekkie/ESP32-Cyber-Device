#pragma once
#include <WiFi.h>

#define MAX_HOSTS 10

struct HostInfo {
  String ip;
  String mac;
  bool   reachable;
};

class NetworkScanner {
public:
  int    scan();
  String getIP(int i);
  String getMAC(int i);
  int    getCount() { return _count; }

private:
  HostInfo _hosts[MAX_HOSTS];
  int      _count = 0;
};
