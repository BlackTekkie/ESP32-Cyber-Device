/**
 * probe_sniffer.cpp — Passive Probe Request Sniffer
 * --------------------------------------------------
 * Passively captures 802.11 probe request frames to build a profile
 * of what WiFi networks nearby devices are searching for.
 *
 * Privacy implication: every device with WiFi enabled constantly
 * broadcasts probe requests for every network it has ever connected to.
 * This leaks the device's location history — if someone probes for
 * "Starbucks_WiFi" and "HeathrowAirport_Free" you know where they've been.
 *
 * Security use: the probe list is the perfect seed for a Karma attack.
 * Instead of responding to everything, use these results to only respond
 * to SSIDs that real nearby devices are actually looking for.
 *
 * This is a key Marauder feature we were missing.
 */
#include "probe_sniffer.h"
#include <string.h>

ProbeSniffer* ProbeSniffer::_instance = nullptr;

void ProbeSniffer::begin()  { _running=false; _count=0; _instance=this; }

void ProbeSniffer::start() {
  _running=true; _count=0; _lastHop=0; _channel=1;

  wifi_promiscuous_filter_t f;
  f.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT; // management frames only
  esp_wifi_set_promiscuous_filter(&f);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(snifferCb);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  Serial.println("ProbeSniffer: started - listening for probe requests");
}

void ProbeSniffer::stop() {
  _running=false;
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  delay(30);
  esp_wifi_set_promiscuous(false);
  Serial.printf("ProbeSniffer: stopped - %d unique probes captured\n", _count);
}

void ProbeSniffer::snifferCb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if(!_instance || !_instance->_running) return;
  if(type != WIFI_PKT_MGMT) return;
  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  _instance->handleFrame(pkt->payload, pkt->rx_ctrl.sig_len, pkt->rx_ctrl.rssi);
}

String ProbeSniffer::macToStr(const uint8_t* m) {
  char buf[18];
  snprintf(buf,sizeof(buf),"%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
  return String(buf);
}

void ProbeSniffer::handleFrame(const uint8_t* data, int len, int rssi) {
  if(len < 24) return;

  // Check it's a probe request: FC byte 0 = 0x40 (type=mgmt, subtype=4)
  if(data[0] != 0x40) return;

  // Extract source MAC (addr2 = bytes 10-15)
  const uint8_t* src = data + 10;
  // Skip broadcast/multicast MACs
  if(src[0] == 0xFF) return;
  if(src[0] & 0x01) return;

  String clientMAC = macToStr(src);

  // Extract SSID from the first Information Element
  // 802.11 header is 24 bytes, then IEs start
  int ieOffset = 24;
  if(len <= ieOffset + 2) return;

  uint8_t ieTag = data[ieOffset];
  uint8_t ieLen = data[ieOffset + 1];

  // Tag 0 = SSID IE
  if(ieTag != 0x00) return;
  if(ieLen == 0) return;  // wildcard probe (looking for any network)
  if(ieOffset + 2 + ieLen > len) return;

  // Extract the SSID string
  char ssidBuf[33];
  int copyLen = min((int)ieLen, 32);
  memcpy(ssidBuf, data + ieOffset + 2, copyLen);
  ssidBuf[copyLen] = '\0';

  // Filter out non-printable SSIDs
  for(int i=0; i<copyLen; i++)
    if(ssidBuf[i] < 32) ssidBuf[i] = '?';

  String ssid = String(ssidBuf);
  String key  = clientMAC + "|" + ssid;

  // Update existing entry for this client+SSID pair
  for(int i=0; i<_count; i++) {
    if(_results[i].clientMAC == clientMAC && _results[i].ssid == ssid) {
      _results[i].lastSeen = millis();
      _results[i].count++;
      if(rssi > _results[i].rssi) _results[i].rssi = rssi;
      return;
    }
  }

  // New probe result
  if(_count < MAX_PROBE_RESULTS) {
    ProbeResult& r = _results[_count++];
    r.clientMAC = clientMAC;
    r.ssid      = ssid;
    r.rssi      = rssi;
    r.lastSeen  = millis();
    r.count     = 1;
    Serial.printf("Probe: %s -> \"%s\" (%ddBm)\n",
                  clientMAC.c_str(), ssid.c_str(), rssi);
  }
}

void ProbeSniffer::tick() {
  if(!_running) return;
  uint32_t now = millis();
  // Hop all 13 channels at 200ms each - longer dwell than station scanner
  // because probe requests are infrequent and we want to catch all of them
  if(now - _lastHop < 200) return;
  _lastHop = now;
  _channel = (_channel % 13) + 1;
  esp_wifi_set_channel(_channel, WIFI_SECOND_CHAN_NONE);
}
