#include "License.h"

#include <Preferences.h>
#include <mbedtls/error.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <time.h>
#include <cstring>

namespace {
constexpr uint8_t kVersionV2 = 0x02;
constexpr size_t kPayloadLenV2 = 1 /*version*/ + 1 /*tier*/ + 4 /*expiry*/ + 6 /*mac*/;
constexpr size_t kMaxSigLen = 80; // DER-encoded ECDSA P-256 signatures are typically ~70 bytes
constexpr size_t kMaxTokens = 5;
constexpr char kNvsNamespace[] = "license";
constexpr char kNvsKeyTokens[] = "tokens";

struct TokenSlot {
  String token;
  uint8_t tier;
  uint32_t expiry;
  uint64_t mac;
  bool valid;
};

Preferences s_licensePrefs;
uint64_t s_deviceMac = 0;
TokenSlot s_slots[kMaxTokens];
size_t s_slotCount = 0;
String s_lastError;

mbedtls_pk_context s_pubKey;
bool s_pubKeyReady = false;

int decodeBase32Char(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= '2' && c <= '7') return 26 + (c - '2');
  if (c >= 'a' && c <= 'z') return c - 'a';
  return -1;
}

bool base32Decode(const String& input, uint8_t* out, size_t maxOut, size_t& outLen) {
  outLen = 0;
  uint32_t buffer = 0;
  int bitsLeft = 0;
  for (size_t i = 0; i < input.length(); ++i) {
    char c = input.charAt(i);
    if (c == '=') break;
    int val = decodeBase32Char(c);
    if (val < 0) return false;
    buffer = (buffer << 5) | static_cast<uint32_t>(val);
    bitsLeft += 5;
    if (bitsLeft >= 8) {
      bitsLeft -= 8;
      if (outLen >= maxOut) return false;
      out[outLen++] = static_cast<uint8_t>((buffer >> bitsLeft) & 0xFF);
    }
  }
  // Allow leftover bits when no padding was included.
  return bitsLeft == 0 || bitsLeft < 5;
}

uint64_t macBytesToUint64(const uint8_t mac[6]) {
  uint64_t val = 0;
  for (int i = 0; i < 6; ++i) {
    val = (val << 8) | mac[i];
  }
  return val;
}

const char* tierNameFromCode(uint8_t tier) {
  switch (tier) {
    case 0x01: return "basic";
    case 0x02: return "advanced";
    case 0x03: return "pro";
    default: return "unknown";
  }
}

bool ensurePubKey() {
  if (s_pubKeyReady) return true;
  if (!kLicensePublicKeyPem[0]) {
    s_lastError = "pubkey_not_set";
    return false;
  }
  mbedtls_pk_init(&s_pubKey);
  int rc = mbedtls_pk_parse_public_key(&s_pubKey,
                                       reinterpret_cast<const unsigned char*>(kLicensePublicKeyPem),
                                       strlen(kLicensePublicKeyPem) + 1);
  if (rc != 0) {
    s_lastError = "pubkey_parse";
    return false;
  }
  if (mbedtls_pk_get_type(&s_pubKey) != MBEDTLS_PK_ECKEY) {
    s_lastError = "pubkey_type";
    return false;
  }
  s_pubKeyReady = true;
  return true;
}

bool verifySignature(const uint8_t* payload, size_t payloadLen, const uint8_t* sig, size_t sigLen) {
  if (!ensurePubKey()) return false;
  unsigned char hash[32];
  int rc = mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                      payload, payloadLen, hash);
  if (rc != 0) {
    s_lastError = "hash";
    return false;
  }
  rc = mbedtls_pk_verify(&s_pubKey, MBEDTLS_MD_SHA256,
                         hash, sizeof(hash),
                         sig, sigLen);
  if (rc != 0) {
    s_lastError = "sig";
    return false;
  }
  return true;
}

bool parseTokenV2(const String& token, TokenSlot& outSlot, uint32_t nowEpoch, String& err) {
  err = "invalid";
  uint8_t decoded[128] = {0};
  size_t decodedLen = 0;
  if (!base32Decode(token, decoded, sizeof(decoded), decodedLen)) {
    err = "base32_decode";
    return false;
  }
  if (decodedLen < kPayloadLenV2 + 1) {
    err = "length_mismatch";
    return false;
  }
  uint8_t sigLen = decoded[kPayloadLenV2];
  size_t expectedLen = kPayloadLenV2 + 1 + sigLen;
  if (sigLen == 0 || sigLen > kMaxSigLen || decodedLen != expectedLen) {
    err = "sig_len";
    return false;
  }
  const uint8_t* payload = decoded;
  const uint8_t* sig = decoded + kPayloadLenV2 + 1;
  if (payload[0] != kVersionV2) {
    err = "version";
    return false;
  }
  uint8_t tier = payload[1];
  uint32_t expiry = (static_cast<uint32_t>(payload[2]) << 24) |
                    (static_cast<uint32_t>(payload[3]) << 16) |
                    (static_cast<uint32_t>(payload[4]) << 8) |
                    static_cast<uint32_t>(payload[5]);
  uint64_t macVal = macBytesToUint64(payload + 6);
  if (macVal != s_deviceMac) {
    err = "mac_mismatch";
    return false;
  }
  if (nowEpoch >= expiry) {
    err = "expired";
    return false;
  }
  if (!verifySignature(payload, kPayloadLenV2, sig, sigLen)) {
    if (s_lastError.length()) err = s_lastError;
    return false;
  }
  outSlot.token = token;
  outSlot.tier = tier;
  outSlot.expiry = expiry;
  outSlot.mac = macVal;
  outSlot.valid = true;
  err = "";
  return true;
}

bool saveTokensToNvs() {
  if (!s_licensePrefs.begin(kNvsNamespace, false)) {
    return false;
  }
  String blob;
  for (size_t i = 0; i < s_slotCount; ++i) {
    if (!s_slots[i].valid || s_slots[i].token.length() == 0) continue;
    if (blob.length() > 0) blob += "\n";
    blob += s_slots[i].token;
  }
  bool ok = s_licensePrefs.putString(kNvsKeyTokens, blob) > 0;
  s_licensePrefs.end();
  return ok;
}

bool recomputeActive(uint32_t nowEpoch) {
  g_licenseInfo.valid = false;
  g_licenseInfo.error = "empty";
  uint8_t bestTier = 0;
  uint32_t bestExpiry = 0;
  for (size_t i = 0; i < s_slotCount; ++i) {
    const TokenSlot& slot = s_slots[i];
    if (!slot.valid) continue;
    if (slot.expiry <= nowEpoch) continue;
    if (slot.mac != s_deviceMac) continue;
    if (!g_licenseInfo.valid ||
        slot.tier > bestTier ||
        (slot.tier == bestTier && slot.expiry > bestExpiry)) {
      bestTier = slot.tier;
      bestExpiry = slot.expiry;
      g_licenseInfo.valid = true;
      g_licenseInfo.tier = slot.tier;
      g_licenseInfo.expiryEpoch = slot.expiry;
      g_licenseInfo.boundMac = slot.mac;
      g_licenseInfo.error = nullptr;
    }
  }
  if (!g_licenseInfo.valid) {
    g_licenseInfo.error = s_lastError.length() ? s_lastError.c_str() : "invalid";
  }
  return g_licenseInfo.valid;
}

int findTokenIndex(const String& token) {
  for (size_t i = 0; i < s_slotCount; ++i) {
    if (s_slots[i].token == token) return static_cast<int>(i);
  }
  return -1;
}

int findExpiredIndex(uint32_t nowEpoch) {
  for (size_t i = 0; i < s_slotCount; ++i) {
    if (!s_slots[i].valid) return static_cast<int>(i);
    if (s_slots[i].expiry <= nowEpoch) return static_cast<int>(i);
  }
  return -1;
}

int findEarliestExpiryIndex() {
  if (s_slotCount == 0) return -1;
  uint32_t earliest = s_slots[0].expiry;
  size_t idx = 0;
  for (size_t i = 1; i < s_slotCount; ++i) {
    if (s_slots[i].expiry < earliest) {
      earliest = s_slots[i].expiry;
      idx = i;
    }
  }
  return static_cast<int>(idx);
}

}  // namespace

LicenseInfo g_licenseInfo{false, 0, 0, 0, "uninitialized"};
// ECDSA P-256 public key (PEM, include BEGIN/END lines)
const char kLicensePublicKeyPem[] =
"-----BEGIN PUBLIC KEY-----\n"
"MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEm0g4NGr6ZRJ4apYXTeuwbJ7p8e8D\n"
"PtXRKhjIPLQA4e7k9uur6u+JfhUA2UxnrbTJmvxNdicMZiWWNLboRuVnmA==\n"
"-----END PUBLIC KEY-----\n";

bool licenseInit(uint64_t deviceMac) {
  s_deviceMac = deviceMac;
  g_licenseInfo.boundMac = deviceMac;
  s_slotCount = 0;
  s_lastError = "empty";
  s_pubKeyReady = false;
  mbedtls_pk_init(&s_pubKey);
  return true;
}

bool licenseValidateStored(uint32_t nowEpoch) {
  s_slotCount = 0;
  s_lastError = "empty";
  g_licenseInfo.valid = false;
  g_licenseInfo.error = "empty";
  // Namespace may not exist yet (fresh device) -> treat as empty, not an error.
  if (!s_licensePrefs.begin(kNvsNamespace, true)) {
    s_lastError = "empty";
    g_licenseInfo.error = s_lastError.c_str();
    return false;
  }
  String blob = s_licensePrefs.getString(kNvsKeyTokens, "");
  s_licensePrefs.end();
  if (blob.length() == 0) {
    s_lastError = "empty";
    g_licenseInfo.error = s_lastError.c_str();
    return false;
  }

  int start = 0;
  while (start < blob.length() && s_slotCount < kMaxTokens) {
    int nl = blob.indexOf('\n', start);
    if (nl < 0) nl = blob.length();
    String tok = blob.substring(start, nl);
    tok.trim();
    start = nl + 1;
    if (tok.length() == 0) continue;
    TokenSlot slot{};
    String err;
    if (parseTokenV2(tok, slot, nowEpoch, err)) {
      s_slots[s_slotCount++] = slot;
    } else {
      s_lastError = err;
    }
  }
  if (s_slotCount == 0) {
    g_licenseInfo.error = s_lastError.length() ? s_lastError.c_str() : "invalid";
    return false;
  }
  return recomputeActive(nowEpoch);
}

bool licenseApplyToken(const String& token, uint32_t nowEpoch) {
  TokenSlot slot{};
  String err;
  if (!parseTokenV2(token, slot, nowEpoch, err)) {
    s_lastError = err;
    return false;
  }

  int existing = findTokenIndex(token);
  if (existing >= 0) {
    s_slots[existing] = slot;
  } else if (s_slotCount < kMaxTokens) {
    s_slots[s_slotCount++] = slot;
  } else {
    int idx = findExpiredIndex(nowEpoch);
    if (idx < 0) idx = findEarliestExpiryIndex();
    if (idx < 0) idx = 0;
    s_slots[idx] = slot;
  }

  saveTokensToNvs();
  recomputeActive(nowEpoch);
  return g_licenseInfo.valid;
}

bool licenseIsValid() {
  time_t now = time(nullptr);
  // If current cache is invalid, try recompute from available slots.
  if (!g_licenseInfo.valid) {
    return recomputeActive(static_cast<uint32_t>(now));
  }
  if (now >= static_cast<time_t>(g_licenseInfo.expiryEpoch)) {
    // Current license expired; try finding another valid one from slots.
    s_lastError = "expired";
    g_licenseInfo.valid = false;
    g_licenseInfo.error = s_lastError.c_str();
    return recomputeActive(static_cast<uint32_t>(now));
  }
  return true;
}

const char* licenseTierName(uint8_t tier) {
  return tierNameFromCode(tier);
}

const char* licenseLastError() {
  return s_lastError.length() ? s_lastError.c_str() : "invalid";
}

String licenseDescribeAll() {
  String out = "{";
  // device mac
  char macStr[18] = {0};
  snprintf(macStr, sizeof(macStr), "%012llX", static_cast<unsigned long long>(s_deviceMac & 0xFFFFFFFFFFFFULL));
  out += "\"device_mac\":\"";
  out += macStr;
  out += "\",\"licenses\":[";
  time_t now = time(nullptr);
  for (size_t i = 0; i < s_slotCount; ++i) {
    const TokenSlot& slot = s_slots[i];
    if (i > 0) out += ",";
    out += "{";
    out += "\"tier\":\"";
    out += licenseTierName(slot.tier);
    out += "\",\"expiry\":";
    out += String(slot.expiry);
    out += ",\"mac\":\"";
    snprintf(macStr, sizeof(macStr), "%012llX", static_cast<unsigned long long>(slot.mac & 0xFFFFFFFFFFFFULL));
    out += macStr;
    out += "\",\"valid\":";
    out += (slot.valid && slot.expiry > static_cast<uint32_t>(now)) ? "true" : "false";
    out += ",\"token\":\"";
    out += slot.token;
    out += "\"}";
  }
  out += "]}";
  return out;
}

uint8_t licenseCurrentTier() {
  return g_licenseInfo.valid ? g_licenseInfo.tier : 0;
}

uint32_t licenseCurrentExpiry() {
  return g_licenseInfo.valid ? g_licenseInfo.expiryEpoch : 0;
}

bool licenseAtLeast(uint8_t minTier) {
  if (!licenseIsValid()) return false;
  return g_licenseInfo.tier >= minTier;
}
