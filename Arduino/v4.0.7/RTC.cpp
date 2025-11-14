#include "GCU.h"

// Initialize RTC time via NTP servers. Waits for a second tick change
// and sets the ESP32Time rtc using the resulting epoch time.
// Returns true on success, false if time not yet available.
/**
 * @brief Synchronize the ESP32Time RTC using configured NTP servers.
 * @return True when NTP succeeds and RTC is updated.
 */
bool init_RTC_from_net() {
  struct tm timeinfo1;
  struct tm timeinfo2;
  time_t epochTime;

  sntp_servermode_dhcp(1);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);

  if (!getLocalTime(&timeinfo1)) {
    Serial.println("No time available (yet)");
    return false;
  }
  do {
    if (!getLocalTime(&timeinfo2)) {
      Serial.println("No time available (yet)");
      return false;
    }
  } while (timeinfo2.tm_sec == timeinfo1.tm_sec);

  epochTime = mktime(&timeinfo2);
  rtc.setTime(epochTime);
  // Note: bq32002 hardware removed; skip persisting to external RTC
  // write_tm_to_bq32002(timeinfo2);
  Serial.println();
  Serial.println(epochTime);
  Serial.println(rtc.getTime("%A, %B %d %Y %H:%M:%S"));
  return true;
}

/**
 * @brief Log a generic RTC synchronization error.
 */
void RTC_error() {
  Serial.println("RTC sync failed.");
}
