#include "ControlServer.h"
#include "SensorConfig.h"
#include <License.h>
#include "GCU.h"

#include <WiFiServer.h>
#include <WiFiClient.h>

namespace {
constexpr uint16_t kControlPort = 22345;
constexpr size_t kBufferMax = 512;

WiFiServer g_server(kControlPort);
WiFiClient g_client;
String g_buffer;
bool g_started = false;
}

void beginControlServer() {
  if (g_started) return;
  g_server.begin();
  g_server.setNoDelay(true);
  g_started = true;
  Serial.printf("Control TCP server listening on port %u\n", kControlPort);
}

static bool parseStringValue(const String& source, const char* key, String& out) {
  String keyPattern = "\"" + String(key) + "\"";
  int keyIdx = source.indexOf(keyPattern);
  if (keyIdx < 0) {
    return false;
  }
  int colonIdx = source.indexOf(':', keyIdx + keyPattern.length());
  if (colonIdx < 0) {
    return false;
  }
  int firstQuote = source.indexOf('"', colonIdx);
  if (firstQuote < 0) {
    return false;
  }
  int secondQuote = source.indexOf('"', firstQuote + 1);
  if (secondQuote < 0) {
    return false;
  }
  out = source.substring(firstQuote + 1, secondQuote);
  out.trim();
  return out.length() > 0;
}

static bool parseUintArray(const String& source, const char* key, uint8_t* out, size_t& count, size_t maxCount) {
  count = 0;
  String keyPattern = "\"" + String(key) + "\"";
  int keyIdx = source.indexOf(keyPattern);
  if (keyIdx < 0) {
    return false;
  }
  int arrayStart = source.indexOf('[', keyIdx);
  int arrayEnd = source.indexOf(']', arrayStart);
  if (arrayStart < 0 || arrayEnd < 0 || arrayEnd <= arrayStart) {
    return false;
  }
  int cursor = arrayStart + 1;
  while (cursor < arrayEnd && count < maxCount) {
    int nextSep = source.indexOf(',', cursor);
    if (nextSep < 0 || nextSep > arrayEnd) {
      nextSep = arrayEnd;
    }
    String token = source.substring(cursor, nextSep);
    token.trim();
    if (token.length() > 0) {
      int value = token.toInt();
      if (value < 0 || value > 255) {
        return false;
      }
      out[count++] = static_cast<uint8_t>(value);
    }
    cursor = nextSep + 1;
  }
  return count > 0;
}

static void respondError(const char* msg) {
  if (!g_client || !g_client.connected()) return;
  g_client.print("{\"status\":\"error\",\"msg\":\"");
  g_client.print(msg);
  g_client.println("\"}");
}

static void respondOk(const char* msg = nullptr) {
  if (!g_client || !g_client.connected()) return;
  if (msg && msg[0]) {
    g_client.print("{\"status\":\"ok\",\"msg\":\"");
    g_client.print(msg);
    g_client.println("\"}");
  } else {
    g_client.println("{\"status\":\"ok\"}");
  }
}

static void respondJson(const String& json) {
  if (!g_client || !g_client.connected()) return;
  g_client.println(json);
}

static void handlePayload(const String& payload) {
  String trimmed = payload;
  trimmed.trim();
  if (trimmed.length() == 0) {
    respondError("empty");
    return;
  }

  String licenseToken;
  bool hasLicense = parseStringValue(trimmed, "license", licenseToken);

  bool analogPresent = trimmed.indexOf("\"analog\"") >= 0;
  bool selectPresent = trimmed.indexOf("\"select\"") >= 0;

  if (hasLicense && licenseToken == "?") {
    // Query stored licenses
    extern String licenseDescribeAll();
    String desc = licenseDescribeAll();
    respondJson(desc);
    g_client.stop();
    return;
  }

  if (hasLicense && !analogPresent && !selectPresent) {
    bool licOk = licenseApplyToken(licenseToken, rtc.getEpoch());
    if (licOk) {
      respondOk("license_ok");
      Serial.println("License updated via TCP");
      ledShowColor(LedPalette::ReadyIdle);
    } else {
      const char* reason = licenseLastError();
      respondError((reason && reason[0]) ? reason : "license_invalid");
    }
    g_client.stop();
    return;
  }

  if (!analogPresent || !selectPresent) {
    respondError("analog/select_required");
    return;
  }

  uint8_t rows[kMaxAnalogPins];
  size_t rowCount = 0;
  uint8_t cols[kMaxSelectPins];
  size_t colCount = 0;
  bool analogOk = parseUintArray(trimmed, "analog", rows, rowCount, kMaxAnalogPins);
  bool selectOk = parseUintArray(trimmed, "select", cols, colCount, kMaxSelectPins);
  if (!analogOk || !selectOk) {
    respondError("parse_error");
    return;
  }

  if (!sensorConfigApplyJson(trimmed)) {
    const char* reason = sensorConfigLastError();
    respondError((reason && reason[0]) ? reason : "invalid");
    return;
  }

  sensorConfigSaveJson(trimmed);
  bool licOk = true;
  if (hasLicense) {
    licOk = licenseApplyToken(licenseToken, rtc.getEpoch());
  }
  if (licOk) {
    respondOk();
    Serial.println("Sensor IO config updated via TCP");
    ledShowColor(LedPalette::ReadyIdle);
    g_client.stop();
  } else {
    const char* reason = licenseLastError();
    respondError(reason ? reason : "license_invalid");
  }
}

void handleControlServer() {
  if (!g_started) {
    return;
  }
  if (!g_client || !g_client.connected()) {
    if (g_client) {
      g_client.stop();
    }
    WiFiClient nextClient = g_server.available();
    if (nextClient) {
      g_client = nextClient;
      g_buffer = "";
      Serial.println("Control client connected");
    }
    return;
  }

  while (g_client.available()) {
    char c = g_client.read();
    if (c == '\r') continue;
    if (c == '\n') {
      handlePayload(g_buffer);
      g_buffer = "";
      break;
    }
    if (g_buffer.length() < kBufferMax) {
      g_buffer += c;
    }
  }
}
