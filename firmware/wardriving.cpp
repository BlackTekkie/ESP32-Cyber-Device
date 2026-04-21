/**
 * wardriving.cpp — Passive AP Logger
 * Logs nearby APs using async WiFi scan so the softAP stays up.
 * CRITICAL: Never call WiFi.mode() — always stay in WIFI_AP_STA.
 * Uses WiFi.scanNetworks(async=true) so it never blocks the loop.
 */
#include "wardriving.h"

void Wardriving::begin() {
  _running=false; _count=0; _totalSeen=0; _scanStarted=false;
}
void Wardriving::start() {
  _running=true; _startTime=millis(); _lastScan=0; _scanStarted=false;
  Serial.println("Wardriving: started");
}
void Wardriving::stop() {
  if(_scanStarted){ WiFi.scanDelete(); _scanStarted=false; }
  _running=false;
  Serial.printf("Wardriving: stopped. APs=%d total=%d\n",_count,_totalSeen);
}
void Wardriving::clear() { _count=0; _totalSeen=0; }

String Wardriving::getEncStr(int enc) {
  switch(enc){
    case WIFI_AUTH_OPEN:           return "Open";
    case WIFI_AUTH_WEP:            return "WEP";
    case WIFI_AUTH_WPA_PSK:        return "WPA";
    case WIFI_AUTH_WPA2_PSK:       return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:   return "WPA/2";
    case WIFI_AUTH_WPA3_PSK:       return "WPA3";
    case WIFI_AUTH_WPA2_ENTERPRISE:return "WPA2-E";
    default:                       return "?";
  }
}
int Wardriving::_findBSSID(const String& b) {
  for(int i=0;i<_count;i++) if(_aps[i].bssid==b) return i;
  return -1;
}
void Wardriving::tick() {
  if(!_running) return;

  if(!_scanStarted) {
    if(millis()-_lastScan < 8000) return; // scan every 8s
    _lastScan = millis();
    // MUST stay AP_STA — never change mode, never call WiFi.mode()
    // async=true means this returns immediately, AP stays up
    int r = WiFi.scanNetworks(true,   // async — does NOT block, does NOT drop AP
                              true,   // show hidden
                              false,  // passive scan (even less disruptive)
                              300);   // 300ms per channel max
    if(r == WIFI_SCAN_FAILED) return;
    _scanStarted = true;
    return;
  }

  int n = WiFi.scanComplete();
  if(n == WIFI_SCAN_RUNNING) return;  // still going — loop keeps running, AP stays up
  if(n < 0){ _scanStarted=false; return; }

  _scanStarted = false;
  uint32_t now = millis();
  _totalSeen += n;

  for(int i=0;i<n&&i<500;i++){
    String bssid = WiFi.BSSIDstr(i);
    int idx = _findBSSID(bssid);
    if(idx>=0){
      _aps[idx].rssi=WiFi.RSSI(i); _aps[idx].lastSeen=now; _aps[idx].seenCount++;
    } else if(_count<MAX_WARDRIVE_APS){
      WardrivedAP& a = _aps[_count++];
      a.ssid      = WiFi.SSID(i).length() ? WiFi.SSID(i) : "[Hidden]";
      a.bssid     = bssid;
      a.rssi      = WiFi.RSSI(i);
      a.channel   = WiFi.channel(i);
      a.enc       = (int)WiFi.encryptionType(i);
      a.firstSeen = now; a.lastSeen = now; a.seenCount = 1;
    }
  }
  WiFi.scanDelete(); // free memory immediately after processing
}
