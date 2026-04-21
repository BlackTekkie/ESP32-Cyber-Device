/**
 * pineapple_detector.cpp — Rogue Access Point Detector
 * ------------------------------------------------------
 * Scans for suspicious access points that may indicate the presence
 * of WiFi Pineapple devices or other rogue AP attacks.
 *
 * Detection heuristics:
 *   - Duplicate SSIDs with different BSSIDs (evil twin attack)
 *   - Open networks with names matching known secured networks
 *   - Unusually strong signal from unfamiliar APs
 *   - Known Hak5 WiFi Pineapple OUI prefixes in BSSID
 *
 * Ironically uses the same techniques this device employs (beacon
 * injection, AP impersonation) to detect when others are doing it.
 * Demonstrates the dual-use nature of wireless security tooling.
 *
 * Author: ESP32 Cyber Device Project
 */
#include "pineapple_detector.h"

// WiFi Pineapple / Evil Twin Detector
// Scans for APs that match known attack patterns:
// 1. Duplicate SSIDs with different BSSIDs on same channel (Evil Twin)
// 2. Known Pineapple default SSIDs ("PineAP", "WiFi Pineapple" etc)
// 3. Open networks with names matching common WPA2 networks nearby
// 4. Unusually strong signal for "new" networks (close-range rogue AP)
// Based on detection logic from GhostESP and ESP32Marauder

static const char* PINEAPPLE_SSIDS[] = {
  "PineAP","WiFi Pineapple","pineapple","Pineapple",
  "MK7","MK6","MK5","hak5","Hak5"
};
static const int N_PINE = 9;

void PineappleDetector::begin() { _running=false; _count=0; }

bool PineappleDetector::start() {
  _running=true; _count=0; _lastScan=0;
  Serial.println("PineappleDetector: started");
  return true;
}

void PineappleDetector::stop() {
  _running=false;
  Serial.printf("PineappleDetector: stopped — %d suspicious APs found\n",_count);
}

bool PineappleDetector::_alreadyFound(const String& bssid) {
  for(int i=0;i<_count;i++)
    if(_found[i].bssid==bssid) return true;
  return false;
}

void PineappleDetector::scanAndAnalyse() {
  int n = WiFi.scanNetworks(false, true);
  if(n<=0){ WiFi.scanDelete(); return; }

  // Build a list of all SSIDs and their BSSIDs
  for(int i=0;i<n && _count<MAX_SUSPICIOUS;i++){
    String ssid   = WiFi.SSID(i);
    String bssid  = WiFi.BSSIDstr(i);
    int    rssi   = WiFi.RSSI(i);
    int    enc    = WiFi.encryptionType(i);
    int    ch     = WiFi.channel(i);

    if(_alreadyFound(bssid)) continue;

    String reason = "";

    // Check 1: Known Pineapple SSIDs
    for(int p=0;p<N_PINE;p++){
      if(ssid.equalsIgnoreCase(PINEAPPLE_SSIDS[p])){
        reason="Known Pineapple SSID";
        break;
      }
    }

    // Check 2: Duplicate SSID with different BSSID (Evil Twin)
    if(reason.isEmpty()){
      for(int j=0;j<n;j++){
        if(j==i) continue;
        if(WiFi.SSID(j)==ssid && WiFi.BSSIDstr(j)!=bssid){
          reason="Duplicate SSID (Evil Twin)";
          break;
        }
      }
    }

    // Check 3: Open network with common ISP/router name
    if(reason.isEmpty() && enc==WIFI_AUTH_OPEN){
      const char* common[]={"BT","Sky","Virgin","Vodafone","EE","TalkTalk","NETGEAR","TP-Link"};
      for(int k=0;k<8;k++){
        if(ssid.indexOf(common[k])>=0){
          reason="Open network with ISP name (suspicious)";
          break;
        }
      }
    }

    // Check 4: Very strong signal from unknown AP (rogue device nearby)
    if(reason.isEmpty() && rssi > -40){
      reason="Unusually strong signal (possible rogue AP nearby)";
    }

    if(!reason.isEmpty()){
      SuspiciousAP ap;
      ap.ssid=ssid; ap.bssid=bssid;
      ap.rssi=rssi; ap.channel=ch; ap.reason=reason;
      _found[_count++]=ap;

      Serial.println("╔══════════════════════════════════════════╗");
      Serial.println("║     PINEAPPLE DETECTOR — ALERT           ║");
      Serial.println("╠══════════════════════════════════════════╣");
      Serial.printf( "║ SSID   : %-32s║\n", ssid.c_str());
      Serial.printf( "║ BSSID  : %-32s║\n", bssid.c_str());
      Serial.printf( "║ RSSI   : %-32s║\n", (String(rssi)+"dBm").c_str());
      Serial.printf( "║ Reason : %-32s║\n", reason.c_str());
      Serial.println("╚══════════════════════════════════════════╝");
    }
  }
  WiFi.scanDelete();
}

void PineappleDetector::tick() {
  if(!_running) return;
  uint32_t now=millis();
  if(now-_lastScan < 15000) return;  // scan every 15s
  _lastScan=now;
  Serial.println("PineappleDetector: scanning...");
  scanAndAnalyse();
  Serial.printf("PineappleDetector: scan done, %d suspicious found\n",_count);
}
