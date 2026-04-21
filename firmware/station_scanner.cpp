/**
 * station_scanner.cpp — WiFi Station (Client) Scanner
 * -----------------------------------------------------
 * Passively sniffs 802.11 frames to discover client devices and
 * which AP they are connected to.
 *
 * This is a key feature of Marauder and Ghost ESP that we were missing.
 * Rather than scanning for APs (which advertise themselves), this finds
 * the CLIENTS — phones, laptops, IoT devices — that are connected to APs.
 *
 * How it works:
 *   Data frames have a from-DS/to-DS bit that tells us direction.
 *   to-DS (client → AP):   addr1=BSSID, addr2=client MAC, addr3=destination
 *   from-DS (AP → client): addr1=client MAC, addr2=BSSID, addr3=source
 *   Probe requests: addr2=client MAC, BSSID=broadcast — client looking for networks
 *
 * Why it's useful:
 *   - Reveals exactly who is connected to nearby networks
 *   - Useful for targeted deauth (specific client, not broadcast)
 *   - Probe requests reveal what networks a device has previously used
 *   - Cross-reference with ARP scan for full device inventory
 *
 * Note: Hops all 13 channels briefly to catch stations on any channel.
 * Only 100ms per channel so the web UI stays responsive.
 */
#include "station_scanner.h"
#include <string.h>

StationScanner* StationScanner::_instance = nullptr;

void StationScanner::begin() {
  _running=false; _count=0; _instance=this;
}

void StationScanner::start() {
  _running=true; _count=0; _lastScan=0;

  wifi_promiscuous_filter_t f;
  f.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL;
  esp_wifi_set_promiscuous_filter(&f);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(snifferCb);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  Serial.println("StationScanner: started - sniffing for client devices");
}

void StationScanner::stop() {
  _running=false;
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  delay(30);
  esp_wifi_set_promiscuous(false);
  Serial.printf("StationScanner: stopped - %d stations found\n", _count);
}

void StationScanner::snifferCb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if(!_instance || !_instance->_running) return;
  if(type != WIFI_PKT_DATA && type != WIFI_PKT_MGMT) return;
  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  _instance->handleFrame(pkt->payload, pkt->rx_ctrl.sig_len,
                         pkt->rx_ctrl.channel, pkt->rx_ctrl.rssi);
}

String StationScanner::macToStr(const uint8_t* m) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
  return String(buf);
}

static bool isBroadcast(const uint8_t* mac) {
  return mac[0]==0xFF && mac[1]==0xFF && mac[2]==0xFF &&
         mac[3]==0xFF && mac[4]==0xFF && mac[5]==0xFF;
}

static bool isMulticast(const uint8_t* mac) {
  return (mac[0] & 0x01) != 0;
}

void StationScanner::handleFrame(const uint8_t* data, int len, int channel, int rssi) {
  if(len < 24) return;

  uint8_t fc0 = data[0];
  uint8_t fc1 = data[1];
  uint8_t type    = (fc0 >> 2) & 0x03;  // 0=mgmt, 1=ctrl, 2=data
  uint8_t subtype = (fc0 >> 4) & 0x0F;

  const uint8_t* addr1 = data + 4;   // Destination
  const uint8_t* addr2 = data + 10;  // Source / Transmitter
  const uint8_t* addr3 = data + 16;  // BSSID / Receiver

  String clientMAC, apBSSID;
  bool isProbe = false;

  if(type == 0 && subtype == 4) {
    // Probe request — client broadcasting "I'm looking for network X"
    // addr2 = client MAC
    if(isBroadcast(addr2) || isMulticast(addr2)) return;
    clientMAC = macToStr(addr2);
    apBSSID   = ""; // not yet associated
    isProbe   = true;
  } else if(type == 2) {
    // Data frame
    bool toDS   = (fc1 & 0x01) != 0;
    bool fromDS = (fc1 & 0x02) != 0;

    if(toDS && !fromDS) {
      // Client → AP: addr1=BSSID, addr2=client
      if(isBroadcast(addr2) || isMulticast(addr2)) return;
      clientMAC = macToStr(addr2);
      apBSSID   = macToStr(addr1);
    } else if(!toDS && fromDS) {
      // AP → Client: addr1=client, addr2=BSSID
      if(isBroadcast(addr1) || isMulticast(addr1)) return;
      clientMAC = macToStr(addr1);
      apBSSID   = macToStr(addr2);
    } else {
      return; // WDS / ad-hoc — skip
    }
  } else {
    return;
  }

  // Ignore our own AP's MAC
  uint8_t ourMAC[6];
  esp_wifi_get_mac(WIFI_IF_AP, ourMAC);
  if(clientMAC == macToStr(ourMAC)) return;

  // Update existing entry or add new one
  for(int i=0; i<_count; i++) {
    if(_stations[i].mac == clientMAC) {
      _stations[i].lastSeen = millis();
      if(rssi > _stations[i].rssi) _stations[i].rssi = rssi;
      if(apBSSID.length() && _stations[i].apBssid.length()==0)
        _stations[i].apBssid = apBSSID;
      return;
    }
  }

  // New station
  if(_count < MAX_STATIONS) {
    Station& s    = _stations[_count++];
    s.mac         = clientMAC;
    s.apBssid     = apBSSID;
    s.apSSID      = ""; // filled later from scan correlation
    s.rssi        = rssi;
    s.channel     = channel;
    s.lastSeen    = millis();
    s.isProbing   = isProbe;
    Serial.printf("Station: %s  AP: %s  ch%d  %ddBm%s\n",
      clientMAC.c_str(), apBSSID.c_str(), channel, rssi,
      isProbe ? " [probing]" : "");
  }
}

static uint8_t _chIdx = 0;
void StationScanner::tick() {
  if(!_running) return;
  uint32_t now = millis();
  // Hop channels every 100ms — short enough to catch stations on all channels
  // while keeping the web UI responsive
  if(now - _lastScan < 100) return;
  _lastScan = now;
  _chIdx = (_chIdx + 1) % 13;
  esp_wifi_set_channel(_chIdx + 1, WIFI_SECOND_CHAN_NONE);
}
