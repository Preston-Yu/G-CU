#include "ControlServer.h"
#include "SensorConfig.h"
#include <License.h>
#include "GCU.h"

#include <WiFiServer.h>
#include <WiFiClient.h>
#include <SPIFFS.h>
#include <mbedtls/base64.h>
#include <vector>

namespace {
constexpr uint16_t kControlPort = 22345;
constexpr uint16_t kDiscoveryPort = 22346;
// Enlarged RX buffer to support SPIFFS read/write payloads with base64
constexpr size_t kBufferMax = 4096;
constexpr size_t kSerialLogMax = 160;
constexpr char kDiscoveryMagic[] = "GCU_DISCOVER";

WiFiServer g_server(kControlPort);
WiFiClient g_client;
String g_buffer;
bool g_started = false;
WiFiUDP g_discoveryUdp;
}

void beginControlServer() {
  if (g_started) return;
  g_server.begin();
  g_server.setNoDelay(true);
  if (!g_discoveryUdp.begin(kDiscoveryPort)) {
    LOGE("CTRL", "discovery_bind status=fail port=%u", kDiscoveryPort);
  } else {
    LOGI("CTRL", "discovery_listen port=%u", kDiscoveryPort);
  }
  g_started = true;
  LOGI("CTRL", "tcp_listen port=%u", kControlPort);
}

static bool parseStringValue(const String& source, const char* key, String& out) {
  String keyPattern = "\"" + String(key) + "\"";
  int keyIdx = source.indexOf(keyPattern);
  if (keyIdx < 0) return false;
  int colonIdx = source.indexOf(':', keyIdx + keyPattern.length());
  if (colonIdx < 0) return false;
  int firstQuote = source.indexOf('"', colonIdx);
  if (firstQuote < 0) return false;
  int secondQuote = source.indexOf('"', firstQuote + 1);
  if (secondQuote < 0) return false;
  out = source.substring(firstQuote + 1, secondQuote);
  out.trim();
  return out.length() > 0;
}

static bool extractScalarValue(const String& source, const char* key, String& out) {
  String keyPattern = "\"" + String(key) + "\"";
  int keyIdx = source.indexOf(keyPattern);
  if (keyIdx < 0) return false;
  int colonIdx = source.indexOf(':', keyIdx + keyPattern.length());
  if (colonIdx < 0) return false;
  int commaIdx = source.indexOf(',', colonIdx + 1);
  int braceIdx = source.indexOf('}', colonIdx + 1);
  int endIdx = -1;
  if (commaIdx < 0) endIdx = braceIdx;
  else if (braceIdx < 0) endIdx = commaIdx;
  else endIdx = (commaIdx < braceIdx) ? commaIdx : braceIdx;
  if (endIdx < 0) endIdx = source.length();
  out = source.substring(colonIdx + 1, endIdx);
  out.trim();
  return out.length() > 0;
}

static bool parseBoolValue(const String& source, const char* key, bool& out) {
  String val;
  if (!extractScalarValue(source, key, val)) return false;
  val.toLowerCase();
  if (val.startsWith("true")) {
    out = true;
    return true;
  }
  if (val.startsWith("false")) {
    out = false;
    return true;
  }
  return false;
}

static bool parseFloatValue(const String& source, const char* key, float& out) {
  String val;
  if (!extractScalarValue(source, key, val)) return false;
  out = val.toFloat();
  return true;
}

static bool parseIntValue(const String& source, const char* key, int& out) {
  String val;
  if (!extractScalarValue(source, key, val)) return false;
  out = val.toInt();
  return true;
}

static bool parseLogLevel(const String& source, LogLevel& out) {
  String tmp = source;
  tmp.trim();
  tmp.toLowerCase();
  if (tmp == "debug") {
    out = LogLevel::Debug;
    return true;
  }
  if (tmp == "info" || tmp == "ok") {
    out = LogLevel::Info;
    return true;
  }
  if (tmp == "warn" || tmp == "warning") {
    out = LogLevel::Warn;
    return true;
  }
  if (tmp == "error") {
    out = LogLevel::Error;
    return true;
  }
  return false;
}

static bool extractSection(const String& source, const char* key, String& out) {
  String keyPattern = "\"" + String(key) + "\"";
  int keyIdx = source.indexOf(keyPattern);
  if (keyIdx < 0) return false;
  int braceStart = source.indexOf('{', keyIdx);
  if (braceStart < 0) return false;
  int depth = 0;
  for (int i = braceStart; i < source.length(); ++i) {
    char c = source.charAt(i);
    if (c == '{') depth++;
    else if (c == '}') depth--;
    if (depth == 0) {
      out = source.substring(braceStart, i + 1);
      return true;
    }
  }
  return false;
}

static bool parseUintArray(const String& source, const char* key, uint8_t* out, size_t& count, size_t maxCount) {
  count = 0;
  String keyPattern = "\"" + String(key) + "\"";
  int keyIdx = source.indexOf(keyPattern);
  if (keyIdx < 0) return false;
  int arrayStart = source.indexOf('[', keyIdx);
  int arrayEnd = source.indexOf(']', arrayStart);
  if (arrayStart < 0 || arrayEnd < 0 || arrayEnd <= arrayStart) return false;
  int cursor = arrayStart + 1;
  while (cursor < arrayEnd && count < maxCount) {
    int nextSep = source.indexOf(',', cursor);
    if (nextSep < 0 || nextSep > arrayEnd) nextSep = arrayEnd;
    String token = source.substring(cursor, nextSep);
    token.trim();
    if (token.length() > 0) {
      int value = token.toInt();
      if (value < 0 || value > 255) return false;
      out[count++] = static_cast<uint8_t>(value);
    }
    cursor = nextSep + 1;
  }
  return count > 0;
}

static bool base64Encode(const uint8_t* data, size_t len, String& out) {
  size_t outLen = 4 * ((len + 2) / 3) + 4;
  std::vector<unsigned char> buf(outLen, 0);
  size_t written = 0;
  int rc = mbedtls_base64_encode(buf.data(), buf.size(), &written, data, len);
  if (rc != 0 || written == 0) return false;
  buf[written] = 0;
  out = String(reinterpret_cast<char*>(buf.data()));
  return true;
}

static void writeBase64ToClient(WiFiClient& client, const uint8_t* data, size_t len) {
  static const char kBase64Table[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  if (!data || len == 0) {
    return;
  }
  size_t i = 0;
  char out[4];
  char buffer[512];
  size_t buffered = 0;
  while (i + 3 <= len) {
    uint32_t triple = (static_cast<uint32_t>(data[i]) << 16) |
                      (static_cast<uint32_t>(data[i + 1]) << 8) |
                      static_cast<uint32_t>(data[i + 2]);
    out[0] = kBase64Table[(triple >> 18) & 0x3F];
    out[1] = kBase64Table[(triple >> 12) & 0x3F];
    out[2] = kBase64Table[(triple >> 6) & 0x3F];
    out[3] = kBase64Table[triple & 0x3F];
    if (buffered + 4 > sizeof(buffer)) {
      client.write(reinterpret_cast<const uint8_t*>(buffer), buffered);
      buffered = 0;
    }
    buffer[buffered++] = out[0];
    buffer[buffered++] = out[1];
    buffer[buffered++] = out[2];
    buffer[buffered++] = out[3];
    i += 3;
  }
  if (i < len) {
    uint32_t triple = static_cast<uint32_t>(data[i]) << 16;
    int pad = 2;
    if (i + 1 < len) {
      triple |= static_cast<uint32_t>(data[i + 1]) << 8;
      pad = 1;
    }
    out[0] = kBase64Table[(triple >> 18) & 0x3F];
    out[1] = kBase64Table[(triple >> 12) & 0x3F];
    out[2] = (pad == 2) ? '=' : kBase64Table[(triple >> 6) & 0x3F];
    out[3] = '=';
    if (buffered + 4 > sizeof(buffer)) {
      client.write(reinterpret_cast<const uint8_t*>(buffer), buffered);
      buffered = 0;
    }
    buffer[buffered++] = out[0];
    buffer[buffered++] = out[1];
    buffer[buffered++] = out[2];
    buffer[buffered++] = out[3];
  }
  if (buffered > 0) {
    client.write(reinterpret_cast<const uint8_t*>(buffer), buffered);
  }
}

static bool base64Decode(const String& in, std::vector<uint8_t>& out) {
  size_t outLen = (in.length() * 3) / 4 + 4;
  out.assign(outLen, 0);
  size_t written = 0;
  int rc = mbedtls_base64_decode(out.data(), outLen, &written,
                                 reinterpret_cast<const unsigned char*>(in.c_str()),
                                 in.length());
  if (rc != 0) return false;
  out.resize(written);
  return true;
}

static void respondError(const char* msg) {
  if (msg && msg[0]) {
    LOGW("CTRL", "response=error msg=%s", msg);
  } else {
    LOGW("CTRL", "response=error");
  }
  if (!g_client || !g_client.connected()) return;
  g_client.print("{\"status\":\"error\",\"msg\":\"");
  g_client.print(msg);
  g_client.println("\"}");
}

static void respondOk(const char* msg = nullptr) {
  if (msg && msg[0]) {
    LOGI("CTRL", "response=ok msg=%s", msg);
  } else {
    LOGI("CTRL", "response=ok");
  }
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
  const size_t jsonLen = static_cast<size_t>(json.length());
  const bool hasBase64 = json.indexOf("\"data_base64\"") >= 0;
  const bool isError = json.indexOf("\"status\":\"error\"") >= 0;
  const bool isOk = json.indexOf("\"status\":\"ok\"") >= 0;
  if (hasBase64) {
    String path;
    int size = -1;
    parseStringValue(json, "path", path);
    parseIntValue(json, "size", size);
    if (isError) {
      LOGW("CTRL", "response=error json_len=%u base64=1 path=%s size=%d",
           static_cast<unsigned>(jsonLen), path.c_str(), size);
    } else if (isOk) {
      LOGI("CTRL", "response=ok json_len=%u base64=1 path=%s size=%d",
           static_cast<unsigned>(jsonLen), path.c_str(), size);
    } else {
      LOGI("CTRL", "response=json len=%u base64=1 path=%s size=%d",
           static_cast<unsigned>(jsonLen), path.c_str(), size);
    }
  } else {
    String preview;
    if (jsonLen > kSerialLogMax) {
      preview = json.substring(0, static_cast<unsigned int>(kSerialLogMax));
      preview += "...";
    } else {
      preview = json;
    }
    if (isError) {
      LOGW("CTRL", "response=error json_len=%u preview=%s",
           static_cast<unsigned>(jsonLen), preview.c_str());
    } else if (isOk) {
      LOGI("CTRL", "response=ok json_len=%u preview=%s",
           static_cast<unsigned>(jsonLen), preview.c_str());
    } else {
      LOGI("CTRL", "response=json len=%u preview=%s",
           static_cast<unsigned>(jsonLen), preview.c_str());
    }
  }
  if (!g_client || !g_client.connected()) return;
  g_client.println(json);
}

static void respondFilterStatus() {
  bool licenseOk = licenseAtLeast(LICENSE_TIER_ADV);
  bool effective = g_filterConfig.enabled && licenseOk;
  String resp = "{\"filter\":{";
  resp += "\"enabled\":";
  resp += g_filterConfig.enabled ? "true" : "false";
  resp += ",\"alpha\":";
  resp += String(g_filterConfig.alpha, 3);
  resp += ",\"median\":";
  resp += String(g_filterConfig.medianWindow);
  resp += "},\"license_ok\":";
  resp += licenseOk ? "true" : "false";
  resp += ",\"effective\":";
  resp += effective ? "true" : "false";
  resp += "}";
  respondJson(resp);
  g_client.stop();
}

static void respondCalibrationStatus() {
  String levels;
  calibrationListLevels(levels);
  String resp = "{\"status\":\"ok\",\"enabled\":";
  resp += g_calibrationEnabled ? "true" : "false";
  resp += ",\"mode_active\":";
  resp += g_calibrationModeActive ? "true" : "false";
  resp += ",\"levels\":";
  resp += levels;
  resp += ",\"complete\":";
  resp += calibrationHasCompleteData() ? "true" : "false";
  resp += "}";
  respondJson(resp);
  g_client.stop();
}

static void serviceDiscoveryUdp() {
  int packetSize = g_discoveryUdp.parsePacket();
  while (packetSize > 0) {
    int toRead = packetSize;
    if (toRead > 127) toRead = 127;
    char buf[128] = {0};
    int len = g_discoveryUdp.read(buf, toRead);
    if (len > 0) {
      buf[len] = '\0';
      String msg(buf);
      msg.trim();
      msg.toUpperCase();
      if (msg == kDiscoveryMagic) {
        String resp = "{\"status\":\"ok\",\"ip\":\"";
        resp += WiFi.localIP().toString();
        resp += "\",\"model\":\"";
        resp += kExpectedModel ? kExpectedModel : "";
        resp += "\",\"mac\":\"";
        resp += WiFi.macAddress();
        resp += "\",\"license\":\"";
        resp += licenseTierName(licenseCurrentTier());
        resp += "\",\"port\":";
        resp += kControlPort;
        resp += "}";
        g_discoveryUdp.beginPacket(g_discoveryUdp.remoteIP(), g_discoveryUdp.remotePort());
        g_discoveryUdp.print(resp);
        g_discoveryUdp.endPacket();
      }
    }
    packetSize = g_discoveryUdp.parsePacket();
  }
}

static void handlePayload(const String& payload) {
  String trimmed = payload;
  trimmed.trim();
  if (trimmed.length() == 0) {
    respondError("empty");
    return;
  }
  LOGD("CTRL", "payload=%s", trimmed.c_str());

  String licenseToken;
  bool hasLicense = parseStringValue(trimmed, "license", licenseToken);

  bool analogPresent = trimmed.indexOf("\"analog\"") >= 0;
  bool selectPresent = trimmed.indexOf("\"select\"") >= 0;
  bool filterPresent = trimmed.indexOf("\"filter\"") >= 0;
  bool calibPresent = trimmed.indexOf("\"calibration\"") >= 0;
  bool standbyPresent = trimmed.indexOf("\"standby\"") >= 0;
  bool spiffsPresent = trimmed.indexOf("\"spiffs\"") >= 0;
  bool logPresent = trimmed.indexOf("\"log\"") >= 0;

  if (hasLicense && licenseToken == "?") {
    extern String licenseDescribeAll();
    String desc = licenseDescribeAll();
    respondJson(desc);
    g_client.stop();
    return;
  }

  String filterQuery;
  bool filterQueryOnly = parseStringValue(trimmed, "filter", filterQuery) && filterQuery == "?";
  bool calibQueryOnly = false;
  {
    String calibSectionTmp;
    if (extractSection(trimmed, "calibration", calibSectionTmp)) {
      String cmdTmp;
      if (parseStringValue(calibSectionTmp, "command", cmdTmp) && cmdTmp == "?") {
        calibQueryOnly = true;
      }
    }
  }
  bool standbyQueryOnly = false;
  {
    String standbySectionTmp;
    if (extractSection(trimmed, "standby", standbySectionTmp)) {
      String cmdTmp;
      if (parseStringValue(standbySectionTmp, "command", cmdTmp) && cmdTmp == "?") {
        standbyQueryOnly = true;
      }
    }
  }

  if (!analogPresent && !selectPresent && !filterPresent && hasLicense) {
    bool licOk = licenseApplyToken(licenseToken, rtc.getEpoch());
    if (licOk) {
      respondOk("license_ok");
      LOGI("LICENSE", "updated source=tcp");
      ledShowColor(LedPalette::ReadyIdle);
      delay(500);
      if (!licenseWarningActive()) {
        ledShowColor(LedPalette::StreamActive);
      }
      setLicenseWarning(false);
    } else {
      const char* reason = licenseLastError();
      respondError((reason && reason[0]) ? reason : "license_invalid");
      setLicenseWarning(true);
    }
    g_client.stop();
    return;
  }

  if ((analogPresent && !selectPresent) || (!analogPresent && selectPresent)) {
    respondError("analog/select_required");
    g_client.stop();
    return;
  }

  if (spiffsPresent && !g_calibrationModeActive) {
    respondError("standby_required");
    g_client.stop();
    return;
  }

  if (logPresent && !g_calibrationModeActive) {
    respondError("standby_required");
    g_client.stop();
    return;
  }

  if (spiffsPresent) {
    String spiffsJson;
    if (!extractSection(trimmed, "spiffs", spiffsJson)) {
      respondError("spiffs_parse_error");
      g_client.stop();
      return;
    }
    String cmd;
    if (!parseStringValue(spiffsJson, "command", cmd)) {
      respondError("spiffs_command_required");
      g_client.stop();
      return;
    }
    cmd.toLowerCase();

    if (cmd == "list") {
      File root = SPIFFS.open("/");
      if (!root || !root.isDirectory()) {
        respondError("spiffs_unavailable");
        g_client.stop();
        return;
      }
      String resp = "{\"status\":\"ok\",\"files\":[";
      bool first = true;
      File f = root.openNextFile();
      while (f) {
        if (!first) resp += ",";
        resp += "{\"name\":\"";
        resp += f.name();
        resp += "\",\"size\":";
        resp += String(f.size());
        resp += "}";
        first = false;
        f = root.openNextFile();
      }
      resp += "]}";
      respondJson(resp);
      g_client.stop();
      return;
    }

    if (cmd == "read") {
      String path;
      if (!parseStringValue(spiffsJson, "path", path)) {
        respondError("spiffs_path_required");
        g_client.stop();
        return;
      }
      path.trim();
      if (!path.startsWith("/")) path = "/" + path;
      int limit = 4096;
      int tmpLimit = 0;
      if (parseIntValue(spiffsJson, "limit", tmpLimit) && tmpLimit > 0) {
        limit = tmpLimit;
      }
      File f = SPIFFS.open(path, "r");
      if (!f) {
        respondError("spiffs_open_failed");
        g_client.stop();
        return;
      }
      size_t toRead = f.size();
      if (limit > 0 && toRead > static_cast<size_t>(limit)) {
        toRead = static_cast<size_t>(limit);
      }
      std::vector<uint8_t> buf(toRead);
      size_t readBytes = f.read(buf.data(), toRead);
      f.close();
      String b64;
      if (!base64Encode(buf.data(), readBytes, b64)) {
        respondError("spiffs_base64_encode_failed");
        g_client.stop();
        return;
      }
      String resp = "{\"status\":\"ok\",\"path\":\"";
      resp += path;
      resp += "\",\"size\":";
      resp += String(readBytes);
      resp += ",\"data_base64\":\"";
      resp += b64;
      resp += "\"}";
      respondJson(resp);
      g_client.stop();
      return;
    }

    if (cmd == "write") {
      String path;
      String dataB64;
      if (!parseStringValue(spiffsJson, "path", path) ||
          !parseStringValue(spiffsJson, "data_base64", dataB64)) {
        respondError("spiffs_path_data_required");
        g_client.stop();
        return;
      }
      path.trim();
      if (!path.startsWith("/")) path = "/" + path;
      std::vector<uint8_t> decoded;
      if (!base64Decode(dataB64, decoded)) {
        respondError("spiffs_base64_decode_failed");
        g_client.stop();
        return;
      }
      File f = SPIFFS.open(path, "w");
      if (!f) {
        respondError("spiffs_open_failed");
        g_client.stop();
        return;
      }
      size_t written = f.write(decoded.data(), decoded.size());
      f.close();
      if (written != decoded.size()) {
        respondError("spiffs_write_incomplete");
        g_client.stop();
        return;
      }
      String resp = "{\"status\":\"ok\",\"path\":\"";
      resp += path;
      resp += "\",\"written\":";
      resp += String(written);
      resp += "}";
      respondJson(resp);
      g_client.stop();
      return;
    }

    if (cmd == "delete") {
      String path;
      if (!parseStringValue(spiffsJson, "path", path)) {
        respondError("spiffs_path_required");
        g_client.stop();
        return;
      }
      path.trim();
      if (!path.startsWith("/")) path = "/" + path;
      if (!SPIFFS.exists(path)) {
        respondError("spiffs_not_found");
        g_client.stop();
        return;
      }
      if (!SPIFFS.remove(path)) {
        respondError("spiffs_delete_failed");
        g_client.stop();
        return;
      }
      String resp = "{\"status\":\"ok\",\"path\":\"";
      resp += path;
      resp += "\",\"deleted\":true}";
      respondJson(resp);
      g_client.stop();
      return;
    }

    respondError("spiffs_unknown_command");
    g_client.stop();
    return;
  }

  if (logPresent) {
    if (analogPresent || selectPresent || filterPresent || calibPresent || standbyPresent || spiffsPresent) {
      respondError("log_conflict");
      g_client.stop();
      return;
    }
    String logSection;
    if (!extractSection(trimmed, "log", logSection)) {
      respondError("log_parse_error");
      g_client.stop();
      return;
    }
    String cmd;
    if (!parseStringValue(logSection, "command", cmd)) {
      respondError("log_command_required");
      g_client.stop();
      return;
    }
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "?" || cmd == "status") {
      respondJson(logStatusJson());
      g_client.stop();
      return;
    }

    if (cmd == "enable") {
      String levelStr;
      if (!parseStringValue(logSection, "level", levelStr)) {
        respondError("log_level_required");
        g_client.stop();
        return;
      }
      LogLevel level;
      if (!parseLogLevel(levelStr, level)) {
        respondError("log_level_invalid");
        g_client.stop();
        return;
      }
      if (!logSetEnabled(true, level)) {
        respondError("log_enable_failed");
        g_client.stop();
        return;
      }
      respondOk("log_enabled_reboot");
      g_client.stop();
      delay(200);
      ESP.restart();
      return;
    }

    if (cmd == "read") {
      std::vector<uint8_t> ordered;
      uint32_t offset = 0;
      if (!logReadOrderedBytes(ordered, offset)) {
        respondError("log_read_failed");
        g_client.stop();
        return;
      }
      size_t rawSize = ordered.size();
      if (g_client && g_client.connected()) {
        LOGI("CTRL", "response=ok base64=1 path=/log.txt size=%u offset=%u",
             static_cast<unsigned>(rawSize), static_cast<unsigned>(offset));
        g_client.print("{\"status\":\"ok\",\"path\":\"/log.txt\",\"size\":");
        g_client.print(rawSize);
        g_client.print(",\"offset\":");
        g_client.print(offset);
        g_client.print(",\"ordered\":true,\"data_base64\":\"");
        if (rawSize > 0) {
          writeBase64ToClient(g_client, ordered.data(), rawSize);
        }
        g_client.println("\"}");
      }
      g_client.stop();
      return;
    }

    if (cmd == "disable") {
      logSetEnabled(false, LogLevel::Info);
      respondOk("log_disabled");
      g_client.stop();
      return;
    }

    respondError("log_unknown_cmd");
    g_client.stop();
    return;
  }

  if (filterQueryOnly) {
    if (!g_calibrationModeActive) {
      respondError("standby_required");
      return;
    }
    respondFilterStatus();
    return;
  }

  if (calibQueryOnly) {
    if (!g_calibrationModeActive) {
      respondError("standby_required");
      return;
    }
    respondCalibrationStatus();
    return;
  }

  if (standbyQueryOnly) {
    String resp = "{\"status\":\"ok\",\"mode_active\":";
    resp += g_calibrationModeActive ? "true" : "false";
    resp += "}";
    respondJson(resp);
    return;
  }

  if (filterPresent && !g_calibrationModeActive) {
    respondError("standby_required");
    g_client.stop();
    return;
  }

  if (g_calibrationModeActive && !calibPresent && !spiffsPresent && !standbyPresent && !filterPresent && !logPresent) {
    respondError("standby_active");
    g_client.stop();
    return;
  }

  // Handle standby command set
  if (standbyPresent && !analogPresent && !selectPresent && !filterPresent && !calibPresent && !logPresent) {
    String standbySection;
    if (!extractSection(trimmed, "standby", standbySection)) {
      respondError("standby_parse_error");
      g_client.stop();
      return;
    }
    String cmd;
    if (!parseStringValue(standbySection, "command", cmd)) {
      respondError("standby_command_required");
      g_client.stop();
      return;
    }
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "enable") {
      g_calibrationModeActive = true;
      ledShowColor(LedPalette::CalibPrep);
      respondOk("standby_mode");
      g_client.stop();
      return;
    }

    if (cmd == "disable") {
      if (!calibrationSaveToSpiffs()) {
        respondError("calibration_save_failed");
      } else {
        g_calibrationModeActive = false;
        bool keepEnabled = g_calibrationEnabled;
        if (keepEnabled && !calibrationHasCompleteData()) {
          keepEnabled = false;
        }
        g_calibrationEnabled = keepEnabled;
        respondOk("standby_exit");
        ledShowColor(LedPalette::ReadyIdle);
        delay(500);
        if (!licenseWarningActive()) {
          ledShowColor(LedPalette::StreamActive);
        }
      }
      g_client.stop();
      return;
    }

    respondError("standby_unknown_cmd");
    g_client.stop();
    return;
  }

  // Handle calibration command set
  if (calibPresent && !analogPresent && !selectPresent && !filterPresent && !standbyPresent && !logPresent) {
    String calibSection;
    if (!extractSection(trimmed, "calibration", calibSection)) {
      respondError("calibration_parse_error");
      g_client.stop();
      return;
    }
    String cmd;
    if (!parseStringValue(calibSection, "command", cmd)) {
      respondError("calibration_command_required");
      g_client.stop();
      return;
    }
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "start") cmd = "standby_on";
    if (cmd == "end") cmd = "standby_off";
    bool isStandbyToggle = (cmd == "standby_on" || cmd == "standby_off");

    if (cmd == "standby_on") {
      if (!licenseAtLeast(LICENSE_TIER_PRO)) {
        respondError("license_tier_insufficient");
        g_client.stop();
        return;
      }
      g_calibrationModeActive = true;
      ledShowColor(LedPalette::CalibPrep);
      respondOk("standby_mode");
      g_client.stop();
      return;
    }

    if (cmd == "standby_off") {
      if (!calibrationSaveToSpiffs()) {
        respondError("calibration_save_failed");
      } else {
        g_calibrationModeActive = false;
        g_calibrationEnabled = calibrationHasCompleteData();
        respondOk("standby_exit");
        // After calibration finishes, restore normal LED state
        ledShowColor(LedPalette::ReadyIdle);
        delay(500);
        if (!licenseWarningActive()) {
          ledShowColor(LedPalette::StreamActive);
        }
      }
      g_client.stop();
      return;
    }

    if (!isStandbyToggle && !g_calibrationModeActive) {
      respondError("standby_required");
      g_client.stop();
      return;
    }

    if (cmd == "enabled") {
      if (!licenseAtLeast(LICENSE_TIER_PRO)) {
        respondError("license_tier_insufficient");
        g_client.stop();
        return;
      }
      if (!calibrationLoadFromSpiffs() || !calibrationHasCompleteData()) {
        respondError("calibration_incomplete");
      } else {
        g_calibrationEnabled = true;
        respondOk("calibration_enabled");
      }
      g_client.stop();
      return;
    }

    if (cmd == "disabled") {
      g_calibrationEnabled = false;
      respondOk("calibration_disabled");
      g_client.stop();
      return;
    }

    if (cmd == "level") {
      String lvlStr;
      if (!parseStringValue(calibSection, "level", lvlStr)) {
        respondError("level_required");
        g_client.stop();
        return;
      }
      float level = lvlStr.toFloat();
      String resp;
      if (!calibrationDumpLevel(level, resp)) {
        // If not found in memory, try to reload from SPIFFS
        if (calibrationLoadFromSpiffs() && calibrationDumpLevel(level, resp)) {
          respondJson(resp);
        } else {
          respondError("level_not_found");
        }
        g_client.stop();
        return;
      }
      respondJson(resp);
      g_client.stop();
      return;
    }

    if (cmd == "delete") {
      String lvlStr;
      if (!parseStringValue(calibSection, "level", lvlStr)) {
        respondError("level_required");
        g_client.stop();
        return;
      }
      float level = lvlStr.toFloat();
      if (calibrationRemoveLevel(level)) {
        if (!calibrationSaveToSpiffs()) {
          respondError("calibration_save_failed");
        } else {
          respondOk("level_deleted");
        }
      } else {
        respondError("level_not_found");
      }
      g_client.stop();
      return;
    }

    if (cmd == "calibrate_all") {
      if (!licenseAtLeast(LICENSE_TIER_PRO)) {
        respondError("license_tier_insufficient");
        g_client.stop();
        return;
      }
      float level = 0.0f;
      int startDelay = 0;
      int calibTime = 5000;
      if (!parseFloatValue(calibSection, "level", level)) {
        respondError("level_required");
        g_client.stop();
        return;
      }
      parseIntValue(calibSection, "start_time", startDelay);
      parseIntValue(calibSection, "calibration_time", calibTime);
      if (startDelay < 0) startDelay = 0;
      if (calibTime <= 0) calibTime = 5000;
      ledShowColor(LedPalette::CalibPrep);
      if (startDelay > 0) delay(startDelay);
      ledShowColor(LedPalette::CalibSampling);
      configureMatrixPinsForScan();
      unsigned long endMs = millis() + calibTime;
      std::vector<double> sums(sensors_num, 0.0);
      std::vector<uint32_t> counts(sensors_num, 0);
      while (static_cast<long>(millis() - endMs) < 0) {
        for (size_t r = 0; r < sensors_rows_num; ++r) {
          const uint8_t anaPin = analogReadIO[r];
          for (size_t c = 0; c < sensors_columns_num; ++c) {
            const uint8_t selPin = SelectIO[c];
            pinMode(selPin, OUTPUT_OPEN_DRAIN);
            digitalWrite(selPin, LOW);
            uint16_t mv = analogReadMilliVolts(anaPin);
            digitalWrite(selPin, HIGH);
            size_t idx = r * sensors_columns_num + c;
            sums[idx] += mv;
            ++counts[idx];
          }
        }
        delay(1);
      }
      uint32_t minSamples = counts.empty() ? 0 : counts[0];
      uint32_t maxSamples = counts.empty() ? 0 : counts[0];
      for (size_t i = 0; i < counts.size(); ++i) {
        if (counts[i] == 0) {
          respondError("calibration_no_samples");
          ledShowColor(LedPalette::CalibError);
          g_client.stop();
          return;
        }
        if (counts[i] < minSamples) minSamples = counts[i];
        if (counts[i] > maxSamples) maxSamples = counts[i];
      }
      for (size_t r = 0; r < sensors_rows_num; ++r) {
        const uint8_t anaPin = analogReadIO[r];
        for (size_t c = 0; c < sensors_columns_num; ++c) {
          size_t idx = r * sensors_columns_num + c;
          float avg = static_cast<float>(sums[idx] / static_cast<double>(counts[idx]));
          if (!calibrationSetPoint(level, anaPin, SelectIO[c], avg)) {
            respondError("calibration_store_failed");
            ledShowColor(LedPalette::CalibError);
            g_client.stop();
            return;
          }
        }
      }
      configureMatrixPinsForScan();
      if (!calibrationSaveToSpiffs()) {
        respondError("calibration_save_failed");
      } else {
        String resp = "{\"status\":\"ok\",\"level\":";
        resp += String(level, 3);
        resp += ",\"samples_min\":";
        resp += String(minSamples);
        resp += ",\"samples_max\":";
        resp += String(maxSamples);
        resp += "}";
        respondJson(resp);
      }
      ledShowColor(LedPalette::CalibPrep);
      g_client.stop();
      return;
    }

    if (cmd == "calibration") {
      if (!g_calibrationModeActive) {
        respondError("standby_required");
        g_client.stop();
        return;
      }
      if (!licenseAtLeast(LICENSE_TIER_PRO)) {
        respondError("license_tier_insufficient");
        g_client.stop();
        return;
      }
      int analogPin = -1;
      int selectPin = -1;
      float level = 0.0f;
      int startDelay = 0;
      int calibTime = 5000;
      bool okParams =
        parseIntValue(calibSection, "analogpin", analogPin) &&
        parseIntValue(calibSection, "selectpin", selectPin) &&
        parseFloatValue(calibSection, "level", level) &&
        parseIntValue(calibSection, "start_time", startDelay) &&
        parseIntValue(calibSection, "calibration_time", calibTime);
      if (!okParams) {
        respondError("calibration_params_missing");
        g_client.stop();
        return;
      }
      size_t rowIdx = SIZE_MAX;
      size_t colIdx = SIZE_MAX;
      for (size_t r = 0; r < sensors_rows_num; ++r) {
        if (analogReadIO[r] == analogPin) { rowIdx = r; break; }
      }
      for (size_t c = 0; c < sensors_columns_num; ++c) {
        if (SelectIO[c] == selectPin) { colIdx = c; break; }
      }
      if (rowIdx == SIZE_MAX || colIdx == SIZE_MAX) {
        respondError("calibration_pin_invalid");
        g_client.stop();
        return;
      }
      ledShowColor(LedPalette::CalibPrep);
      if (startDelay > 0) delay(startDelay);
      ledShowColor(LedPalette::CalibSampling);
      configureMatrixPinsForScan();
      unsigned long endMs = millis() + (calibTime > 0 ? calibTime : 5000);
      double sum = 0.0;
      uint32_t count = 0;
      uint8_t selPin = static_cast<uint8_t>(selectPin);
      uint8_t anaPin = static_cast<uint8_t>(analogPin);
      while (static_cast<long>(millis() - endMs) < 0) {
        pinMode(selPin, OUTPUT_OPEN_DRAIN);
        digitalWrite(selPin, LOW);
        uint16_t mv = analogReadMilliVolts(anaPin);
        digitalWrite(selPin, HIGH);
        sum += mv;
        ++count;
        delay(1);
      }
      if (count == 0) {
        respondError("calibration_no_samples");
        ledShowColor(LedPalette::CalibError);
        g_client.stop();
        return;
      }
      float avg = static_cast<float>(sum / static_cast<double>(count));
      if (!calibrationSetPoint(level, anaPin, selPin, avg)) {
        respondError("calibration_store_failed");
        ledShowColor(LedPalette::CalibError);
        g_client.stop();
        return;
      }
      // Restore matrix pins for scan mode (open-drain/input) to avoid bad readings
      configureMatrixPinsForScan();
      if (!calibrationSaveToSpiffs()) {
        respondError("calibration_save_failed");
      } else {
        String resp = "{\"status\":\"ok\",\"avg\":";
        resp += String(avg, 3);
        resp += "}";
        respondJson(resp);
      }
      ledShowColor(LedPalette::CalibPrep);
      g_client.stop();
      return;
    }

    respondError("calibration_unknown_cmd");
    g_client.stop();
    return;
  }

  // Filter config apply
  FilterConfig newFilter = g_filterConfig;
  bool filterTouched = false;
  if (filterPresent) {
    String filterJson;
    if (!extractSection(trimmed, "filter", filterJson)) {
      respondError("filter_parse_error");
      return;
    }
    bool tmpBool;
    float tmpFloat;
    int tmpInt;
    if (parseBoolValue(filterJson, "enabled", tmpBool)) {
      newFilter.enabled = tmpBool;
      filterTouched = true;
    }
    if (parseFloatValue(filterJson, "alpha", tmpFloat)) {
      newFilter.alpha = tmpFloat;
      filterTouched = true;
    }
    if (parseIntValue(filterJson, "median", tmpInt)) {
      newFilter.medianWindow = static_cast<uint8_t>(tmpInt);
      filterTouched = true;
    }
    if (!filterTouched) {
      respondError("filter_fields_required");
      g_client.stop();
      return;
    }
    if (newFilter.alpha < kFilterAlphaMin || newFilter.alpha > kFilterAlphaMax) {
      respondError("alpha_out_of_range");
      g_client.stop();
      return;
    }
    if (newFilter.medianWindow != 1 && newFilter.medianWindow != 3 && newFilter.medianWindow != 5) {
      respondError("median_invalid");
      g_client.stop();
      return;
    }
  }

  if (hasLicense && licenseToken != "?") {
    bool licOk = licenseApplyToken(licenseToken, rtc.getEpoch());
    if (!licOk) {
      const char* reason = licenseLastError();
      respondError((reason && reason[0]) ? reason : "license_invalid");
      setLicenseWarning(true);
      return;
    }
    setLicenseWarning(false);
  }

  if (analogPresent && selectPresent) {
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
      g_client.stop();
      return;
    }
    sensorConfigSaveJson(trimmed);
    LOGI("SENSOR", "io_config updated source=tcp");
  }

  if (filterPresent) {
    if (!applyFilterConfig(newFilter, true, false)) {
      respondError("filter_apply_failed");
      g_client.stop();
      return;
    }
  }

  if (analogPresent) {
    respondOk(filterPresent ? "filter_updated" : nullptr);
    ledShowColor(LedPalette::ReadyIdle);
    delay(500);
    if (!licenseWarningActive()) {
      ledShowColor(LedPalette::StreamActive);
    }
    working_flag = true;
    g_client.stop();
    return;
  }

  if (filterPresent) {
    respondOk("filter_updated");
    ledShowColor(LedPalette::ReadyIdle);
    delay(500);
    if (!licenseWarningActive()) {
      ledShowColor(LedPalette::StreamActive);
    }
    working_flag = true;
    g_client.stop();
    return;
  }

  respondError("no_supported_fields");
  g_client.stop();
}

void handleControlServer() {
  if (!g_started) return;

  serviceDiscoveryUdp();

  if (!g_client || !g_client.connected()) {
    if (g_client) {
      g_client.stop();
    }
    WiFiClient nextClient = g_server.available();
    if (nextClient) {
      g_client = nextClient;
      g_buffer = "";
      LOGI("CTRL", "client_connected");
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
