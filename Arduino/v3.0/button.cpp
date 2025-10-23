#include "GCU.h"

bool wasButtonLongPressedOnBoot(unsigned char BUTTON_PIN, unsigned long pressThresholdMs) {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  unsigned long t0 = millis();
  unsigned long pressStart = 0;
  bool counting = false;

  const unsigned long DETECT_WINDOW_MS = 4000;  // 上电检测窗口
  const unsigned long DEBOUNCE_MS = 30;         // 消抖时间

  while (millis() - t0 < DETECT_WINDOW_MS) {
    int level = digitalRead(BUTTON_PIN);
    unsigned long now = millis();

    if (level == LOW) {
      static unsigned long lowSince = 0;
      if (lowSince == 0) lowSince = now;
      if (!counting && (now - lowSince >= DEBOUNCE_MS)) {
        counting = true;
        pressStart = now;
      }
      if (counting && (now - pressStart >= pressThresholdMs)) {
        return true;  // 达到自定义长按时间
      }
    } else {
      counting = false;
      pressStart = 0;
      static unsigned long lowSince = 0;
      lowSince = 0;
    }

    delay(5);
  }
  return false;
}
