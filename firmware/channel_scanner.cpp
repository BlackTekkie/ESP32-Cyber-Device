#include "channel_scanner.h"

void ChannelScanner::analyse(String ssid[], int rssi[], int chan[], int count) {
  // Reset
  for (int c = 0; c <= 14; c++) {
    _ch[c] = {c, 0, 0, 0};
  }

  int rssiSum[15] = {0};

  for (int i = 0; i < count; i++) {
    int c = chan[i];
    if (c < 1 || c > 14) continue;
    _ch[c].networkCount++;
    if (rssi[i] > -70) _ch[c].strongCount++;
    rssiSum[c] += rssi[i];
  }

  for (int c = 1; c <= 14; c++) {
    if (_ch[c].networkCount > 0)
      _ch[c].avgRssi = rssiSum[c] / _ch[c].networkCount;
  }
}

int ChannelScanner::getBusiestChannel() {
  int best = 1, bestCount = 0;
  for (int c = 1; c <= 13; c++) {
    if (_ch[c].networkCount > bestCount) {
      bestCount = _ch[c].networkCount;
      best = c;
    }
  }
  return best;
}

int ChannelScanner::getFreestChannel() {
  // Only consider non-overlapping channels: 1, 6, 11
  int best = 1, bestCount = 999;
  for (int c : {1, 6, 11}) {
    if (_ch[c].networkCount < bestCount) {
      bestCount = _ch[c].networkCount;
      best = c;
    }
  }
  return best;
}
