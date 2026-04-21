#include "wifi_viewer.h"

void WiFiViewer::begin() {
  // Don't change WiFi mode here — AP mode is managed by WebUI
  // Just make sure any previous scan is cleared
  WiFi.scanDelete();
}

int WiFiViewer::scan() {
  // Stay in AP_STA mode — AP stays up while we scan
  WiFi.mode(WIFI_AP_STA);
  networkCount = WiFi.scanNetworks(false, true); // sync, show hidden
  if (networkCount < 0)            networkCount = 0;
  if (networkCount > MAX_NETWORKS) networkCount = MAX_NETWORKS;
  // Restore AP after scan in case mode changed
  WiFi.softAP("ESP32-Cyber", "esp32cyber");
  delay(100);
  return networkCount;
}

String WiFiViewer::getSSID(int i)       { return WiFi.SSID(i); }
int    WiFiViewer::getRSSI(int i)       { return WiFi.RSSI(i); }
int    WiFiViewer::getEncryption(int i) { return (int)WiFi.encryptionType(i); }
int    WiFiViewer::getChannel(int i)    { return WiFi.channel(i); }

bool WiFiViewer::hasWPS(int i) {
  // WPS (Wi-Fi Protected Setup) is a major vulnerability — allows PIN-based
  // attacks (Pixie Dust) that can recover WPA2 passwords.
  // We flag WPA2 networks as potentially having WPS enabled.
  // A full implementation would parse the beacon's vendor-specific IEs.
  int enc = WiFi.encryptionType(i);
  return (enc == WIFI_AUTH_WPA2_PSK || enc == WIFI_AUTH_WPA_WPA2_PSK);
}

String WiFiViewer::getManufacturer(int i) {
  // OUI = first 3 bytes of BSSID, identifies network adapter manufacturer
  uint8_t* b = WiFi.BSSID(i);
  if(!b) return "?";
  // Common UK/Ireland router OUIs
  if(b[0]==0xC8&&b[1]==0x96) return "BT";
  if(b[0]==0xC4&&b[1]==0x0F) return "Sky";
  if(b[0]==0xA0&&b[1]==0x99) return "Sky";
  if(b[0]==0x80&&b[1]==0xF3) return "Sky";
  if(b[0]==0xBC&&b[1]==0x30) return "Virgin";
  if(b[0]==0x78&&b[1]==0x62) return "Virgin";
  if(b[0]==0xCC&&b[1]==0x40) return "Apple";
  if(b[0]==0x00&&b[1]==0x17&&b[2]==0xF2) return "Apple";
  if(b[0]==0xAC&&b[1]==0x22) return "Samsung";
  if(b[0]==0x50&&b[1]==0x6B) return "Huawei";
  if(b[0]==0x14&&b[1]==0xEB) return "TP-Link";
  if(b[0]==0xEC&&b[1]==0x08) return "TP-Link";
  if(b[0]==0x84&&b[1]==0x16) return "Netgear";
  if(b[0]==0x30&&b[1]==0x23) return "Asus";
  if((b[0]&0x02)!=0)         return "RandomMAC";
  return "?";
}