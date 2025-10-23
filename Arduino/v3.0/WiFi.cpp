#include "GCU.h"

// WARNING: SysProvEvent is called from a separate FreeRTOS task (thread)!
void SysProvEvent(arduino_event_t *sys_event) {
  switch (sys_event->event_id) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("\nConnected IP address : ");
      Serial.println(IPAddress(sys_event->event_info.got_ip.ip_info.ip.addr));
      g_wifiGotIP = false;
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

bool waitForWiFiOrReboot(uint32_t timeout_ms, bool blinkDuringProvision) {

  uint32_t t0 = millis();

  while (millis() - t0 < timeout_ms) {
    if (blinkDuringProvision) {
    // 例如每秒闪 1 次，循环持续整个等待期
    neopixelBlink(GCU_RGB_BRIGHTNESS, 0, 0, 1, 1000);
    }
    if (g_wifiGotIP || WiFi.status() == WL_CONNECTED) {
      neopixelWrite(0, 0, 0); // 连接成功 -> 灯灭
      return true;
    }
    delay(100);
  }

  // 超时未连上
  neopixelBlink(GCU_RGB_BRIGHTNESS, 0, 0, 10, 200);  // 红灯狂闪提示错误
  Serial.println("[NET] Timeout waiting WiFi. Rebooting...");
  delay(2000);
  ESP.restart();
  while (1) { delay(1000); }
}
