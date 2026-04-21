#include "ap_manager.h"
#include <lwip/netif.h>
#include <lwip/etharp.h>

// Resolve IP from ARP table by MAC
String APManager::arpLookup(uint8_t* targetMac) {
  ip4_addr_t*      entryIP    = nullptr;
  struct netif*    entryNetif = nullptr;
  struct eth_addr* entryMAC   = nullptr;

  for (int i = 0; i < ARP_TABLE_SIZE; i++) {
    if (etharp_get_entry(i, &entryIP, &entryNetif, &entryMAC) && entryMAC) {
      if (entryMAC->addr[0] == targetMac[0] &&
          entryMAC->addr[1] == targetMac[1] &&
          entryMAC->addr[2] == targetMac[2] &&
          entryMAC->addr[3] == targetMac[3] &&
          entryMAC->addr[4] == targetMac[4] &&
          entryMAC->addr[5] == targetMac[5]) {
        return IPAddress(entryIP->addr).toString();
      }
    }
  }
  return "Unknown";
}

int APManager::refresh() {
  _count = 0;

  wifi_sta_list_t list;
  memset(&list, 0, sizeof(list));
  if (esp_wifi_ap_get_sta_list(&list) != ESP_OK) return 0;

  int total = list.num;
  if (total > MAX_CLIENTS) total = MAX_CLIENTS;

  for (int i = 0; i < total; i++) {
    memcpy(_clients[i].mac, list.sta[i].mac, 6);
    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             _clients[i].mac[0], _clients[i].mac[1], _clients[i].mac[2],
             _clients[i].mac[3], _clients[i].mac[4], _clients[i].mac[5]);
    _clients[i].macStr = String(mac);
    _clients[i].ip     = arpLookup(_clients[i].mac);
  }

  _count = total;
  return _count;
}

bool APManager::kickAll() {
  // Send deauthentication to ALL stations connected to YOUR OWN AP.
  // AID = 0 means broadcast deauth — only works on the ESP32's own softAP.
  return (esp_wifi_deauth_sta(0) == ESP_OK);
}

String APManager::getMAC(int i) {
  return (i < 0 || i >= _count) ? "" : _clients[i].macStr;
}

String APManager::getIP(int i) {
  return (i < 0 || i >= _count) ? "" : _clients[i].ip;
}
