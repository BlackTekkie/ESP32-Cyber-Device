/**
 * beacon_spam_random.cpp — Random Beacon Spam (Improved)
 * -------------------------------------------------------
 * Floods the spectrum with 802.11 beacon frames using realistic SSIDs.
 *
 * IMPROVEMENTS over original:
 *
 * 1. REALISTIC SSID GENERATION — uses real UK/Irish ISP naming patterns
 *    with random alphanumeric suffixes (e.g. "BTHub6-4XK2", "SKYAB1C23")
 *    instead of random words + numbers. These look like real routers.
 *
 * 2. ALL 13 CHANNELS — hops channels 1-13, not just 1/6/11, so it
 *    floods the full EU WiFi spectrum including channels 12/13.
 *
 * 3. REALISTIC CAPABILITY BITS — randomly mixes WPA2 and Open networks
 *    so the fake APs look like a mix of secured and unsecured networks,
 *    matching what you'd see in a real urban environment.
 *
 * 4. RANDOMISED BEACON INTERVALS — varies between 100-200 TU to avoid
 *    all beacons looking identical (different "routers" have different
 *    interval settings).
 *
 * Use case: demonstrates passive SSID scanning, spectrum congestion,
 * and how trivially easy it is to create convincing fake networks.
 */
#include "beacon_spam_random.h"
#include <string.h>

// UK/Ireland ISP name prefixes for realistic SSID generation
static const char* ISP_PREFIXES[] = {
  "BTHub6-","BTHub5-","BT-Hub-","BThub-",
  "SKY","SKYQ","Sky_","SkyHub",
  "VM","VirginMedia-","VodafoneMobile-",
  "EE-","EEBrightBox-","EE-BrightBox-",
  "NETGEAR","TP-Link_","TP-LINK_","ASUS_",
  "Eir-","eircom-","Three-","iPhone-",
  "HUAWEI-","Linksys","DIRECT-",
};
static const int N_PREFIXES = 25;

static const char HEX_CHARS[] = "0123456789ABCDEF";
static const char ALNUM_CHARS[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

// Generate one realistic-looking ISP-style SSID
static String makeRealisticSSID() {
  const char* prefix = ISP_PREFIXES[random(N_PREFIXES)];
  String s = String(prefix);
  int remaining = 32 - s.length();
  if(remaining <= 0) return s;

  // Most ISP routers have 4-8 character alphanumeric suffix
  int suffixLen = random(4, min(9, remaining+1));
  for(int i=0; i<suffixLen; i++) {
    // Mix of hex and alphanumeric - different ISPs use different styles
    if(random(2)) s += HEX_CHARS[random(16)];
    else          s += ALNUM_CHARS[random(32)];
  }
  return s;
}

// Channel list: all 13 EU 2.4GHz channels
static const uint8_t ALL_CHANNELS[] = {1,2,3,4,5,6,7,8,9,10,11,12,13};

void BeaconSpamRandom::begin() {
  _running=false; _sent=0; _cacheCount=0;
  memset(_ssids, 0, sizeof(_ssids));
}

void BeaconSpamRandom::generateSSIDs(int count) {
  if(count > MAX_BSR_SSIDS) count = MAX_BSR_SSIDS;
  _cacheCount = count;
  for(int i=0; i<count; i++) {
    String s = makeRealisticSSID();
    strncpy(_ssids[i], s.c_str(), 32);
    _ssids[i][32] = '\0';
  }
}

bool BeaconSpamRandom::start(int count) {
  generateSSIDs(count > 0 ? count : 20);
  _chIdx=0; _channel=1;
  _sent=0; _lastTx=0; _running=true;

  Serial.printf("BeaconRandom: started with %d realistic SSIDs\n", _cacheCount);
  for(int i=0;i<_cacheCount;i++)
    Serial.printf("  [%d] %s\n", i, _ssids[i]);
  return true;
}

void BeaconSpamRandom::stop() {
  _running=false;
  Serial.printf("BeaconRandom: stopped — %d beacons sent\n", _sent);
}

void BeaconSpamRandom::tick() {
  if(!_running) return;
  uint32_t now=millis();
  if(now-_lastTx < 80) return; // slightly faster than original
  _lastTx=now;

  // Hop through all 13 channels (not just 1/6/11)
  uint8_t ch = ALL_CHANNELS[_chIdx % 13];
  _chIdx++;
  if(ch != _channel) {
    _channel = ch;
    esp_wifi_set_channel(_channel, WIFI_SECOND_CHAN_NONE);
  }

  for(int s=0; s<_cacheCount; s++) {
    // Build a fresh beacon frame for each SSID
    uint8_t pkt[128];
    int pos = 0;

    // Frame Control + Duration
    pkt[pos++]=0x80; pkt[pos++]=0x00;
    pkt[pos++]=0x00; pkt[pos++]=0x00;

    // Destination: broadcast
    for(int i=0;i<6;i++) pkt[pos++]=0xFF;

    // Source MAC: base MAC with SSID index in last byte
    pkt[pos++]=0x02; pkt[pos++]=0x11; pkt[pos++]=0x22;
    pkt[pos++]=0x33; pkt[pos++]=0x44; pkt[pos++]=(uint8_t)s;
    // BSSID = same as source
    pkt[pos-6+6]=(uint8_t)s;
    memcpy(&pkt[10], &pkt[10], 6);
    memcpy(&pkt[16], &pkt[10], 6);

    // Sequence control (random for realism)
    pkt[pos++]=random(256); pkt[pos++]=random(16)<<4;

    // Timestamp (8 bytes - use millis for some variation)
    uint32_t ts=millis();
    memcpy(&pkt[pos], &ts, 4); memset(&pkt[pos+4],0,4); pos+=8;

    // Beacon interval: randomised between 100-150 TU (different "routers")
    uint16_t bi = 100 + random(51);
    pkt[pos++] = bi & 0xFF; pkt[pos++] = (bi>>8) & 0xFF;

    // Capability: randomly WPA2 or open (mix looks more real)
    uint16_t caps = (random(4)==0) ? 0x0421 : 0x0431; // 25% open, 75% WPA2
    pkt[pos++] = caps & 0xFF; pkt[pos++] = (caps>>8) & 0xFF;

    // SSID IE
    uint8_t slen = (uint8_t)strlen(_ssids[s]);
    pkt[pos++]=0x00; pkt[pos++]=slen;
    memcpy(&pkt[pos], _ssids[s], slen); pos+=slen;

    // Supported Rates IE
    pkt[pos++]=0x01; pkt[pos++]=0x08;
    pkt[pos++]=0x82; pkt[pos++]=0x84; pkt[pos++]=0x8b; pkt[pos++]=0x96;
    pkt[pos++]=0x24; pkt[pos++]=0x30; pkt[pos++]=0x48; pkt[pos++]=0x6c;

    // DS Parameter Set (current channel)
    pkt[pos++]=0x03; pkt[pos++]=0x01; pkt[pos++]=_channel;

    for(int r=0;r<2;r++){
      if(esp_wifi_80211_tx(WIFI_IF_STA, pkt, pos, false)==ESP_OK) _sent++;
      delay(1);
    }
    _lastSSID = String(_ssids[s]);
  }
}
