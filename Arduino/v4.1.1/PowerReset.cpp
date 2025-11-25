// Power/reset helpers
#include "GCU.h"
#include <nvs_flash.h>

// Avoid double-increment within the same boot
static bool s_bootcntUpdated = false;

/**
 * @brief Increment the boot counter at most once per boot and cache the value.
 * @return Latest boot counter persisted in NVS.
 */
static uint8_t incrementBootCounterOnce() {
  if (s_bootcntUpdated) {
    Serial.printf("[BootCtl] bootcnt (cached)=%u\n", bootcnt_local);
    return bootcnt_local;
  }
  prefs.begin("provctl", false);
  uint8_t before = prefs.getUChar("bootcnt", 0);
  Serial.printf("[BootCtl] before=%u\n", before);

  uint8_t cnt = (uint8_t)(before + 1);
  prefs.putUChar("bootcnt", cnt);
  prefs.end();

  bootcnt_local = cnt;
  s_bootcntUpdated = true;
  Serial.printf("[BootCtl] after=%u\n", cnt);
  Serial.printf("[BootCtl] bootcnt=%u\n", cnt);
  return cnt;
}

/**
 * @brief Increment boot counter and request reprovisioning when threshold met.
 * @param POWER_CYCLE_THRESHOLD Number of reboots required to trigger.
 * @return True when provisioning should be forced.
 */
bool checkMultiPowerCycleForceProvision(unsigned int POWER_CYCLE_THRESHOLD) {
  uint8_t cnt = incrementBootCounterOnce();
  if (cnt >= POWER_CYCLE_THRESHOLD) {
    // Do NOT reset the counter here; allow it to keep increasing
    // so a higher threshold (e.g., factory reset) can still be reached
    Serial.println("[BootCtl] Threshold reached -> force provisioning flag.");
    return true;
  }
  return false;
}

/**
 * @brief Increment boot counter and factory reset (erase NVS) on threshold.
 * @param FACTORY_RESET_THRESHOLD Reboot count required for factory reset.
 * @return True if factory reset path executed (device restarts).
 */
bool checkMultiPowerCycleFactoryReset(unsigned int FACTORY_RESET_THRESHOLD) {
  uint8_t cnt = incrementBootCounterOnce();
  if (cnt >= FACTORY_RESET_THRESHOLD) {
    Serial.println("[BootCtl] Factory-reset threshold reached -> erasing NVS...");
    // Erase entire NVS partition
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK) {
      Serial.printf("[BootCtl] nvs_flash_erase failed: 0x%X\n", (unsigned)err);
    }
    err = nvs_flash_init();
    if (err != ESP_OK) {
      Serial.printf("[BootCtl] nvs_flash_init failed: 0x%X\n", (unsigned)err);
    }
    delay(200);
    Serial.println("[BootCtl] NVS erased. Restarting...");
    delay(100);
    ESP.restart();
    return true; // not reached due to restart
  }
  return false;
}

/**
 * @brief Clear the boot counter and mark the detection window as expired.
 */
void ClearBootCounter() {
  prefs.begin("provctl", false);
  prefs.putUChar("bootcnt", 0);
  prefs.end();
  windowCleared = true;
  Serial.println("[BootCtl] Window expired -> bootcnt reset to 0.");
}

/**
 * @brief Automatically clear boot counter after a timeout window.
 * @param window_ms Milliseconds after boot before resetting the counter.
 */
void clearBootCounterIfTimeout(uint32_t window_ms) {
  if (windowCleared) return;
  if (millis() >= window_ms) {
    Serial.printf("[BootCtl] Boot window %ums elapsed. Clearing bootcnt...\n", (unsigned)window_ms);
    ClearBootCounter();
  }
}
