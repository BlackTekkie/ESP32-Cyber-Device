#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define MAX_CRACK_WORDLIST 60

struct CrackResult {
  bool     found      = false;
  String   password   = "";
  String   ssid       = "";
  int      tried      = 0;
  int      total      = 0;
  String   current    = "";
  bool     running    = false;
  bool     done       = false;
};

class WiFiCracker {
public:
  void begin();
  // Start cracking against a target SSID + PMKID capture
  bool startCrack(const String& ssid, const String& bssidHex,
                  const String& clientHex, const String& pmkidHex);
  void stop();
  void tick();

  bool         isRunning()  { return _result.running; }
  bool         isDone()     { return _result.done; }
  CrackResult& getResult()  { return _result; }

  static void  _crackTask(void* param);
  bool         _computeAndCheck(const char* password);

private:
  CrackResult  _result;
  String       _targetSSID;
  String       _targetBSSID;   // hex no colons
  String       _targetClient;  // hex no colons
  String       _targetPMKID;   // hex 32 chars

  static const char* WORDLIST[];
  static const int   WORDLIST_SIZE;
};

extern WiFiCracker wifiCracker;
