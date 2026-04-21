#pragma once
#include <WiFi.h>
#include <esp_wifi.h>

class SSIDConfusion {
public:
  void begin();
  bool start();
  void stop();
  void tick();
  bool isRunning()    { return _running; }
  int  getSentCount() { return _sent; }

private:
  bool     _running = false;
  int      _sent    = 0;
  uint32_t _lastTx  = 0;
  uint8_t  _idx     = 0;

  // Beacon frame buffer
  uint8_t  _pkt[109];
  void     buildBeacon(const char* ssid, uint8_t num);
};
