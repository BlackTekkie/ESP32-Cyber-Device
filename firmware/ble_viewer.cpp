#include "ble_viewer.h"

void BLEViewer::begin() {
  NimBLEDevice::init("");
  _count = 0;
}

int BLEViewer::scan() {
  _count = 0;
  NimBLEScan* scanner = NimBLEDevice::getScan();
  scanner->setActiveScan(true);
  scanner->setInterval(100);
  scanner->setWindow(99);

  // NimBLE 2.x: start() returns bool, results via getResults()
  scanner->start(4, false);
  NimBLEScanResults results = scanner->getResults();

  int found = results.getCount();
  _count = min(found, MAX_BLE_DEVICES);

  for(int i = 0; i < _count; i++){
    const NimBLEAdvertisedDevice* dev = results.getDevice(i);
    _devices[i].name    = dev->haveName() ? String(dev->getName().c_str()) : String("Unknown");
    _devices[i].address = String(dev->getAddress().toString().c_str());
    _devices[i].rssi    = dev->getRSSI();
  }
  scanner->clearResults();
  return _count;
}

int         BLEViewer::getCount()        { return _count; }
String      BLEViewer::getName(int i)    { return (i>=0&&i<_count)?_devices[i].name:String(""); }
String      BLEViewer::getAddress(int i) { return (i>=0&&i<_count)?_devices[i].address:String(""); }
int         BLEViewer::getRSSI(int i)    { return (i>=0&&i<_count)?_devices[i].rssi:0; }
BLEDevice_t BLEViewer::getDevice(int i)  { return (i>=0&&i<_count)?_devices[i]:BLEDevice_t{}; }
