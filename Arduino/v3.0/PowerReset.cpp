#include "GCU.h"

// 在NVS中自增开机计数；若达到阈值则返回true（表示强制重配网）
bool checkMultiPowerCycleForceProvision(unsigned int POWER_CYCLE_WINDOW_MS, unsigned int POWER_CYCLE_THRESHOLD) {
  prefs.begin("provctl", false);
  uint8_t cnt = prefs.getUChar("bootcnt", 0);
  cnt++;
  prefs.putUChar("bootcnt", cnt);
  prefs.end();

  bootcnt_local = cnt;
  Serial.printf("[BootCtl] bootcnt=%u\n", cnt);

  // 设置一个“超时后清零”的时刻；超过窗口期再清零
  windowDeadline = millis() + POWER_CYCLE_WINDOW_MS;

  if (cnt >= POWER_CYCLE_THRESHOLD) {
    // 达到阈值：清零计数，声明强制重配
    prefs.begin("provctl", false);
    prefs.putUChar("bootcnt", 0);
    prefs.end();
    bootcnt_local = 0;
    Serial.println("[BootCtl] Threshold reached → force provisioning.");
    return true;
  }
  return false;
}

// 超过窗口期后，把NVS计数清零（避免误触发）
void tryClearBootCounterAfterWindow() {
  if (!windowCleared && millis() > windowDeadline) {
    prefs.begin("provctl", false);
    prefs.putUChar("bootcnt", 0);
    prefs.end();
    windowCleared = true;
    Serial.println("[BootCtl] Window expired → bootcnt reset to 0.");
  }
}

void ClearBootCounter() {
  prefs.begin("provctl", false);
  prefs.putUChar("bootcnt", 0);
  prefs.end();
  windowCleared = true;
  Serial.println("[BootCtl] Window expired → bootcnt reset to 0.");
}