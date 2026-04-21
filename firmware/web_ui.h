#pragma once
#include "cyber_storage.h"
#include "bad_ble.h"
#include <DNSServer.h>
#include "station_scanner.h"
#include "probe_sniffer.h"
#include "ota_update.h"
#include "dns_spoof.h"
#include "karma_attack.h"
#include "pineapple_detector.h"
#include "deauth_attack.h"
#include "auth_flood.h"
#include "pmkid_capture.h"
#include "ble_attacks_extra.h"
#include "wardriving.h"
#include "chromecast_attack.h"
#include "ap_clone_spam.h"
#include "ble_spam_all.h"
#include "eapol_sniffer.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#define AP_SSID "ESP32-Cyber"
#define AP_PASS "esp32cyber"

enum WebCommand {
  CMD_NONE,
  CMD_PIN,
  CMD_GOTO_MENU,
  CMD_SCAN_WIFI,
  CMD_SCAN_BLE,
  CMD_SCAN_NET,
  CMD_AP_REFRESH,
  CMD_AP_KICK_ALL,
  CMD_PORT_SCAN,
  CMD_CHANNEL_ANALYSE,
  CMD_GOTO_SETTINGS,
  CMD_SELECT_WIFI,
  CMD_SELECT_BLE,
  CMD_BACK,
  CMD_REBOOT,
  // ── Attacks ──
  CMD_ATK_BEACON_LIST,    // index=duration_ms, strVal=comma-separated SSIDs
  CMD_ATK_BEACON_RANDOM,  // index=duration_ms
  CMD_ATK_EVIL_PORTAL,    // starts evil portal (blocks web UI)
  CMD_ATK_RICK_ROLL,      // index=duration_ms
  CMD_ATK_PROBE_FLOOD,    // index=duration_ms
  CMD_ATK_AP_CLONE_SCAN,
  CMD_ATK_AP_CLONE,
  CMD_ATK_KARMA,
  CMD_ATK_KARMA_SCAN,
  CMD_ATK_BLE_SPAM,
  CMD_ATK_BLE_SPAM_ALL,
  CMD_ATK_EAPOL,
  CMD_ATK_PINEAPPLE,
  CMD_ATK_CC_SCAN,
  CMD_ATK_CC_RICKROLL,
  CMD_ATK_SSID_CONF,
  CMD_SET_PIN,
  CMD_PACKET_COUNT_START,
  CMD_PACKET_COUNT_STOP,
  CMD_DNS_START,
  CMD_DNS_STOP,
  CMD_CLEAR_CREDS,
  CMD_BADBLE_START,
  CMD_BADBLE_STOP,
  CMD_BADBLE_RUN,
  CMD_EP_CLEAR,
  CMD_WIFI_CRACK_START,
  CMD_WIFI_CRACK_STOP,
  CMD_EAPOL_SET_CH,
  CMD_EAPOL_TARGETED,
  CMD_DEAUTH_SCAN,
  CMD_DEAUTH_ALL,
  CMD_DEAUTH_SEL,
  CMD_DEAUTH_STOP,
  CMD_STATION_SCAN_START,
  CMD_STATION_SCAN_STOP,
  CMD_PROBE_SNIFF_START,
  CMD_PROBE_SNIFF_STOP,
  CMD_WARDRIVE_START,
  CMD_WARDRIVE_STOP,
  CMD_WARDRIVE_CLEAR,
  CMD_AUTH_FLOOD_SCAN,
  CMD_AUTH_FLOOD_ALL,
  CMD_AUTH_FLOOD_SEL,
  CMD_AUTH_FLOOD_STOP,
  CMD_PMKID_START,
  CMD_PMKID_STOP,
  CMD_AIRDROP_START,
  CMD_AIRDROP_STOP,
  CMD_CTRL_SPAM,
  CMD_CTRL_STOP,
  CMD_SCAN_TRACKERS,
  CMD_NAME_SPOOF_START,
  CMD_NAME_SPOOF_STOP,
  CMD_ATK_BEACON_STOP,
  CMD_STOP_ALL,
};

struct WebCommandData {
  WebCommand cmd;
  int        index;
  String     strVal;
};

struct WebPortResult {
  int    port;
  String service;
};

class WebUI {
public:
  bool begin();
  void stopServer();
  void setPortalMode(bool active, int tplId=0);
  bool isPortalActive() { return _portalActive; }         // release port 80
  WebServer* getServer()     { return &server; } // borrow server to evil portal
  void registerRoot();       // re-register / after portal stops
  void handleKarmaCreds();
  void handleKarmaConns();
  void handlePineResults();
  void handleCCDevices();
  void handleStoredCreds();
  void handleBadBLEState();
  void handleAuthFloodScan();
  void handlePMKIDData();
  void handlePMKIDExport();
  void handleTrackerData();
  void handleDeauthScan();
  void handleDeauthState();
  void handleWardriveData();
  void handleStationData();
  void handleProbeData();
  void handleEPCreds();
  void handleEPConns();
  void handleKarmaPortal();
  void handleCrackState();
  void handleKarmaLogin();
  void handleOTAState();
  void handleFullState();
  void handleKarmaState();
  void handleKarmaScan();
  void handleKarmaSelect();
  void handleAPCScan();
  void handleEapolCaps();
  void handleAPCSelect();
  void handle();
  bool hasCommand();
  WebCommandData getCommand();
  String getIP();
  void setPinResult(bool correct);
  void setBatPct(int pct) { _batPct = pct; }
  void setCrackerPayload(const String& s){ crackerPayload=s; }
  void setPin(const String& pin)          { _pin = pin; }
  void setPinePayload(const String& json) { pinePayload = json; }
  void setCCPayload(const String& json)          { ccPayload = json; }
  void setDeauthScanPayload(const String& json)  { deauthScanPayload = json; }
  void setDeauthStatePayload(const String& json) { deauthStatePayload = json; }
  void setWardrivePayload(const String& json)    { wardrivePayload = json; }
  void setBadBLEPayload(const String& json)      { badBLEPayload = json; }
  void setAuthFloodPayload(const String& json)   { authFloodPayload = json; }
  void setPMKIDPayload(const String& json, const String& exp) { pmkidPayload=json; pmkidExport=exp; }
  void setTrackerPayload(const String& json)     { trackerPayload = json; }

  void pushState(
    String stateName,
    String ssid[], int wRssi[], int enc[], int chan[], int wCount,
    String bName[], String bAddr[], int bRssi[], int bCount,
    String netIP[], String netMAC[], int nCount,
    String apIP[], String apMAC[], int apCount,
    int portPorts[], String portSvcs[], int portCount, String portTarget,
    int chNets[], int chMax,
    int wSel, int bSel,
    int battery
  );

private:
  WebServer      server{80};
  WebCommandData pendingCmd     = {CMD_NONE, 0, ""};
  bool           cmdReady       = false;
  bool           _portalActive  = false;
  int            _portalTplId   = 0;
  DNSServer      _portalDns;

  // In-RAM capture (also persisted to flash via cyberStorage)
  static const int MAX_PORTAL_CAP = 20;
  struct PortalConn { String ip, mac, ua; };
  struct PortalCred { String user, pass, ip, mac; };
  PortalConn  _portalConns[20];
  int         _portalConnCount = 0;
  PortalCred  _portalCreds[20];
  int         _portalCredCount = 0;
  String         _pin            = "";
  String         pinePayload        = "[]";
  String         ccPayload          = "[]";
  String         deauthScanPayload  = "[]";
  String         deauthStatePayload = "{\"running\":false,\"count\":0}";
  String         wardrivePayload    = "{\"running\":false,\"count\":0,\"total\":0,\"aps\":[]}";
  String         badBLEPayload      = "{\"running\":false,\"connected\":false,\"busy\":false,\"status\":\"Idle\"}";
  String         authFloodPayload   = "[]";
  String         pmkidPayload       = "{\"count\":0,\"caps\":[]}";
  String         pmkidExport        = "";
  String         trackerPayload     = "{\"count\":0,\"trackers\":[]}";
  String         statePayload   = "{}";
  int            _batPct         = 0;
  String         crackerPayload  = "{}";
  bool           pinResultReady = false;
  bool           _routesRegistered = false;
  bool           pinCorrect     = false;

  void handleRoot();
  void handleGetState();
  void handleCommand();
};
