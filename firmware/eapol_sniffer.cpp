/**
 * eapol_sniffer.cpp — WPA2 4-way Handshake Capture
 * --------------------------------------------------
 * Captures EAPOL M1+M2 frames and outputs hashcat 22000 format.
 *
 * Sky Q Hub confirmed: does NOT include PMKID in M1.
 * However M1+M2 together = full WPA2 handshake = crackable offline.
 *
 * hashcat -m 22000 capture.hc22000 wordlist.txt
 *
 * Detection method: Marauder fixed-offset
 *   payload[30..31] == 0x88 0x8E  (non-QoS data frame)
 *   payload[32..33] == 0x88 0x8E  (QoS data frame)
 *
 * Stays on home channel (ch1 = softAP channel = Sky channel).
 * No hopping — avoids promiscuous mode conflicts with the AP.
 */
#include "eapol_sniffer.h"
#include <string.h>

EapolSniffer* EapolSniffer::_instance = nullptr;

void EapolSniffer::begin() {
  _running=false; _count=0; _instance=this;
}

bool EapolSniffer::start() {
  _running=true; _count=0;
  _targetedRunning=false;

  // Detect our current AP channel
  uint8_t apCh=1;
  wifi_second_chan_t sec;
  esp_wifi_get_channel(&apCh,&sec);
  _homeChannel=apCh; _channel=apCh;

  // Accept ALL frame types — same as Marauder
  wifi_promiscuous_filter_t f={.filter_mask=WIFI_PROMIS_FILTER_MASK_ALL};
  esp_wifi_set_promiscuous_filter(&f);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(snifferCb);

  // Stay on home channel unless user explicitly locked to another
  uint8_t ch=(_targetChannel>0)?_targetChannel:_homeChannel;
  esp_wifi_set_channel(ch,WIFI_SECOND_CHAN_NONE);
  _channel=ch;

  Serial.printf("EAPOL: started ch%d (AP home=ch%d)\n",ch,_homeChannel);
  Serial.println("EAPOL: reconnect any device to SKYQG4GT to capture handshake");
  Serial.println("EAPOL: M1+M2 = hashcat 22000 crackable — no PMKID needed");
  return true;
}

void EapolSniffer::stop() {
  _running=false;
  _targetedRunning=false;
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  delay(30);
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_channel(_homeChannel,WIFI_SECOND_CHAN_NONE);
  Serial.printf("EAPOL: stopped — %d frames captured\n",_count);
}

void EapolSniffer::snifferCb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if(!_instance||!_instance->_running) return;
  wifi_promiscuous_pkt_t* p=(wifi_promiscuous_pkt_t*)buf;
  _instance->handlePacket(p->payload,p->rx_ctrl.sig_len,p->rx_ctrl.channel);
}

String EapolSniffer::macToStr(const uint8_t* m) {
  char b[18];
  snprintf(b,18,"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
  return String(b);
}

bool EapolSniffer::isEapol(const uint8_t* d, int len) {
  if(len<36) return false;
  // Marauder fixed-offset: EtherType 0x888E at byte 30 (non-QoS) or 32 (QoS)
  if(len>33 && d[30]==0x88 && d[31]==0x8E) return true;
  if(len>35 && d[32]==0x88 && d[33]==0x8E) return true;
  return false;
}

void EapolSniffer::handlePacket(const uint8_t* data, int len, int channel) {
  if(!isEapol(data,len)) return;
  if(_count>=MAX_EAPOL_CAPTURES) return;

  // Which offset did the EtherType hit?
  int eapolStart = (data[30]==0x88 && data[31]==0x8E) ? 32 : 34;

  if(len<eapolStart+4) return;
  if(data[eapolStart+1]!=0x03) return; // not EAPOL-Key

  // Key Info at key[1..2] (body starts after 4-byte EAPOL header)
  const uint8_t* k=data+eapolStart+4;
  int klen=len-eapolStart-4;
  if(klen<7) return;

  uint16_t info=((uint16_t)k[1]<<8)|k[2];
  bool ack    =(info&0x0080)!=0;
  bool mic    =(info&0x0100)!=0;
  bool install=(info&0x0040)!=0;
  bool secure =(info&0x0200)!=0;
  bool keyType=(info&0x0008)!=0; // 1=pairwise

  uint8_t msg=0;
  if(keyType && ack && !mic)                         msg=1;
  else if(keyType && !ack && mic && !install && !secure) msg=2;
  else if(keyType && ack && mic && install)               msg=3;
  else if(keyType && !ack && mic && !install && secure)   msg=4;
  if(msg==0) return;

  // Extract MACs
  uint8_t fc1=data[1];
  bool fromDS=(fc1&0x02)!=0;
  const uint8_t* apMacB  = fromDS?(data+10):(data+16);
  const uint8_t* staMacB = fromDS?(data+4) :(data+10);
  String apMAC  = macToStr(apMacB);
  String staMAC = macToStr(staMacB);

  // Deduplicate
  for(int i=0;i<_count;i++) {
    if(_caps[i].apMAC==apMAC && _caps[i].clientMAC==staMAC && _caps[i].type==msg)
      return;
  }

  // Store metadata + raw frame bytes
  EapolCapture& c=_caps[_count];
  c.clientMAC=staMAC;
  c.apMAC=apMAC;
  c.channel=channel;
  c.type=msg;
  c.timestamp=millis();
  c.apSSID="";
  c.rawLen=min(len,(int)MAX_EAPOL_FRAME_LEN);
  memcpy(c.rawFrame,data,c.rawLen);
  _count++;

  Serial.printf("EAPOL: M%d ap=%s sta=%s ch=%d\n",msg,apMAC.c_str(),staMAC.c_str(),channel);

  // Check if we now have a complete M1+M2 pair for this handshake
  bool hasM1=false, hasM2=false;
  for(int i=0;i<_count;i++) {
    if(_caps[i].apMAC==apMAC && _caps[i].clientMAC==staMAC) {
      if(_caps[i].type==1) hasM1=true;
      if(_caps[i].type==2) hasM2=true;
    }
  }
  if(hasM1&&hasM2) {
    Serial.println("╔══════════════════════════════════════════╗");
    Serial.println("║  WPA2 HANDSHAKE COMPLETE — M1+M2 READY  ║");
    Serial.println("╠══════════════════════════════════════════╣");
    Serial.printf( "║ AP  : %s\n",apMAC.c_str());
    Serial.printf( "║ STA : %s\n",staMAC.c_str());
    Serial.printf( "║ Ch  : %d\n",channel);
    Serial.println("║ Use Download .hc22000 button to crack    ║");
    Serial.println("╚══════════════════════════════════════════╝");
  }
}

uint32_t EapolSniffer::getTargetedSecondsLeft() {
  if(!_targetedRunning) return 0;
  uint32_t now=millis();
  return (_targetedExpiry>now)?(_targetedExpiry-now)/1000:0;
}

void EapolSniffer::startTargeted(const String& bssid, uint8_t channel) {
  if(!_running) return;
  _targetedBSSID=bssid;
  _targetedRunning=true;
  _targetedExpiry=millis()+8000;
  esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE);
  _channel=channel;
  Serial.printf("EAPOL targeted: ch%d %s\n",channel,bssid.c_str());
}

void EapolSniffer::tick() {
  if(!_running) return;
  // Return home after targeted capture expires
  if(_targetedRunning && millis()>=_targetedExpiry) {
    _targetedRunning=false;
    esp_wifi_set_channel(_homeChannel,WIFI_SECOND_CHAN_NONE);
    _channel=_homeChannel;
    Serial.println("EAPOL: targeted window ended, back on home ch");
  }
}

// ── Hashcat 22000 output ────────────────────────────────────────────────────
// Format: WPA*TYPE*PMKID_or_MIC*AP_MAC*STA_MAC*ESSID_hex*AP_NONCE*EAPOL_STA*MESSAGEPAIR

static String bytesToHex(const uint8_t* b, int len) {
  String s;
  for(int i=0;i<len;i++){char h[3];snprintf(h,3,"%02x",b[i]);s+=h;}
  return s;
}
static String macNoColons(const String& mac) {
  String s=mac; s.replace(":",""); return s;
}

String EapolSniffer::getHashcatLine(int m1idx, int m2idx) {
  if(m1idx<0||m1idx>=_count||m2idx<0||m2idx>=_count) return "";
  EapolCapture& m1=_caps[m1idx];
  EapolCapture& m2=_caps[m2idx];
  if(m1.type!=1||m2.type!=2) return "";

  // Find EAPOL start offsets in each raw frame
  auto eapolOff=[](const uint8_t* d,int len)->int {
    if(len>33 && d[30]==0x88 && d[31]==0x8E) return 32;
    if(len>35 && d[32]==0x88 && d[33]==0x8E) return 34;
    return -1;
  };

  int off1=eapolOff(m1.rawFrame,m1.rawLen);
  int off2=eapolOff(m2.rawFrame,m2.rawLen);
  if(off1<0||off2<0) return "";

  // ANonce from M1: EAPOL-Key body at off+4, Nonce at body[13..44]
  const uint8_t* k1=m1.rawFrame+off1+4;
  if(m1.rawLen<off1+4+45) return "";
  const uint8_t* aNonce=k1+13;

  // From M2: MIC at body[77..92], SNonce at body[13..44]
  const uint8_t* k2=m2.rawFrame+off2+4;
  if(m2.rawLen<off2+4+95) return "";
  const uint8_t* mic=k2+77;

  // EAPOL frame for M2: the full EAPOL payload (from EtherType byte onward)
  // Zero out the MIC field before hashing (standard requirement)
  uint8_t eapolM2[MAX_EAPOL_FRAME_LEN];
  int eapolM2Len=m2.rawLen-off2;
  if(eapolM2Len<=0||eapolM2Len>MAX_EAPOL_FRAME_LEN) return "";
  memcpy(eapolM2,m2.rawFrame+off2,eapolM2Len);
  // Zero the MIC: it starts at EAPOL header(4) + key_body_offset(4) + mic_offset(77) = 85
  if(eapolM2Len>=85+16) memset(eapolM2+85,0,16);

  // ESSID hex
  String ssidHex="";
  for(size_t i=0;i<m1.apSSID.length();i++){
    char h[3]; snprintf(h,3,"%02x",(uint8_t)m1.apSSID[i]); ssidHex+=h;
  }

  // hashcat 22000: WPA*02*MIC*APMAC*STAMAC*SSID_hex*ANonce*EAPOL_M2_hex*messagepair
  String line="WPA*02*";
  line+=bytesToHex(mic,16)+"*";
  line+=macNoColons(m1.apMAC)+"*";
  line+=macNoColons(m1.clientMAC)+"*";
  line+=ssidHex+"*";
  line+=bytesToHex(aNonce,32)+"*";
  line+=bytesToHex(eapolM2,eapolM2Len)+"*";
  line+="02"; // messagepair: 02 = M1+M2
  return line;
}

String EapolSniffer::getHashcatFormat() {
  String out;
  // Find all M1+M2 pairs
  for(int i=0;i<_count;i++) {
    if(_caps[i].type!=1) continue;
    for(int j=0;j<_count;j++) {
      if(_caps[j].type!=2) continue;
      if(_caps[i].apMAC!=_caps[j].apMAC) continue;
      if(_caps[i].clientMAC!=_caps[j].clientMAC) continue;
      String line=getHashcatLine(i,j);
      if(line.length()>0) { out+=line+"\n"; break; }
    }
  }
  return out;
}
