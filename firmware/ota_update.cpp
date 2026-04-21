#include "ota_update.h"

OTAUpdate otaUpdate;

void OTAUpdate::begin(const String& hostname) {
  ArduinoOTA.setHostname(hostname.c_str());
  ArduinoOTA.setPassword("esp32cyber");  // OTA password

  ArduinoOTA.onStart([this](){
    _updating = true; _progress = 0;
    String type = ArduinoOTA.getCommand()==U_FLASH ? "firmware" : "filesystem";
    Serial.println("OTA: starting update — " + type);
  });

  ArduinoOTA.onEnd([this](){
    _updating = false;
    Serial.println("\nOTA: complete — rebooting");
  });

  ArduinoOTA.onProgress([this](unsigned int done, unsigned int total){
    _progress = (done * 100) / total;
    if(_progress % 10 == 0)
      Serial.printf("OTA: %d%%\n", _progress);
  });

  ArduinoOTA.onError([this](ota_error_t err){
    _updating = false;
    Serial.printf("OTA: error[%u] ", err);
    if(err==OTA_AUTH_ERROR)         Serial.println("auth failed");
    else if(err==OTA_BEGIN_ERROR)   Serial.println("begin failed");
    else if(err==OTA_CONNECT_ERROR) Serial.println("connect failed");
    else if(err==OTA_RECEIVE_ERROR) Serial.println("receive failed");
    else if(err==OTA_END_ERROR)     Serial.println("end failed");
  });

  ArduinoOTA.begin();
  Serial.println("OTA: ready — password: esp32cyber");
}

void OTAUpdate::tick() {
  ArduinoOTA.handle();
}
