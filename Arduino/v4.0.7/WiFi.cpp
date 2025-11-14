#include "GCU.h"
#include <esp_wifi.h>

/**
 * @brief Global provisioning/Wi-Fi event handler (runs from system task).
 * @param sys_event Arduino provisioning event pointer.
 */
void SysProvEvent(arduino_event_t *sys_event) {
  switch (sys_event->event_id) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("\nConnected IP address : ");
      Serial.println(IPAddress(sys_event->event_info.got_ip.ip_info.ip.addr));
      g_wifiGotIP = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: Serial.println("\nDisconnected. Connecting to the AP again... "); break;
    case ARDUINO_EVENT_PROV_START:            Serial.println("\nProvisioning started\nGive Credentials of your access point using smartphone app"); break;
    case ARDUINO_EVENT_PROV_CRED_RECV:
    {
      Serial.println("\nReceived Wi-Fi credentials");
      Serial.print("\tSSID : ");
      Serial.println((const char *)sys_event->event_info.prov_cred_recv.ssid);
      Serial.print("\tPassword : ");
      Serial.println((char const *)sys_event->event_info.prov_cred_recv.password);
      break;
    }
    case ARDUINO_EVENT_PROV_CRED_FAIL:
    {
      Serial.println("\nProvisioning failed!\nPlease reset to factory and retry provisioning\n");
      if (sys_event->event_info.prov_fail_reason == NETWORK_PROV_WIFI_STA_AUTH_ERROR) {
        Serial.println("\nWi-Fi AP password incorrect");
      } else {
        Serial.println("\nWi-Fi AP not found....Add API \" nvs_flash_erase() \" before beginProvision()");
      }
      break;
    }
    case ARDUINO_EVENT_PROV_CRED_SUCCESS: Serial.println("\nProvisioning Successful"); break;
    case ARDUINO_EVENT_PROV_END:          Serial.println("\nProvisioning Ends"); break;
    default:                              break;
  }
}

/**
 * @brief Wait for Wi-Fi connection or reboot when the timeout expires.
 * @param timeout_ms Maximum time to wait (milliseconds).
 * @param blinkDuringProvision When true, blink LED to signal provisioning.
 * @return True if Wi-Fi connected before timeout, otherwise never returns (reboots).
 */
bool waitForWiFiOrReboot(uint32_t timeout_ms, bool blinkDuringProvision) {

  uint32_t t0 = millis();

  while (millis() - t0 < timeout_ms) {
    if (blinkDuringProvision) {
      // Blink once per second while waiting
      ledBlinkColor(LedPalette::WifiProvision, 1, 1000);
    }
    // Auto-clear boot counter once if boot window elapsed
    clearBootCounterIfTimeout(BOOTCNT_CLEAR_WINDOW_MS);
    if (g_wifiGotIP || WiFi.status() == WL_CONNECTED) {
      ledShowColor(LedPalette::Off); // Connected -> turn off LED
      return true;
    }
    delay(100);
  }

  // Timeout waiting for Wi-Fi: flash amber rapidly to indicate error
  ledBlinkColor(LedPalette::WifiTimeout, 10, 200);
  ledShowColor(LedPalette::WifiTimeout);
  delay(2000);
  ESP.restart();
  while (1) { delay(1000); }
}

/**
 * @brief Check if STA credentials exist in the ESP-IDF Wi-Fi config storage.
 * @return True if SSID is non-empty in esp_wifi config.
 */
bool hasSavedWiFiCredentials() {
  wifi_config_t conf;
  memset(&conf, 0, sizeof(conf));
  if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
    return conf.sta.ssid[0] != 0;
  }
  return false;
}
