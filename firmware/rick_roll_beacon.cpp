#include "rick_roll_beacon.h"

// Rick Roll approach from NoMoreImFull — uses WiFi.softAP() to set real AP name.
// No raw injection needed. Cycles through lyrics every LYRIC_HOLD_MS milliseconds.
// Web UI clients will need to reconnect after each lyric change.

static const char* LYRICS[] = {
  "Were no strangers to love",
  "You know the rules and so do I",
  "A full commitment is",
  "what Im thinking of",
  "You wouldnt get this from",
  "any other guy",
  "I just wanna tell you",
  "how Im feeling",
  "Gotta make you understand",
  "Never gonna give you up",
  "Never gonna let you down",
  "Never gonna run around",
  "and desert you",
  "Never gonna make you cry",
  "Never gonna say goodbye",
  "Never gonna tell a lie",
  "and hurt you",
  "Weve known each other so long",
  "Your hearts been aching but",
  "youre too shy to say it",
  "Inside we both know",
  "whats been going on",
  "We know the game",
  "and were gonna play it"
};
static const int LC = 24;

// How long to show each lyric (ms) — long enough for phones to see it
#define LYRIC_HOLD_MS 8000

void RickRollBeacon::begin() { _running=false; _sent=0; }
void RickRollBeacon::stop() {
  _running = false;
  // Restore original AP name
  WiFi.softAP("ESP32-Cyber", "esp32cyber");
  Serial.println("RickRoll: stopped, AP restored");
}

bool RickRollBeacon::start() {
  _idx=0; _sent=0; _lastTx=0; _running=true;
  // Set first lyric immediately
  WiFi.softAP(LYRICS[0], "esp32cyber");
  Serial.printf("RickRoll: %s\n", LYRICS[0]);
  return true;
}

void RickRollBeacon::tick() {
  if(!_running) return;
  static uint32_t dbg=0; if(millis()-dbg>2000){Serial.printf("RR tick running, idx=%d\n",_idx);dbg=millis();}
  uint32_t now = millis();
  if(now - _lastTx < LYRIC_HOLD_MS) return;
  _lastTx = now;

  _idx = (_idx + 1) % LC;
  WiFi.softAP(LYRICS[_idx], "esp32cyber");
  _sent++;
  Serial.printf("RickRoll: %s\n", LYRICS[_idx]);
}
