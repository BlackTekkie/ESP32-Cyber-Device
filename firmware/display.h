#pragma once
#include "ui_states.h"
#include <Arduino.h>
#include <TFT_eSPI.h>

// Colours (RGB565)
#define COL_BG      0x0820  // dark navy background
#define COL_PANEL   0x1084  // slightly lighter panel
#define COL_ACCENT  0x07E0  // green
#define COL_YELLOW  0xFFE0  // yellow
#define COL_RED     0xF800  // red
#define COL_WHITE   0xFFFF
#define COL_DIM     0x8410  // grey
#define COL_BORDER  0x2945  // border grey

class Display {
public:
  void begin();
  void drawBoot();
  void drawMenu(int selected, int battery);
  void drawScreen(UiState state);
  void drawLockScreen(String enteredPin, bool shake);

  // WiFi
  void drawWiFiList(String ssid[], int rssi[], int enc[], int count,
                    int selected, int scroll);
  void drawWiFiStats(int total, int strong);
  void drawWiFiDetail(String ssid, int rssi, int enc, int channel);

  // BLE
  void drawBLEList(String name[], String addr[], int rssi[], int count,
                   int selected, int scroll);
  void drawBLEStats(int total);
  void drawBLEDetail(String name, String addr, int rssi);

  // Tools
  void drawNetworkList(String ip[], String mac[], int count);
  void drawAPManager(String ip[], String mac[], int count);
  void drawPortScan(String targetIP, int ports[], String svcs[],
                    int count, bool scanning);
  void drawChannelMap(int nets[], int chBest, int chBusy);
  void drawSettingsScreen();
  void drawAttackStatus(const char* name, bool running);

  // Touch — returns true if screen is being touched
  bool getTouch(uint16_t &x, uint16_t &y);

private:
  TFT_eSPI _tft;
  void _header(const char* title, uint16_t colour = COL_ACCENT);
  void _footer(const char* msg);
  String _encStr(int enc);
  String _rssiBar(int rssi);
};
