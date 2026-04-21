/**
 * network_scanner.cpp — Local Network Host Discovery
 * ----------------------------------------------------
 * Performs ARP-based host discovery on the ESP32's soft AP subnet
 * (192.168.4.0/24). Identifies connected clients by sending ARP
 * requests and collecting responses.
 *
 * Also queries the AP client list via esp_wifi_ap_get_sta_list() to
 * get MAC addresses of associated stations. Correlates IP and MAC
 * addresses for display in the web UI network map.
 *
 * Useful for: identifying connected devices during Evil Portal/Karma
 * attacks, port scanning targets, and network reconnaissance.
 *
 * Author: ESP32 Cyber Device Project
 */
#include "network_scanner.h"
#include <esp_wifi.h>
#include <lwip/netif.h>
#include <lwip/etharp.h>

static String macToIP(uint8_t* targetMac) {
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

int NetworkScanner::scan() {
  _count = 0;

  wifi_sta_list_t stationList;
  memset(&stationList, 0, sizeof(stationList));

  if (esp_wifi_ap_get_sta_list(&stationList) != ESP_OK) return 0;

  int total = stationList.num;
  if (total > MAX_HOSTS) total = MAX_HOSTS;

  for (int i = 0; i < total; i++) {
    uint8_t* m = stationList.sta[i].mac;

    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             m[0], m[1], m[2], m[3], m[4], m[5]);

    _hosts[i].mac       = String(mac);
    _hosts[i].ip        = macToIP(m);
    _hosts[i].reachable = true;
  }

  _count = total;
  return _count;
}

String NetworkScanner::getIP(int i)  { return (i<0||i>=_count)?"":_hosts[i].ip;  }
String NetworkScanner::getMAC(int i) { return (i<0||i>=_count)?"":_hosts[i].mac; }
