#pragma once
#include <WiFi.h>

struct ChannelInfo {
  int channel;
  int networkCount;
  int strongCount;  // RSSI > -70
  int avgRssi;
};

class ChannelScanner {
public:
  // Analyse channel congestion from existing WiFi scan results
  void analyse(String ssid[], int rssi[], int chan[], int count);

  ChannelInfo getChannel(int ch) { return (ch>=1&&ch<=14)?_ch[ch]:ChannelInfo{}; }
  int         getBusiestChannel();
  int         getFreestChannel();

private:
  ChannelInfo _ch[15];  // channels 1-14
};
