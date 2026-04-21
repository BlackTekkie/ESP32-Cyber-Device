#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include <Arduino.h>

#define MAX_EAPOL_CAPTURES  8
#define MAX_EAPOL_FRAME_LEN 300  // max raw frame bytes to store per capture

// One captured EAPOL handshake message
struct EapolCapture {
  String   clientMAC;
  String   apMAC;
  String   apSSID;
  int      channel;
  uint8_t  type;           // 1=M1, 2=M2, 3=M3, 4=M4
  uint32_t timestamp;
  // Raw frame storage for hashcat output
  uint8_t  rawFrame[MAX_EAPOL_FRAME_LEN];
  int      rawLen;
};

class EapolSniffer {
public:
  void begin();
  bool start();
  void stop();
  void tick();

  bool         isRunning()         { return _running; }
  int          getCaptureCount()   { return _count; }
  EapolCapture getCapture(int i)   { return (i>=0&&i<_count)?_caps[i]:EapolCapture{}; }
  int          getCurrentChannel() { return _channel; }
  void         setTargetChannel(int ch) { _targetChannel=(uint8_t)ch; }
  int          getTargetChannel()       { return _targetChannel; }

  // Targeted deauth+capture
  void     startTargeted(const String& bssid, uint8_t channel);
  bool     isTargetedRunning() { return _targetedRunning; }
  uint32_t getTargetedSecondsLeft();

  // Hashcat 22000 format output
  // Requires M1+M2 pair from the same handshake
  String   getHashcatLine(int m1idx, int m2idx);
  String   getHashcatFormat(); // auto-pairs M1+M2, returns full file content

private:
  EapolCapture _caps[MAX_EAPOL_CAPTURES];
  int      _count          = 0;
  bool     _running        = false;
  uint8_t  _channel        = 1;
  uint8_t  _homeChannel    = 1;
  uint8_t  _targetChannel  = 0;

  // Channel hopping state
  uint8_t  _foreignChannels[12];
  int      _hopCount       = 0;
  int      _hopIdx         = 0;
  bool     _onHome         = true;
  uint32_t _lastHop        = 0;
  bool     _channelLocked  = false;
  uint8_t  _lockedChannel  = 0;
  uint32_t _lockExpiry     = 0;

  // Targeted capture state
  bool     _targetedRunning = false;
  uint32_t _targetedExpiry  = 0;
  String   _targetedBSSID;

  static void snifferCb(void* buf, wifi_promiscuous_pkt_type_t type);
  static EapolSniffer* _instance;
  void    handlePacket(const uint8_t* data, int len, int channel);
  bool    isEapol(const uint8_t* data, int len);
  String  macToStr(const uint8_t* m);
};

extern EapolSniffer eapolSniffer;
