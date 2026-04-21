#pragma once
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEHIDDevice.h>
#include <NimBLECharacteristic.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Arduino.h>

#define KEY_MOD_NONE   0x00
#define KEY_MOD_LSHIFT 0x02
#define KEY_MOD_LCTRL  0x01
#define KEY_MOD_LGUI   0x08

enum BadBLEPayload {
  BADBLE_CUSTOM       = 0,
  BADBLE_OPEN_CMD     = 1,
  BADBLE_DUMP_WIFI    = 2,
  BADBLE_RICKROLL     = 3,
  BADBLE_NOTEPAD_MSG  = 4,
  BADBLE_POWERSHELL   = 5,
};

class BadBLE : public NimBLEServerCallbacks {
public:
  void begin();
  bool startAdvertising(const String& deviceName);
  void stop();
  void tick();
  void runPayload(BadBLEPayload type, const String& custom = "");

  bool   isRunning()   { return _running; }
  bool   isConnected() { return _connected; }
  bool   isBusy()      { return _busy; }
  String getStatus()   { return _status; }

  void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override;
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& info, int reason) override;

  static void _payloadTask(void* param);

  // Public for task access
  NimBLECharacteristic* _input       = nullptr;
  BadBLEPayload         _pendingType = BADBLE_CUSTOM;
  String                _pendingPayload;
  String                _status      = "Idle";
  uint32_t              _connectedAt = 0;
  volatile bool         _busy        = false;
  volatile bool         _taskRunning = false;
  volatile bool         _hasPending  = false;
  bool                  _connected   = false;
  bool                  _running     = false;
  int                   _connCount   = 0;

  void    _pressKey(uint8_t mod, uint8_t key);
  void    _releaseKeys();
  void    _typeString(const String& s);
  void    _openRun();
  uint8_t _charToKeycode(char c, uint8_t& mod);

private:
  bool             _ready  = false;
  NimBLEServer*    _server = nullptr;
  NimBLEHIDDevice* _hid    = nullptr;
};

extern BadBLE badBLE;
