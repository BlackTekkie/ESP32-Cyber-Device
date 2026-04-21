#include "input.h"
#include <Arduino.h>

#define BTN_NEXT   0
#define BTN_SELECT 35

void Input::begin() {
  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
}

bool Input::nextPressed() {
  if (digitalRead(BTN_NEXT) == LOW) {
    delay(150);
    return true;
  }
  return false;
}

bool Input::selectPressed() {
  if (digitalRead(BTN_SELECT) == LOW) {
    delay(150);
    return true;
  }
  return false;
}
