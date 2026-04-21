#pragma once
#include <NimBLEDevice.h>
#include <NimBLEScan.h>
#include <Arduino.h>

// Extra BLE attacks:
// - AirDrop spam (iOS popup)
// - Game controller spam (Switch Joy-Con, PS5, Xbox)
// - Tracker detector (AirTag, Tile, SmartTag)
// - BLE name spoof

#define MAX_TRACKERS 30

struct TrackerResult {
  String name;
  String address;
  String type;  // "AirTag", "Tile", "SmartTag", "Unknown Tracker"
  int    rssi;
};

class BLEAttacksExtra {
public:
  void begin();

  // AirDrop spam - iOS popup flood (uses existing Apple BLE payload)
  void startAirdropSpam();
  void stopAirdropSpam();
  bool isAirdropRunning() { return _airdropRunning; }
  void tickAirdrop();

  // Game controller spam
  void startControllerSpam(int type); // 0=JoyCon, 1=PS5, 2=Xbox
  void stopControllerSpam();
  bool isControllerRunning() { return _controllerRunning; }
  void tickController();

  // Tracker detector
  void scanTrackers();
  int  getTrackerCount() { return _trackerCount; }
  TrackerResult& getTracker(int i) { return _trackers[i]; }

  // BLE name spoof - advertise as a named device
  void startNameSpoof(const String& name, int deviceType);
  void stopNameSpoof();
  bool isNameSpoofRunning() { return _nameSpoofRunning; }

private:
  bool _airdropRunning    = false;
  bool _controllerRunning = false;
  bool _nameSpoofRunning  = false;
  int  _controllerType    = 0;
  uint32_t _lastAdv       = 0;
  int  _advIdx            = 0;

  TrackerResult _trackers[MAX_TRACKERS];
  int  _trackerCount = 0;

  NimBLEServer* _server = nullptr;

  void _sendAirdropAdv();
  void _sendControllerAdv();
};

extern BLEAttacksExtra bleAttacksExtra;
