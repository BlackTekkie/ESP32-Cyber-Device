/**
 * beacon_spam_list.cpp — Custom SSID Beacon Spam
 * -------------------------------------------------
 * Broadcasts 802.11 beacon frames for a user-defined list of SSIDs.
 * Identical approach to beacon_spam_random but with specific SSIDs
 * supplied via the web UI (one per line, comma-separated internally).
 *
 * Packet format matches the ESP32Marauder beacon implementation.
 * Frames are injected via esp_wifi_80211_tx() on WIFI_IF_STA.
 * Each SSID gets a unique MAC (last byte = SSID index).
 *
 * Author: ESP32 Cyber Device Project
 */
#include "beacon_spam_list.h"
#include <string.h>
#include "esp_wifi.h"

// Exact Marauder packet format (from ESP32Marauder wiki)
static const uint8_t MARAUDER_BASE[] = {
  0x80, 0x00, 0x00, 0x00,               // Frame Control, Duration
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // Destination: broadcast
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // Source MAC (overwritten)
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // BSSID (overwritten)
  0x00, 0x00,                           // Sequence control
  0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00, // Timestamp
  0x64, 0x00,                           // Beacon interval: 100 TU
  0x31, 0x04,                           // Capability: ESS + privacy
  0x00                                  // SSID tag (length overwritten)
};

static uint8_t bsl_mac[6];

void BeaconSpamList::begin()      { _count=0; _running=false; _sent=0; }
void BeaconSpamList::clearSSIDs() { _count=0; }

void BeaconSpamList::addSSID(const char* ssid) {
  if(_count >= MAX_SPAM_SSIDS) return;
  strncpy(_ssids[_count], ssid, 32);
  _ssids[_count][32]='\0';
  _count++;
}

bool BeaconSpamList::start() {
  if(_count==0) return false;
  for(int i=0;i<6;i++) bsl_mac[i]=random(256);
  bsl_mac[0]=(bsl_mac[0]&0xFE)|0x02;
  _sent=0; _idx=0; _chIdx=0; _lastTx=0; _running=true;
  Serial.printf("BeaconSpamList: %d SSIDs\n", _count);
  return true;
}

void BeaconSpamList::stop() { _running=false; }

void BeaconSpamList::tick() {
  if(!_running) return; // not started yet - do nothing

  uint32_t now = millis();
  if(now - _lastTx < 100) return; // wait 100ms between transmissions
  _lastTx = now;

  // Rotate through channels 1, 6, 11 - these are the three non-overlapping
  // 2.4GHz WiFi channels. Rotating means we appear on all of them.
  static const uint8_t CH[] = {1, 6, 11};
  esp_wifi_set_channel(CH[_chIdx % 3], WIFI_SECOND_CHAN_NONE);
  _chIdx++;

  // Send a beacon frame for each SSID in our list
  for(int s=0; s<_count; s++){
    uint8_t packet[128]; // 128 bytes is plenty for a beacon frame

    // Start with the base beacon frame template (header + fixed fields)
    memcpy(packet, MARAUDER_BASE, sizeof(MARAUDER_BASE));

    // Give each SSID a unique MAC address - last byte = SSID index
    // This makes each fake network appear to come from a different router
    bsl_mac[5] = s;
    memcpy(&packet[10], bsl_mac, 6); // source MAC (bytes 10-15)
    memcpy(&packet[16], bsl_mac, 6); // BSSID (bytes 16-21, same as source)

    // Build the SSID Information Element (IE):
    // 802.11 IEs have format: [Tag Number][Length][Data...]
    uint8_t sLen = strlen(_ssids[s]);
    packet[36] = 0x00;  // IE tag 0 = SSID
    packet[37] = sLen;  // length of the SSID
    memcpy(&packet[38], _ssids[s], sLen); // copy the SSID string

    // Build the Supported Rates IE (tag 1)
    // These are the data rates this "AP" claims to support (in 500kbps units)
    int pos = 38 + sLen; // start right after the SSID
    packet[pos++]=0x01; packet[pos++]=0x08; // tag=1 (supported rates), length=8
    packet[pos++]=0x82; // 1 Mbps  (basic rate, 0x82 = 0x02 | 0x80)
    packet[pos++]=0x84; // 2 Mbps
    packet[pos++]=0x8b; // 5.5 Mbps
    packet[pos++]=0x96; // 11 Mbps
    packet[pos++]=0x24; // 18 Mbps
    packet[pos++]=0x30; // 24 Mbps
    packet[pos++]=0x48; // 36 Mbps
    packet[pos++]=0x6c; // 54 Mbps

    // DS Parameter Set IE (tag 3) - tells devices which channel we're on
    packet[pos++]=0x03; // tag = 3 (DS parameter set)
    packet[pos++]=0x01; // length = 1 byte
    packet[pos++]=CH[_chIdx%3]; // the current channel number

    // Inject the packet directly into the WiFi radio (bypasses normal AP stack)
    // Send each beacon 3 times to improve reception reliability
    for(int r=0; r<3; r++){
      if(esp_wifi_80211_tx(WIFI_IF_STA, packet, pos, false)==ESP_OK) _sent++;
      delay(1); // tiny delay between retransmissions
    }
  }
}
