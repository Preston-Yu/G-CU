#include "GCU.h"

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "mbedtls/sha256.h"

namespace {

const char* kManifestUrl = "https://preston-yu.github.io/G-CU/manifest.json";
const uint32_t kHttpTimeoutMs = 8000;
const uint32_t kTlsHandshakeTimeoutMs = 12000;
const size_t kDownloadChunkSize = 4096;
const size_t kLedProgressStepBytes = 16384;
const uint8_t kManifestMaxAttempts = 3;
const uint16_t kManifestRetryDelayMs = 1500;

struct ManifestInfo {
  String model;
  String latest;
  String url;
  String sha256;
};

uint8_t g_otaChunkBuffer[kDownloadChunkSize];

/**
 * @brief Linearly interpolate between two LED colors.
 * @param from Starting color.
 * @param to Ending color.
 * @param ratio Blend ratio 0.0-1.0 (clamped internally).
 */
LedColor mixColors(const LedColor& from, const LedColor& to, float ratio) {
  float clamped = ratio;
  if (clamped < 0.0f) clamped = 0.0f;
  if (clamped > 1.0f) clamped = 1.0f;
  LedColor mixed{
    static_cast<uint8_t>(from.r + (to.r - from.r) * clamped),
    static_cast<uint8_t>(from.g + (to.g - from.g) * clamped),
    static_cast<uint8_t>(from.b + (to.b - from.b) * clamped)
  };
  return mixed;
}

/**
 * @brief Helper to set the OTA status LED to a fixed color.
 */
void otaLedSet(const LedColor& color) {
  ledShowColor(color);
}

/**
 * @brief Turn off the OTA status LED (idle state).
 */
void otaLedIdle() {
  otaLedSet(LedPalette::Off);
}

/**
 * @brief Indicate manifest download activity (blue).
 */
void otaLedManifest() {
  otaLedSet(LedPalette::OtaManifest);
}

/**
 * @brief Indicate OTA preparation state (purple).
 */
void otaLedReady() {
  otaLedSet(LedPalette::OtaPreparing);
}

/**
 * @brief Indicate OTA failure (red).
 */
void otaLedError() {
  otaLedSet(LedPalette::OtaError);
}

/**
 * @brief Indicate OTA success (cyan).
 */
void otaLedSuccess() {
  otaLedSet(LedPalette::OtaSuccess);
}

/**
 * @brief Map OTA download progress to a gradient on the RGB LED.
 * @param written Bytes already written to flash.
 * @param totalLength Total payload length (or <=0 when unknown).
 */
void otaLedShowProgress(size_t written, int totalLength) {
  const LedColor from = LedPalette::OtaPreparing;
  const LedColor to = LedPalette::OtaSuccess;
  if (totalLength <= 0) {
    uint8_t pulse = (written / 1024) % (LED_LEVEL_HIGH + 1);
    float pseudoRatio = static_cast<float>(pulse) / static_cast<float>(LED_LEVEL_HIGH);
    otaLedSet(mixColors(from, to, pseudoRatio));
    return;
  }
  float ratio = static_cast<float>(written) / static_cast<float>(totalLength);
  otaLedSet(mixColors(from, to, ratio));
}

/**
 * @brief Fetch the manifest JSON from the remote server with retries.
 * @param payload Output buffer for the downloaded manifest.
 * @return True when HTTP GET succeeds.
 */
bool fetchManifestPayload(String& payload) {
  for (uint8_t attempt = 1; attempt <= kManifestMaxAttempts; ++attempt) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout((kHttpTimeoutMs + 999) / 1000);
    client.setHandshakeTimeout(kTlsHandshakeTimeoutMs);
    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);
    http.setConnectTimeout(kHttpTimeoutMs);
    Serial.printf("Fetching manifest (attempt %u/%u): %s\n",
                  attempt, kManifestMaxAttempts, kManifestUrl);
    if (!http.begin(client, kManifestUrl)) {
      Serial.println("Failed to init HTTP client for manifest");
    } else {
      int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK) {
        payload = http.getString();
        http.end();
        return true;
      }
      Serial.printf("Manifest HTTP error: %d\n", httpCode);
      http.end();
    }
    if (attempt < kManifestMaxAttempts) {
      uint32_t backoff = kManifestRetryDelayMs * attempt * attempt;
      Serial.printf("Retrying manifest in %u ms...\n", backoff);
      delay(backoff);
      WiFi.reconnect();
    }
  }
  return false;
}

/**
 * @brief Return a lowercase copy of the provided string.
 */
String toLowerCopy(String value) {
  value.toLowerCase();
  return value;
}

/**
 * @brief Locate the next token in the manifest JSON string.
 * @param json Manifest payload.
 * @param start Offset to start searching from.
 * @param token Token string to match.
 */
int findNext(const String& json, int start, const char* token) {
  int idx = json.indexOf(token, start);
  return idx;
}

/**
 * @brief Extract a string field value within a manifest object.
 * @param json Manifest payload.
 * @param objStart Offset where the object begins.
 * @param key Field name to read.
 * @param out Filled with the field value on success.
 */
bool extractField(const String& json, int objStart, const char* key, String& out) {
  String keyPattern = "\"" + String(key) + "\"";
  int keyIdx = json.indexOf(keyPattern, objStart);
  if (keyIdx < 0) {
    return false;
  }
  int colonIdx = json.indexOf(':', keyIdx + keyPattern.length());
  if (colonIdx < 0) {
    return false;
  }
  int firstQuote = json.indexOf('"', colonIdx);
  if (firstQuote < 0) {
    return false;
  }
  int secondQuote = json.indexOf('"', firstQuote + 1);
  if (secondQuote < 0) {
    return false;
  }
  out = json.substring(firstQuote + 1, secondQuote);
  out.trim();
  return true;
}

/**
 * @brief Search manifest for the entry matching the requested model string.
 * @param json Manifest payload.
 * @param targetModel Model string to match (case-insensitive).
 * @param manifest Output info structure when found.
 */
bool findManifestForModel(const String& json, const char* targetModel, ManifestInfo& manifest) {
  String devicesKey = "\"devices\"";
  int devicesIdx = json.indexOf(devicesKey);
  if (devicesIdx < 0) {
    Serial.println("Manifest missing devices array");
    return false;
  }
  int arrayStart = json.indexOf('[', devicesIdx);
  int arrayEnd = json.indexOf(']', arrayStart);
  if (arrayStart < 0 || arrayEnd < 0) {
    Serial.println("Manifest devices array malformed");
    return false;
  }
  int cursor = arrayStart;
  while (cursor < arrayEnd) {
    int objStart = json.indexOf('{', cursor);
    if (objStart < 0 || objStart >= arrayEnd) {
      break;
    }
    int objEnd = json.indexOf('}', objStart);
    if (objEnd < 0) {
      break;
    }

    String model;
    if (!extractField(json, objStart, "model", model)) {
      cursor = objEnd + 1;
      continue;
    }
    if (!model.equalsIgnoreCase(targetModel)) {
      cursor = objEnd + 1;
      continue;
    }

    manifest.model = model;
    if (!extractField(json, objStart, "latest", manifest.latest)) {
      Serial.println("Manifest entry missing latest");
      return false;
    }
    if (!extractField(json, objStart, "url", manifest.url)) {
      Serial.println("Manifest entry missing url");
      return false;
    }
    if (!extractField(json, objStart, "sha256", manifest.sha256)) {
      Serial.println("Manifest entry missing sha256");
      return false;
    }
    return true;
  }
  Serial.println("No manifest entry found for this model");
  return false;
}

/**
 * @brief Split dotted version into integer array by reference.
 * @param version Version string.
 * @param parts Output array reference (size 3).
 */
void splitVersion(const String& version, int (&parts)[3]) {
  parts[0] = parts[1] = parts[2] = 0;
  int partIdx = 0;
  int start = 0;
  for (int i = 0; i < version.length() && partIdx < 3; i++) {
    if (version[i] == '.') {
      parts[partIdx++] = version.substring(start, i).toInt();
      start = i + 1;
    }
  }
  if (partIdx < 3) {
    parts[partIdx] = version.substring(start).toInt();
  }
}

/**
 * @brief Lexicographically compare two semantic versions (remote vs local).
 */
int compareVersions(const String& remote, const char* local) {
  int remoteParts[3];
  int localParts[3];
  splitVersion(remote, remoteParts);
  splitVersion(String(local), localParts);
  for (int i = 0; i < 3; i++) {
    if (remoteParts[i] > localParts[i]) {
      return 1;
    }
    if (remoteParts[i] < localParts[i]) {
      return -1;
    }
  }
  return 0;
}

/**
 * @brief Download the manifest and populate metadata for the expected model.
 * @param manifest Output descriptor with model/version/url/hash.
 */
bool downloadManifest(ManifestInfo& manifest) {
  otaLedManifest();
  String payload;
  if (!fetchManifestPayload(payload)) {
    otaLedError();
    return false;
  }
  if (!findManifestForModel(payload, kExpectedModel, manifest)) {
    otaLedError();
    return false;
  }
  otaLedIdle();
  return true;
}

/**
 * @brief Convert raw bytes to a lowercase hexadecimal string.
 */
String bytesToHex(const uint8_t* data, size_t len) {
  static const char hex[] = "0123456789abcdef";
  String out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; i++) {
    out += hex[(data[i] >> 4) & 0x0F];
    out += hex[data[i] & 0x0F];
  }
  return out;
}

/**
 * @brief Check whether two SHA256 digests match (case-insensitive).
 */
bool shaMatches(const String& expected, const String& actual) {
  return toLowerCopy(expected) == toLowerCopy(actual);
}

/**
 * @brief Download firmware binary, verify SHA256, and flash OTA.
 * @param manifest Manifest entry with download URL and SHA.
 */
bool downloadAndFlash(const ManifestInfo& manifest) {
  if (manifest.url.isEmpty()) {
    Serial.println("Manifest firmware URL empty");
    otaLedError();
    return false;
  }
  otaLedReady();
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  Serial.printf("Downloading firmware: %s\n", manifest.url.c_str());
  if (!http.begin(client, manifest.url)) {
    Serial.println("Failed to init HTTP client for firmware");
    otaLedError();
    return false;
  }
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Firmware HTTP error: %d\n", httpCode);
    http.end();
    otaLedError();
    return false;
  }
  int totalLength = http.getSize();
  if (totalLength > 0) {
    Serial.printf("Firmware size: %d bytes\n", totalLength);
  } else {
    Serial.println("Firmware size unknown");
  }
  if (!Update.begin(totalLength > 0 ? totalLength : UPDATE_SIZE_UNKNOWN)) {
    Update.printError(Serial);
    http.end();
    otaLedError();
    return false;
  }

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);

  WiFiClient* stream = http.getStreamPtr();
  uint8_t* buffer = g_otaChunkBuffer;
  size_t written = 0;
  unsigned long lastLog = millis();
  size_t nextLedUpdate = 0;
  otaLedShowProgress(0, totalLength);

  while (http.connected()) {
    size_t available = stream->available();
    if (!available) {
      if (totalLength > 0 && written >= static_cast<size_t>(totalLength)) {
        break;
      }
      delay(0);
      continue;
    }
    if (available > kDownloadChunkSize) {
      available = kDownloadChunkSize;
    }
    int readBytes = stream->readBytes(buffer, available);
    if (readBytes <= 0) {
      delay(0);
      continue;
    }
    if (Update.write(buffer, readBytes) != readBytes) {
      Update.printError(Serial);
      mbedtls_sha256_free(&ctx);
      http.end();
      otaLedError();
      return false;
    }
    mbedtls_sha256_update(&ctx, buffer, readBytes);
    written += readBytes;
    if (written >= nextLedUpdate) {
      otaLedShowProgress(written, totalLength);
      nextLedUpdate = written + kLedProgressStepBytes;
    }
    if (millis() - lastLog > 1000) {
      Serial.printf("OTA progress: %u bytes\n", static_cast<unsigned int>(written));
      lastLog = millis();
    }
  }

  if (totalLength > 0 && written != static_cast<size_t>(totalLength)) {
    Serial.printf("Firmware length mismatch. Expected %d got %u\n", totalLength, static_cast<unsigned int>(written));
    mbedtls_sha256_free(&ctx);
    http.end();
    otaLedError();
    return false;
  }
  otaLedShowProgress(written, totalLength);

  uint8_t digest[32];
  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  http.end();

  String actualSha = bytesToHex(digest, sizeof(digest));
  if (!shaMatches(manifest.sha256, actualSha)) {
    Serial.println("SHA256 mismatch, aborting update");
    Serial.print("Expected: ");
    Serial.println(manifest.sha256);
    Serial.print("Actual  : ");
    Serial.println(actualSha);
    otaLedError();
    return false;
  }

  if (!Update.end(true)) {
    Update.printError(Serial);
    otaLedError();
    return false;
  }
  if (!Update.isFinished()) {
    Serial.println("Update did not finish, aborting");
    otaLedError();
    return false;
  }

  Serial.println("Firmware updated successfully, rebooting...");
  otaLedSuccess();
  delay(100);
  ESP.restart();
  return true;
}

}  // namespace

/**
 * @brief Check remote manifest and perform OTA if the version is newer.
 * @param currentVersion Current firmware semantic version string.
 * @return True when an update was applied (device will reboot).
 */
bool runOnlineOtaUpdateIfAvailable(const char* currentVersion) {
  if (!g_wifiGotIP) {
    Serial.println("WiFi not ready, skip manifest OTA");
    otaLedIdle();
    return false;
  }
  if (!currentVersion) {
    currentVersion = "";
  }

  ManifestInfo manifest;
  if (!downloadManifest(manifest)) {
    return false;
  }

  int cmp = compareVersions(manifest.latest, currentVersion);
  if (cmp <= 0) {
    Serial.printf("No update required. Current: %s Remote: %s\n", currentVersion, manifest.latest.c_str());
    otaLedIdle();
    return false;
  }

  Serial.printf("New firmware available: %s -> %s\n", currentVersion, manifest.latest.c_str());
  return downloadAndFlash(manifest);
}
