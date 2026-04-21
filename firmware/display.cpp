/**
 * display.cpp — ILI9341 TFT Display Driver
 * ESP32 Cyber Device — 2.8" 240x320 with XPT2046 touch
 *
 * Pins (from PCB brief):
 *   TFT_CS=17, TFT_DC=16, TFT_RST=-1
 *   MOSI=23, SCK=18, MISO=19, BL=32
 *   Touch CS=21
 */
#include "display.h"

#define TFT_BL 32

// ── Init ─────────────────────────────────────────────────────────────────────

void Display::begin() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  _tft.init();
  _tft.setRotation(0);       // portrait
  _tft.fillScreen(COL_BG);
  _tft.setTextDatum(TL_DATUM);
  Serial.println("Display: ILI9341 initialised");
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void Display::_header(const char* title, uint16_t colour) {
  _tft.fillRect(0,0,240,22, COL_PANEL);
  _tft.setTextColor(colour, COL_PANEL);
  _tft.setTextSize(1);
  _tft.drawString(title, 6, 5, 2);
}

void Display::_footer(const char* msg) {
  _tft.fillRect(0,308,240,12, COL_PANEL);
  _tft.setTextColor(COL_DIM, COL_PANEL);
  _tft.drawString(msg, 4, 309, 1);
}

String Display::_encStr(int enc) {
  switch(enc) {
    case 0: return "Open";
    case 1: return "WEP";
    case 3: return "WPA2";
    case 4: return "WPA2";
    case 5: return "WPA3";
    default: return "WPA";
  }
}

String Display::_rssiBar(int rssi) {
  if(rssi > -60) return "||||";
  if(rssi > -70) return "|||.";
  if(rssi > -80) return "||..";
  return "|...";
}

// ── Boot screen ───────────────────────────────────────────────────────────────

void Display::drawBoot() {
  _tft.fillScreen(COL_BG);
  _tft.setTextColor(COL_ACCENT, COL_BG);
  _tft.drawString("ESP32", 70, 100, 6);
  _tft.setTextColor(COL_WHITE, COL_BG);
  _tft.drawString("CYBER DEVICE", 30, 170, 2);
  _tft.setTextColor(COL_DIM, COL_BG);
  _tft.drawString("v2.0  192.168.4.1", 28, 200, 1);
  Serial.println("Boot screen drawn");
}

// ── Lock screen ───────────────────────────────────────────────────────────────

void Display::drawLockScreen(String pin, bool shake) {
  _tft.fillScreen(COL_BG);
  _header("UNLOCK DEVICE", shake ? COL_RED : COL_ACCENT);

  // PIN dots
  _tft.setTextColor(COL_WHITE, COL_BG);
  _tft.drawString("PIN:", 80, 50, 2);
  for(int i=0;i<4;i++) {
    uint16_t col = (i < (int)pin.length()) ? COL_ACCENT : COL_BORDER;
    _tft.fillCircle(80 + i*22, 78, 8, col);
  }

  if(shake) {
    _tft.setTextColor(COL_RED, COL_BG);
    _tft.drawString("Wrong PIN", 68, 95, 1);
  }

  // Keypad  1-9, back, 0, OK
  const char* keys[] = {"1","2","3","4","5","6","7","8","9","<","0","OK"};
  const int bW=62, bH=42, gX=4, gY=6;
  int pX=(240-3*bW-2*gX)/2, pY=82;

  for(int r=0;r<4;r++) for(int c=0;c<3;c++) {
    int idx=r*3+c;
    int x=pX+c*(bW+gX), y=pY+r*(bH+gY);
    uint16_t bc = (idx==11) ? COL_ACCENT : COL_PANEL;
    uint16_t tc = (idx==11) ? COL_BG : COL_WHITE;
    _tft.fillRoundRect(x,y,bW,bH,6,bc);
    _tft.drawRoundRect(x,y,bW,bH,6,COL_BORDER);
    _tft.setTextColor(tc,bc);
    int tx=x+bW/2-6, ty=y+bH/2-8;
    _tft.drawString(keys[idx], tx, ty, 2);
  }
}

// ── Main menu ─────────────────────────────────────────────────────────────────

void Display::drawMenu(int selected, int battery) {
  _tft.fillScreen(COL_BG);
  _header("ESP32-CYBER", COL_ACCENT);

  // Battery
  String batStr = String(battery) + "%";
  _tft.setTextColor(battery<20?COL_RED:COL_DIM, COL_PANEL);
  _tft.drawString(batStr.c_str(), 195, 5, 1);

  const char* items[] = {
    "WiFi Scanner","BLE Scanner","Network Scan",
    "AP Manager","Port Scanner","Channel Map",
    "Settings","Reboot"
  };
  const char* icons[] = {"W","B","N","A","P","C","S","R"};

  for(int i=0;i<8;i++) {
    int y=28+i*33;
    uint16_t bg = (i==selected) ? COL_ACCENT : COL_PANEL;
    uint16_t fg = (i==selected) ? COL_BG : COL_WHITE;
    _tft.fillRect(0,y,240,30,bg);
    _tft.setTextColor(fg,bg);
    _tft.drawString(icons[i],6,y+7,2);
    _tft.drawString(items[i],28,y+7,2);
  }
  _footer("192.168.4.1  PIN:1234");
}

// ── Generic screen fill ───────────────────────────────────────────────────────

void Display::drawScreen(UiState state) {
  _tft.fillScreen(COL_BG);
  switch(state) {
    case UI_WIFI:      _header("WiFi Scanner",   COL_ACCENT); break;
    case UI_BLUETOOTH: _header("BLE Scanner",    COL_ACCENT); break;
    case UI_NETWORK:   _header("Network Scan",   COL_ACCENT); break;
    case UI_REBOOT:
      _tft.setTextColor(COL_RED,COL_BG);
      _tft.drawString("Rebooting...", 60,150,4);
      break;
    default: break;
  }
  _tft.setTextColor(COL_DIM,COL_BG);
  _tft.drawString("Scanning...", 80,160,2);
}

// ── WiFi list ─────────────────────────────────────────────────────────────────

void Display::drawWiFiList(String ssid[], int rssi[], int enc[],
                            int count, int sel, int scroll) {
  _tft.fillRect(0,22,240,286,COL_BG);
  if(count==0){
    _tft.setTextColor(COL_DIM,COL_BG);
    _tft.drawString("No networks found",30,150,2);
    return;
  }
  int maxVisible=11;
  for(int i=scroll;i<count&&i<scroll+maxVisible;i++){
    int y=24+(i-scroll)*24;
    bool isSel=(i==sel);
    _tft.fillRect(0,y,240,23, isSel?COL_PANEL:COL_BG);
    if(isSel) _tft.drawRect(0,y,240,23,COL_ACCENT);
    _tft.setTextColor(isSel?COL_ACCENT:COL_WHITE, isSel?COL_PANEL:COL_BG);
    String s=ssid[i]; if(s.length()>18) s=s.substring(0,17)+"~";
    _tft.drawString(s.c_str(),4,y+4,1);
    _tft.setTextColor(COL_DIM, isSel?COL_PANEL:COL_BG);
    _tft.drawString(_rssiBar(rssi[i]).c_str(),185,y+4,1);
    _tft.drawString(_encStr(enc[i]).c_str(),210,y+4,1);
  }
  _footer(("Networks: "+String(count)).c_str());
}

void Display::drawWiFiStats(int total, int strong) {
  // already shown in footer
}

void Display::drawWiFiDetail(String ssid, int rssi, int enc, int ch) {
  _tft.fillRect(20,40,200,180,COL_PANEL);
  _tft.drawRect(20,40,200,180,COL_ACCENT);
  _tft.setTextColor(COL_ACCENT,COL_PANEL);
  _tft.drawString("Network Detail",35,50,2);
  _tft.setTextColor(COL_WHITE,COL_PANEL);
  _tft.drawString(("SSID: "+ssid).c_str(),30,80,1);
  _tft.drawString(("RSSI: "+String(rssi)+" dBm").c_str(),30,100,1);
  _tft.drawString(("Enc : "+_encStr(enc)).c_str(),30,120,1);
  _tft.drawString(("Ch  : "+String(ch)).c_str(),30,140,1);
  _tft.setTextColor(COL_DIM,COL_PANEL);
  _tft.drawString("Tap to dismiss",55,195,1);
}

// ── BLE list ─────────────────────────────────────────────────────────────────

void Display::drawBLEList(String name[], String addr[], int rssi[],
                           int count, int sel, int scroll) {
  _tft.fillRect(0,22,240,286,COL_BG);
  if(count==0){
    _tft.setTextColor(COL_DIM,COL_BG);
    _tft.drawString("No BLE devices",40,150,2);
    return;
  }
  int maxVisible=11;
  for(int i=scroll;i<count&&i<scroll+maxVisible;i++){
    int y=24+(i-scroll)*24;
    bool isSel=(i==sel);
    _tft.fillRect(0,y,240,23,isSel?COL_PANEL:COL_BG);
    if(isSel) _tft.drawRect(0,y,240,23,COL_ACCENT);
    _tft.setTextColor(isSel?COL_ACCENT:COL_WHITE,isSel?COL_PANEL:COL_BG);
    String n=name[i].length()?name[i]:"Unknown";
    if(n.length()>18) n=n.substring(0,17)+"~";
    _tft.drawString(n.c_str(),4,y+4,1);
    _tft.setTextColor(COL_DIM,isSel?COL_PANEL:COL_BG);
    _tft.drawString(String(rssi[i]).c_str(),210,y+4,1);
  }
  _footer(("BLE devices: "+String(count)).c_str());
}

void Display::drawBLEStats(int total) {}

void Display::drawBLEDetail(String name, String addr, int rssi) {
  _tft.fillRect(20,40,200,160,COL_PANEL);
  _tft.drawRect(20,40,200,160,COL_ACCENT);
  _tft.setTextColor(COL_ACCENT,COL_PANEL);
  _tft.drawString("BLE Device",60,50,2);
  _tft.setTextColor(COL_WHITE,COL_PANEL);
  _tft.drawString(("Name: "+name).c_str(),30,80,1);
  _tft.drawString(("MAC : "+addr).c_str(),30,100,1);
  _tft.drawString(("RSSI: "+String(rssi)+" dBm").c_str(),30,120,1);
  _tft.setTextColor(COL_DIM,COL_PANEL);
  _tft.drawString("Tap to dismiss",55,175,1);
}

// ── Network / AP ─────────────────────────────────────────────────────────────

void Display::drawNetworkList(String ip[], String mac[], int count) {
  _tft.fillRect(0,22,240,286,COL_BG);
  _header("Network Scan",COL_ACCENT);
  if(count==0){
    _tft.setTextColor(COL_DIM,COL_BG);
    _tft.drawString("No hosts found",50,150,2);
    return;
  }
  for(int i=0;i<count;i++){
    int y=30+i*30;
    _tft.setTextColor(COL_WHITE,COL_BG);
    _tft.drawString(ip[i].c_str(),4,y,2);
    _tft.setTextColor(COL_DIM,COL_BG);
    _tft.drawString(mac[i].c_str(),4,y+14,1);
  }
}

void Display::drawAPManager(String ip[], String mac[], int count) {
  drawNetworkList(ip,mac,count);
  _header("AP Manager",COL_YELLOW);
}

// ── Port scan ─────────────────────────────────────────────────────────────────

void Display::drawPortScan(String target, int ports[], String svcs[],
                            int count, bool scanning) {
  _tft.fillRect(0,22,240,286,COL_BG);
  _header("Port Scanner",COL_ACCENT);
  _tft.setTextColor(COL_DIM,COL_BG);
  _tft.drawString(("Target: "+target).c_str(),4,28,1);
  if(scanning){
    _tft.setTextColor(COL_YELLOW,COL_BG);
    _tft.drawString("Scanning...",80,150,2);
    return;
  }
  if(count==0){
    _tft.setTextColor(COL_DIM,COL_BG);
    _tft.drawString("No open ports",55,150,2);
    return;
  }
  for(int i=0;i<count&&i<10;i++){
    int y=42+i*26;
    _tft.setTextColor(COL_ACCENT,COL_BG);
    _tft.drawString(String(ports[i]).c_str(),4,y,2);
    _tft.setTextColor(COL_WHITE,COL_BG);
    _tft.drawString(svcs[i].c_str(),60,y,2);
  }
}

// ── Channel map ───────────────────────────────────────────────────────────────

void Display::drawChannelMap(int nets[], int chBest, int chBusy) {
  _tft.fillRect(0,22,240,286,COL_BG);
  _header("Channel Map",COL_ACCENT);
  int maxNets=1;
  for(int i=1;i<=13;i++) if(nets[i]>maxNets) maxNets=nets[i];

  for(int ch=1;ch<=13;ch++){
    int x=8+(ch-1)*17;
    int barH=(nets[ch]*60)/maxNets;
    uint16_t col=(ch==chBest)?COL_ACCENT:(ch==chBusy)?COL_RED:COL_DIM;
    _tft.fillRect(x,180-barH,12,barH,col);
    _tft.setTextColor(COL_DIM,COL_BG);
    _tft.drawString(String(ch).c_str(),x,185,1);
  }
  _tft.setTextColor(COL_ACCENT,COL_BG);
  _tft.drawString(("Best: ch"+String(chBest)).c_str(),20,210,2);
  _tft.setTextColor(COL_RED,COL_BG);
  _tft.drawString(("Busy: ch"+String(chBusy)).c_str(),130,210,2);
}

// ── Settings ─────────────────────────────────────────────────────────────────

void Display::drawSettingsScreen() {
  _tft.fillScreen(COL_BG);
  _header("Settings",COL_ACCENT);
  _tft.setTextColor(COL_WHITE,COL_BG);
  _tft.drawString("Use web UI to",40,100,2);
  _tft.drawString("change settings:",24,130,2);
  _tft.setTextColor(COL_ACCENT,COL_BG);
  _tft.drawString("192.168.4.1",38,165,2);
  _tft.setTextColor(COL_DIM,COL_BG);
  _tft.drawString("PIN: 1234",72,195,2);
}

// ── Attack status screen ─────────────────────────────────────────────────────

void Display::drawAttackStatus(const char* name, bool running) {
  _tft.fillScreen(COL_BG);

  // Accent top bar
  _tft.fillRect(0,0,240,3, running?COL_ACCENT:COL_DIM);

  // Header bar
  _tft.fillRect(0,3,240,20,COL_PANEL);
  _tft.setTextColor(COL_DIM,COL_PANEL);
  _tft.drawString("ESP32-CYBER", 6, 7, 1);
  String statusStr = running ? "ACTIVE" : "STOPPED";
  uint16_t statusCol = running ? COL_ACCENT : COL_RED;
  _tft.setTextColor(statusCol,COL_PANEL);
  _tft.drawString(statusStr.c_str(), 200, 7, 1);

  // Attack name
  _tft.setTextColor(running?COL_ACCENT:COL_DIM, COL_BG);
  _tft.drawString(name, 10, 46, 2);

  // Status dot
  _tft.fillCircle(18, 110, 7, running?COL_ACCENT:COL_RED);
  if(running) _tft.drawCircle(18, 110, 10, COL_ACCENT); // outer ring when active
  _tft.setTextColor(COL_WHITE,COL_BG);
  _tft.drawString(running?"RUNNING":"STOPPED", 34, 104, 2);

  // Hint at bottom
  _tft.setTextColor(COL_DIM,COL_BG);
  _tft.drawLine(0,280,240,280,COL_PANEL);
  _tft.drawString("192.168.4.1", 72, 285, 1);
}

// ── Touch ─────────────────────────────────────────────────────────────────────

bool Display::getTouch(uint16_t& x, uint16_t& y) {
  return _tft.getTouch(&x, &y);
}
