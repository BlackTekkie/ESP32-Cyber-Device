#pragma once
#include <WiFi.h>
#include <ArduinoOTA.h>

// OTA firmware update over WiFi
// Upload new .bin from Arduino IDE or web browser
// Requires being connected to a WiFi network (not just AP mode)

class OTAUpdate {
public:
  void begin(const String& hostname = "esp32-cyber");
  void tick();
  bool isUpdating()  { return _updating; }
  int  getProgress() { return _progress; }

private:
  bool _updating = false;
  int  _progress = 0;
};

extern OTAUpdate otaUpdate;
