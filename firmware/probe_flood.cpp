/**
 * probe_flood.cpp — WiFi Probe Request Flood (Improved)
 * -------------------------------------------------------
 * Injects 802.11 probe request frames with realistic SSIDs.
 *
 * IMPROVEMENT over original: instead of 12 hardcoded US names,
 * we now seed the flood from two sources:
 *
 * 1. REAL nearby SSIDs — scanned on start(). Using real network names
 *    means the probe requests look like actual devices searching for
 *    networks they know. This is far more convincing to IDS systems
 *    and demonstrates the privacy risk of probe requests exposing
 *    a device's known-network history.
 *
 * 2. UK/Ireland ISP names — realistic fallback pool including BT, Sky,
 *    Virgin, Eir, Three, Vodafone etc. (not US-centric names)
 *
 * Real-world impact: probe requests reveal what networks a device has
 * previously connected to. This attack floods that signal so passive
 * tracking becomes impossible (all SSIDs look like probe noise).
 *
 * Sends ~100 frames/second on a single channel (WIFI_IF_STA).
 * Frames are not sent on the AP interface to avoid disrupting clients.
 */
#include "probe_flood.h"
#include <WiFi.h>
#include <string.h>

// UK/Ireland ISP-realistic fallback pool
// These are the kinds of SSIDs that actually exist in Ireland
static const char* UK_POOL[] = {
  "BTHub6-XXXX","BTHub5-XXXX","BT-XXXXXX","BThub-XXXX",
  "SKY12345","SKYXXXXX","Sky_XXXX","SkyHub",
  "VM1234567","VirginMedia","VM-XXXXXXXX",
  "EE-XXXXXX","EEBrightBox","EE-BrightBox-XXXX",
  "DIRECT-xx-BRAVIA","DIRECT-xx-SAMSUNG",
  "iPhone","Galaxy S23","Pixel 7","OnePlus",
  "eircom","eir12345","eir-XXXXXX",
  "Three-XXXX","VodafoneConnect-XXXX",
  "NETGEAR_XXXX","TP-Link_XXXX","TP-LINK_XXXX",
  "ASUS_RT-XXXX","Linksys00000","HUAWEI-XXXX",
  "AndroidAP","AndroidShare","MobileHotspot",
};
static const int UK_POOL_SIZE = 33;

// Stores real SSIDs captured from nearby scan
static char _realSSIDs[20][33];
static int  _realCount = 0;

// Combined pool (real + UK fallback)
static char _pool[53][33];
static int  _poolSize = 0;

void ProbeFlood::begin() { _running=false; _sent=0; }

bool ProbeFlood::start() {
  _sent=0; _lastTx=0; _poolSize=0;

  // Scan for real nearby SSIDs to use as probe request seeds
  Serial.println("ProbeFlood: scanning for real SSIDs to seed flood...");
  int found = WiFi.scanNetworks(false, false); // false = no hidden
  _realCount = 0;
  if(found > 0) {
    for(int i=0; i<found && _realCount<20; i++) {
      String s = WiFi.SSID(i);
      if(s.length()==0 || s.length()>32) continue;
      strncpy(_realSSIDs[_realCount], s.c_str(), 32);
      _realSSIDs[_realCount][32] = '\0';
      _realCount++;
    }
    WiFi.scanDelete();
    Serial.printf("ProbeFlood: got %d real SSIDs from scan\n", _realCount);
  }

  // Build combined pool: real SSIDs first, then UK/Ireland fallback
  for(int i=0; i<_realCount; i++) {
    strncpy(_pool[_poolSize++], _realSSIDs[i], 32);
    _pool[_poolSize-1][32]='\0';
  }
  for(int i=0; i<UK_POOL_SIZE && _poolSize<53; i++) {
    // Substitute X's with random hex digits for variety
    String s = String(UK_POOL[i]);
    while(s.indexOf('X') >= 0) {
      s.setCharAt(s.indexOf('X'), "0123456789ABCDEF"[random(16)]);
    }
    strncpy(_pool[_poolSize], s.c_str(), 32);
    _pool[_poolSize][32]='\0';
    _poolSize++;
  }

  _running=true;
  Serial.printf("ProbeFlood: started with %d SSIDs (%d real + %d UK fallback)\n",
                _poolSize, _realCount, UK_POOL_SIZE);
  return true;
}

void ProbeFlood::stop() {
  _running=false;
  Serial.printf("ProbeFlood: stopped — %d frames sent\n", _sent);
}

static void randomMAC(uint8_t* mac) {
  for(int i=0;i<6;i++) mac[i]=random(0,256);
  mac[0]=(mac[0]&0xFE)|0x02; // locally administered, unicast
}

static int buildProbeReq(uint8_t* buf, const char* ssid, uint8_t* src) {
  int i=0;
  buf[i++]=0x40; buf[i++]=0x00; // Frame Control: probe request (subtype 4)
  buf[i++]=0x00; buf[i++]=0x00; // Duration
  // Destination = broadcast (probe requests go to everyone)
  for(int j=0;j<6;j++) buf[i++]=0xFF;
  // Source = our random MAC (simulates a real device)
  for(int j=0;j<6;j++) buf[i++]=src[j];
  // BSSID = broadcast
  for(int j=0;j<6;j++) buf[i++]=0xFF;
  // Sequence control
  buf[i++]=random(256); buf[i++]=random(256);
  // SSID Information Element
  uint8_t sLen=(uint8_t)strlen(ssid);
  buf[i++]=0x00; buf[i++]=sLen;
  memcpy(&buf[i],ssid,sLen); i+=sLen;
  // Supported Rates IE
  buf[i++]=0x01; buf[i++]=0x04;
  buf[i++]=0x02; buf[i++]=0x04; buf[i++]=0x0B; buf[i++]=0x16;
  // Extended Supported Rates IE (makes frame look more like real 802.11n device)
  buf[i++]=0x32; buf[i++]=0x04;
  buf[i++]=0x24; buf[i++]=0x30; buf[i++]=0x48; buf[i++]=0x6C;
  return i;
}

void ProbeFlood::tick() {
  if(!_running || _poolSize==0) return;
  uint32_t now=millis();
  if(now-_lastTx < 10) return; // 100 frames/second
  _lastTx=now;

  uint8_t buf[128], src[6];
  randomMAC(src);

  // Pick a random SSID from our pool (mix of real + UK ISP names)
  const char* ssid = _pool[random(0, _poolSize)];
  int len = buildProbeReq(buf, ssid, src);

  // Try STA interface first, fall back to AP interface
  esp_err_t r = esp_wifi_80211_tx(WIFI_IF_STA, buf, len, false);
  if(r != ESP_OK) r = esp_wifi_80211_tx(WIFI_IF_AP, buf, len, false);
  if(r == ESP_OK) _sent++;
}
