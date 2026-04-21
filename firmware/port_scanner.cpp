#include "port_scanner.h"

// Well-known port names
String PortScanner::serviceName(int port) {
  switch (port) {
    case 21:   return "FTP";
    case 22:   return "SSH";
    case 23:   return "Telnet";
    case 25:   return "SMTP";
    case 53:   return "DNS";
    case 80:   return "HTTP";
    case 110:  return "POP3";
    case 135:  return "RPC";
    case 139:  return "NetBIOS";
    case 143:  return "IMAP";
    case 443:  return "HTTPS";
    case 445:  return "SMB";
    case 554:  return "RTSP";
    case 1883: return "MQTT";
    case 3000: return "Node";
    case 3306: return "MySQL";
    case 3389: return "RDP";
    case 5000: return "Flask";
    case 8080: return "HTTP-Alt";
    case 8443: return "HTTPS-Alt";
    case 9100: return "Printer";
    default:   return "Unknown";
  }
}

// Ports to probe in order
static const int PROBE_PORTS[] = {
  80, 443, 22, 21, 23, 25, 53, 110, 135, 139,
  143, 445, 554, 1883, 3000, 3306, 3389, 5000, 8080, 8443, 9100
};
static const int PROBE_COUNT = sizeof(PROBE_PORTS) / sizeof(int);

int PortScanner::scan(String targetIP, int timeout_ms) {
  _openCount = 0;
  _target    = targetIP;

  for (int i = 0; i < PROBE_COUNT && _openCount < MAX_OPEN_PORTS; i++) {
    int port = PROBE_PORTS[i];

    WiFiClient client;
    client.setTimeout(timeout_ms);
    bool connected = client.connect(targetIP.c_str(), port);
    client.stop();

    if (connected) {
      _open[_openCount].port    = port;
      _open[_openCount].open    = true;
      _open[_openCount].service = serviceName(port);
      _openCount++;
    }
  }

  return _openCount;
}
