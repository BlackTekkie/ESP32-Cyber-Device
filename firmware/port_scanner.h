#pragma once
#include <WiFi.h>

#define MAX_OPEN_PORTS 20

struct PortResult {
  int    port;
  bool   open;
  String service;
};

class PortScanner {
public:
  // Scan common TCP ports on a target IP on your own subnet.
  // timeout_ms: per-port TCP connect timeout (100–300ms recommended)
  int scan(String targetIP, int timeout_ms = 150);

  int         getOpenCount()        { return _openCount; }
  PortResult  getResult(int i)      { return (i<0||i>=_openCount)?PortResult{}:_open[i]; }
  String      getTargetIP()         { return _target; }

private:
  PortResult _open[MAX_OPEN_PORTS];
  int        _openCount = 0;
  String     _target;

  String serviceName(int port);
};
