#pragma once
#include <NimBLEDevice.h>
#include <NimBLEAdvertising.h>

// Modes
#define BLE_SPAM_APPLE   0
#define BLE_SPAM_ANDROID 1
#define BLE_SPAM_WINDOWS 2
#define BLE_SPAM_ALL     3

class BLESpamAll {
public:
  void begin();
  bool start(int mode = BLE_SPAM_ALL);
  void stop();
  void tick();
  bool isRunning()    { return _running; }
  int  getSentCount() { return _sent; }
  int  getMode()      { return _mode; }

private:
  bool     _running = false;
  int      _sent    = 0;
  int      _mode    = BLE_SPAM_ALL;
  uint32_t _lastTx  = 0;

  void advertiseRaw(uint8_t* data, uint8_t len);
  void sendApple();
  void sendAndroid();
  void sendWindows();
};
