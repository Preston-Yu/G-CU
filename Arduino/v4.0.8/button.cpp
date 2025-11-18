#include "GCU.h"

/**
 * @brief Detect a long press on boot to trigger provisioning or factory reset.
 * @param BUTTON_PIN GPIO used for the button (active low).
 * @param pressThresholdMs Duration (ms) that qualifies as a long press.
 * @return True when the button stayed low beyond the threshold inside the boot window.
 */
bool wasButtonLongPressedOnBoot(unsigned char BUTTON_PIN, unsigned long pressThresholdMs) {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  unsigned long t0 = millis();
  unsigned long pressStart = 0;
  bool counting = false;

  const unsigned long DETECT_WINDOW_MS = 4000;  // Boot detection window
  const unsigned long DEBOUNCE_MS = 30;         // Debounce time
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
        return true;  // Reached custom long-press duration
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
