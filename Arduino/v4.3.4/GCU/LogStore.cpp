#include "GCU.h"

#include <Preferences.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <mbedtls/base64.h>
#include <vector>

namespace {
constexpr char kLogPrefsNamespace[] = "logcfg";
constexpr char kLogKeyEnabled[] = "enabled";
constexpr char kLogKeyLevel[] = "level";
constexpr char kLogKeyOffset[] = "offset";
constexpr char kLogKeyStartEpoch[] = "start";
constexpr char kLogKeyClear[] = "clear";

constexpr char kLogFilePath[] = "/log.txt";
constexpr size_t kLogFileMaxBytes = GCU_LOG_FILE_MAX_BYTES;
constexpr uint32_t kLogMaxDurationSec = 24 * 60 * 60;
constexpr uint32_t kLogMaxDurationMs = 24UL * 60UL * 60UL * 1000UL;
constexpr size_t kLogLineMax = 256;
constexpr size_t kLogLineBufMax = 320;
constexpr size_t kPersistOffsetStep = 1024;
constexpr uint32_t kEpochValidMin = 1000000000UL;

Preferences s_logPrefs;
bool s_logEnabled = false;
LogLevel s_logLevel = LogLevel::Info;
uint32_t s_logStartEpoch = 0;
uint32_t s_logStartMs = 0;
uint32_t s_logOffset = 0;
size_t s_logBytesSincePersist = 0;
bool s_logFileReady = false;
bool s_clearRequested = false;
bool s_logPaused = false;

LogLevel clampLevel(uint8_t level) {
  if (level > static_cast<uint8_t>(LogLevel::Error)) {
    return LogLevel::Info;
  }
  return static_cast<LogLevel>(level);
}

bool logAllows(LogLevel lvl) {
  return static_cast<uint8_t>(lvl) >= static_cast<uint8_t>(s_logLevel);
}

bool epochValid(uint32_t epoch) {
  return epoch >= kEpochValidMin;
}

void persistConfig(bool enabled, LogLevel level, uint32_t startEpoch, uint32_t offset, bool clearRequested) {
  if (!s_logPrefs.begin(kLogPrefsNamespace, false)) return;
  s_logPrefs.putUChar(kLogKeyEnabled, enabled ? 1 : 0);
  s_logPrefs.putUChar(kLogKeyLevel, static_cast<uint8_t>(level));
  s_logPrefs.putUInt(kLogKeyStartEpoch, startEpoch);
  s_logPrefs.putUInt(kLogKeyOffset, offset);
  s_logPrefs.putUChar(kLogKeyClear, clearRequested ? 1 : 0);
  s_logPrefs.end();
}

void persistOffset(uint32_t offset) {
  if (!s_logPrefs.begin(kLogPrefsNamespace, false)) return;
  s_logPrefs.putUInt(kLogKeyOffset, offset);
  s_logPrefs.end();
}

void persistStartEpoch(uint32_t startEpoch) {
  if (!s_logPrefs.begin(kLogPrefsNamespace, false)) return;
  s_logPrefs.putUInt(kLogKeyStartEpoch, startEpoch);
  s_logPrefs.end();
}

bool ensureLogFile() {
  if (s_clearRequested && SPIFFS.exists(kLogFilePath)) {
    SPIFFS.remove(kLogFilePath);
  }

  bool needCreate = !SPIFFS.exists(kLogFilePath);
  if (!needCreate) {
    File f = SPIFFS.open(kLogFilePath, "r+");
    if (!f) {
      return false;
    }
    size_t size = f.size();
    f.close();
    if (size != kLogFileMaxBytes) {
      SPIFFS.remove(kLogFilePath);
      needCreate = true;
    }
  }

  if (needCreate) {
    File f = SPIFFS.open(kLogFilePath, FILE_WRITE);
    if (!f) {
      return false;
    }
    uint8_t zeros[64] = {0};
    size_t remaining = kLogFileMaxBytes;
    while (remaining > 0) {
      size_t chunk = remaining > sizeof(zeros) ? sizeof(zeros) : remaining;
      size_t written = f.write(zeros, chunk);
      if (written != chunk) {
        f.close();
        return false;
      }
      remaining -= written;
    }
    f.close();
    s_logOffset = 0;
    s_logBytesSincePersist = 0;
    persistOffset(s_logOffset);
  }

  return true;
}

void disableLogInternal() {
  s_logEnabled = false;
  s_logFileReady = false;
  s_logStartEpoch = 0;
  s_logStartMs = 0;
  s_logOffset = 0;
  s_logBytesSincePersist = 0;
  s_clearRequested = false;
  g_logLevel = LogLevel::Info;
  persistConfig(false, g_logLevel, 0, 0, false);
}

void logWriteBuffer(const uint8_t* data, size_t len, LogLevel level) {
  if (!s_logEnabled || !s_logFileReady || !logAllows(level)) return;
  if (s_logPaused) return;
  if (!data || len == 0) return;

  if (s_logLevel != LogLevel::Error) {
    uint32_t nowEpoch = rtc.getEpoch();
    uint32_t nowMs = millis();
    if (s_logStartEpoch == 0 && epochValid(nowEpoch)) {
      s_logStartEpoch = nowEpoch;
      persistStartEpoch(nowEpoch);
    }
    if (epochValid(nowEpoch) && s_logStartEpoch > 0) {
      if (nowEpoch - s_logStartEpoch >= kLogMaxDurationSec) {
        disableLogInternal();
        return;
      }
    } else if (nowMs - s_logStartMs >= kLogMaxDurationMs) {
      disableLogInternal();
      return;
    }
  }

  if (len > kLogFileMaxBytes) {
    data += (len - kLogFileMaxBytes);
    len = kLogFileMaxBytes;
  }

  File f = SPIFFS.open(kLogFilePath, "r+");
  if (!f) {
    s_logFileReady = false;
    return;
  }

  size_t remaining = len;
  size_t offset = s_logOffset;
  while (remaining > 0) {
    size_t chunk = kLogFileMaxBytes - offset;
    if (chunk > remaining) {
      chunk = remaining;
    }
    if (!f.seek(static_cast<uint32_t>(offset))) {
      f.close();
      s_logFileReady = false;
      return;
    }
    size_t written = f.write(data, chunk);
    if (written != chunk) {
      f.close();
      s_logFileReady = false;
      return;
    }
    data += written;
    remaining -= written;
    offset += written;
    if (offset >= kLogFileMaxBytes) {
      offset = 0;
    }
  }

  f.close();
  s_logOffset = static_cast<uint32_t>(offset);
  s_logBytesSincePersist += len;
  if (s_logBytesSincePersist >= kPersistOffsetStep) {
    persistOffset(s_logOffset);
    s_logBytesSincePersist = 0;
  }
}

class LogTeePrint : public Print {
 public:
  explicit LogTeePrint(LogLevel level) : level_(level) {}

  size_t write(uint8_t b) override {
    return write(&b, 1);
  }

  size_t write(const uint8_t* buffer, size_t size) override {
    if (!buffer || size == 0) return 0;
    Serial.write(buffer, size);
    logWriteBuffer(buffer, size, level_);
    return size;
  }

 private:
  LogLevel level_;
};

LogTeePrint s_errorPrinter(LogLevel::Error);
} // namespace

void logLoadConfig() {
  s_logEnabled = false;
  s_logLevel = LogLevel::Info;
  s_logStartEpoch = 0;
  s_logStartMs = 0;
  s_logOffset = 0;
  s_logBytesSincePersist = 0;
  s_logFileReady = false;
  s_clearRequested = false;

  if (s_logPrefs.begin(kLogPrefsNamespace, false)) {
    bool needsInit = false;
    needsInit = needsInit || !s_logPrefs.isKey(kLogKeyEnabled);
    needsInit = needsInit || !s_logPrefs.isKey(kLogKeyLevel);
    needsInit = needsInit || !s_logPrefs.isKey(kLogKeyStartEpoch);
    needsInit = needsInit || !s_logPrefs.isKey(kLogKeyOffset);
    needsInit = needsInit || !s_logPrefs.isKey(kLogKeyClear);
    if (needsInit) {
      s_logPrefs.putUChar(kLogKeyEnabled, 0);
      s_logPrefs.putUChar(kLogKeyLevel, static_cast<uint8_t>(LogLevel::Info));
      s_logPrefs.putUInt(kLogKeyStartEpoch, 0);
      s_logPrefs.putUInt(kLogKeyOffset, 0);
      s_logPrefs.putUChar(kLogKeyClear, 0);
    }
    s_logEnabled = s_logPrefs.getUChar(kLogKeyEnabled, 0) != 0;
    s_logLevel = clampLevel(s_logPrefs.getUChar(kLogKeyLevel, static_cast<uint8_t>(LogLevel::Info)));
    s_logStartEpoch = s_logPrefs.getUInt(kLogKeyStartEpoch, 0);
    s_logOffset = s_logPrefs.getUInt(kLogKeyOffset, 0);
    s_clearRequested = s_logPrefs.getUChar(kLogKeyClear, 0) != 0;
    s_logPrefs.end();
  }

  if (s_logOffset >= kLogFileMaxBytes) {
    s_logOffset = 0;
  }

  if (s_logEnabled) {
    g_logLevel = s_logLevel;
    s_logStartMs = millis();
  } else {
    g_logLevel = LogLevel::Info;
  }
}

void logBegin() {
  if (!s_logEnabled) return;
  if (!ensureLogFile()) {
    s_logFileReady = false;
    return;
  }
  s_logFileReady = true;
  if (s_clearRequested) {
    s_clearRequested = false;
    if (s_logPrefs.begin(kLogPrefsNamespace, false)) {
      s_logPrefs.putUChar(kLogKeyClear, 0);
      s_logPrefs.end();
    }
  }
}

void logService() {
  static uint32_t lastCheckMs = 0;
  const uint32_t nowMs = millis();
  if (nowMs - lastCheckMs < 1000) return;
  lastCheckMs = nowMs;

  if (!s_logEnabled) return;
  if (s_logLevel == LogLevel::Error) return;

  uint32_t nowEpoch = rtc.getEpoch();
  if (epochValid(nowEpoch) && s_logStartEpoch == 0) {
    s_logStartEpoch = nowEpoch;
    persistStartEpoch(nowEpoch);
  }
  if (epochValid(nowEpoch) && s_logStartEpoch > 0) {
    if (nowEpoch - s_logStartEpoch >= kLogMaxDurationSec) {
      disableLogInternal();
    }
    return;
  }

  if (nowMs - s_logStartMs >= kLogMaxDurationMs) {
    disableLogInternal();
  }
}

bool logSetEnabled(bool enabled, LogLevel level) {
  if (enabled) {
    s_logEnabled = true;
    s_logLevel = level;
    g_logLevel = level;
    s_logStartMs = millis();
    uint32_t nowEpoch = rtc.getEpoch();
    s_logStartEpoch = epochValid(nowEpoch) ? nowEpoch : 0;
    s_logOffset = 0;
    s_logBytesSincePersist = 0;
    s_logFileReady = false;
    s_clearRequested = true;
    persistConfig(true, s_logLevel, s_logStartEpoch, s_logOffset, true);
    return true;
  }

  disableLogInternal();
  return true;
}

bool logIsEnabled() {
  return s_logEnabled;
}

LogLevel logConfiguredLevel() {
  return s_logLevel;
}

const char* logLevelName(LogLevel level) {
  switch (level) {
    case LogLevel::Debug:
      return "debug";
    case LogLevel::Info:
      return "info";
    case LogLevel::Warn:
      return "warn";
    case LogLevel::Error:
      return "error";
  }
  return "info";
}

String logStatusJson() {
  String resp = "{\"status\":\"ok\",\"log\":{";
  resp += "\"enabled\":";
  resp += s_logEnabled ? "true" : "false";
  resp += ",\"level\":\"";
  resp += logLevelName(s_logLevel);
  resp += "\",\"max_bytes\":";
  resp += String(kLogFileMaxBytes);
  resp += ",\"offset\":";
  resp += String(s_logOffset);
  resp += ",\"start_epoch\":";
  resp += String(s_logStartEpoch);
  if (s_logEnabled && s_logLevel != LogLevel::Error) {
    int32_t remaining = -1;
    uint32_t nowEpoch = rtc.getEpoch();
    if (epochValid(nowEpoch) && s_logStartEpoch > 0) {
      uint32_t elapsed = nowEpoch - s_logStartEpoch;
      if (elapsed >= kLogMaxDurationSec) {
        remaining = 0;
      } else {
        remaining = static_cast<int32_t>(kLogMaxDurationSec - elapsed);
      }
    } else {
      uint32_t elapsedMs = millis() - s_logStartMs;
      if (elapsedMs >= kLogMaxDurationMs) {
        remaining = 0;
      } else {
        remaining = static_cast<int32_t>((kLogMaxDurationMs - elapsedMs) / 1000UL);
      }
    }
    resp += ",\"expires_in_sec\":";
    resp += String(remaining);
  }
  resp += "}}";
  return resp;
}

bool logReadOrderedBytes(std::vector<uint8_t>& ordered, uint32_t& offset) {
  struct PauseGuard {
    bool& flag;
    bool prev;
    explicit PauseGuard(bool& f) : flag(f), prev(f) { flag = true; }
    ~PauseGuard() { flag = prev; }
  };

  PauseGuard guard(s_logPaused);

  ordered.clear();
  offset = s_logOffset;

  if (!SPIFFS.exists(kLogFilePath)) {
    return false;
  }
  File f = SPIFFS.open(kLogFilePath, "r");
  if (!f) {
    return false;
  }

  size_t toRead = f.size();
  if (toRead > kLogFileMaxBytes) {
    toRead = kLogFileMaxBytes;
  }
  if (toRead == 0) {
    f.close();
    return true;
  }

  std::vector<uint8_t> data(toRead);
  size_t readBytes = f.read(data.data(), toRead);
  f.close();
  if (readBytes == 0) {
    return false;
  }
  data.resize(readBytes);

  if (offset >= readBytes) {
    offset = 0;
  }

  ordered.reserve(readBytes);
  if (offset > 0) {
    bool tailHasData = false;
    for (size_t i = offset; i < readBytes; ++i) {
      if (data[i] != 0) {
        tailHasData = true;
        break;
      }
    }
    if (tailHasData) {
      ordered.insert(ordered.end(), data.begin() + offset, data.end());
      ordered.insert(ordered.end(), data.begin(), data.begin() + offset);
    } else {
      ordered.insert(ordered.end(), data.begin(), data.begin() + offset);
    }
  } else {
    ordered.assign(data.begin(), data.end());
  }

  size_t start = 0;
  size_t end = ordered.size();
  while (start < end && ordered[start] == 0) {
    ++start;
  }
  while (end > start && ordered[end - 1] == 0) {
    --end;
  }
  if (start > 0 || end < ordered.size()) {
    ordered = std::vector<uint8_t>(ordered.begin() + start, ordered.begin() + end);
  }

  return true;
}

bool logReadOrderedBase64(String& outB64, size_t& rawSize, uint32_t& offset) {
  outB64 = "";
  rawSize = 0;

  std::vector<uint8_t> ordered;
  if (!logReadOrderedBytes(ordered, offset)) {
    return false;
  }

  rawSize = ordered.size();
  if (rawSize == 0) {
    return true;
  }

  size_t outLen = 4 * ((rawSize + 2) / 3) + 4;
  std::vector<unsigned char> buf(outLen, 0);
  size_t written = 0;
  int rc = mbedtls_base64_encode(buf.data(), buf.size(), &written,
                                 ordered.data(), rawSize);
  if (rc != 0 || written == 0) {
    return false;
  }
  buf[written] = 0;
  outB64 = String(reinterpret_cast<char*>(buf.data()));
  if (outB64.length() != written) {
    outB64 = "";
    return false;
  }
  return true;
}

Print& logErrorPrint() {
  return s_errorPrinter;
}

void logPrintf(const char* level_str, const char* module, LogLevel lvl, const char* fmt, ...) {
  char msg[kLogLineMax] = {0};
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  char line[kLogLineBufMax] = {0};
  snprintf(line, sizeof(line), "[%s][%s][t=%lums] %s\n",
           level_str, module, static_cast<unsigned long>(millis()), msg);
  Serial.print(line);
  logWriteBuffer(reinterpret_cast<const uint8_t*>(line), strlen(line), lvl);
}
