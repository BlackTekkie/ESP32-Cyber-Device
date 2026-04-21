#pragma once
#include <NimBLEDevice.h>
#include <NimBLEScan.h>
#include <NimBLEAdvertisedDevice.h>

#define MAX_BLE_DEVICES 20

struct BLEDevice_t {
  String name;
  String address;
  int    rssi;
};

class BLEViewer {
public:
  void        begin();
  int         scan();
  int         getCount();
  String      getName(int i);
  String      getAddress(int i);
  int         getRSSI(int i);
  BLEDevice_t getDevice(int i);

private:
  int         _count = 0;
  BLEDevice_t _devices[MAX_BLE_DEVICES];
};
