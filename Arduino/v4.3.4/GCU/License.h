#ifndef LICENSE_H
#define LICENSE_H

#include <Arduino.h>

struct LicenseInfo {
  bool valid;
  uint8_t tier;
  uint32_t expiryEpoch;
  uint64_t boundMac;
  const char* error;  // static/error buffer view
};

extern LicenseInfo g_licenseInfo;
extern const char kLicensePublicKeyPem[];

// License tiers
constexpr uint8_t LICENSE_TIER_BASIC = 0x01;
constexpr uint8_t LICENSE_TIER_ADV   = 0x02;
constexpr uint8_t LICENSE_TIER_PRO   = 0x03;

bool licenseInit(uint64_t deviceMac);
// Validate and load stored licenses (max 5), pick highest tier valid at current time
bool licenseValidateStored(uint32_t nowEpoch);
// Add a new license (evict expired or earliest expiry when capacity exceeded)
bool licenseApplyToken(const String& token, uint32_t nowEpoch);
// Runtime validity check (includes expiry)
bool licenseIsValid();
const char* licenseTierName(uint8_t tier);
const char* licenseLastError();
String licenseDescribeAll();

// Return tier of current valid license (0 when invalid)
uint8_t licenseCurrentTier();
// Return expiry epoch of current valid license (0 when invalid)
uint32_t licenseCurrentExpiry();
// Whether current license meets at least the requested tier
bool licenseAtLeast(uint8_t minTier);

#endif // LICENSE_H
