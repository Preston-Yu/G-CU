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
    LOGD("BOOTCTL", "bootcnt cached=%u", bootcnt_local);
    return bootcnt_local;
  }
  prefs.begin("provctl", false);
  uint8_t before = prefs.getUChar("bootcnt", 0);
  LOGD("BOOTCTL", "bootcnt_before=%u", before);

  uint8_t cnt = (uint8_t)(before + 1);
  prefs.putUChar("bootcnt", cnt);
  prefs.end();

  bootcnt_local = cnt;
  s_bootcntUpdated = true;
  LOGD("BOOTCTL", "bootcnt_after=%u", cnt);
  LOGI("BOOTCTL", "bootcnt=%u", cnt);
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
    LOGW("BOOTCTL", "force_provision threshold=%u count=%u", POWER_CYCLE_THRESHOLD, cnt);
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
    // Backup license tokens before wiping NVS
    String licenseBackup;
    {
      Preferences licPrefs;
      if (licPrefs.begin("license", true)) {
        licenseBackup = licPrefs.getString("tokens", "");
        licPrefs.end();
      }
    }

    LOGW("BOOTCTL", "factory_reset threshold=%u count=%u", FACTORY_RESET_THRESHOLD, cnt);
    // Erase entire NVS partition
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK) {
      LOGE("BOOTCTL", "nvs_flash_erase_failed code=0x%X", (unsigned)err);
    }
    err = nvs_flash_init();
    if (err != ESP_OK) {
      LOGE("BOOTCTL", "nvs_flash_init_failed code=0x%X", (unsigned)err);
    }

    // Restore license tokens if we had any
    if (licenseBackup.length() > 0) {
      Preferences licPrefs;
      if (licPrefs.begin("license", false)) {
        if (licPrefs.putString("tokens", licenseBackup) > 0) {
          LOGI("BOOTCTL", "license_tokens_restore status=ok");
        } else {
          LOGE("BOOTCTL", "license_tokens_restore status=fail");
        }
        licPrefs.end();
      } else {
        LOGE("BOOTCTL", "license_namespace_open_failed");
      }
    }

    delay(200);
    LOGI("BOOTCTL", "nvs_erased restart=1");
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
  LOGI("BOOTCTL", "bootcnt_reset");
}

/**
 * @brief Automatically clear boot counter after a timeout window.
 * @param window_ms Milliseconds after boot before resetting the counter.
 */
void clearBootCounterIfTimeout(uint32_t window_ms) {
  if (windowCleared) return;
  if (millis() >= window_ms) {
    LOGI("BOOTCTL", "boot_window_elapsed window_ms=%u", (unsigned)window_ms);
    ClearBootCounter();
  }
}
