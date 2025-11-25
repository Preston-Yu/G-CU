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
// 校验并加载存储的授权（最多5条），根据当前时间选出最高等级可用授权
bool licenseValidateStored(uint32_t nowEpoch);
// 新增一条授权（若超过上限，优先替换已过期/最早到期者）
bool licenseApplyToken(const String& token, uint32_t nowEpoch);
// 运行时检查有效性（含过期判断）
bool licenseIsValid();
const char* licenseTierName(uint8_t tier);
const char* licenseLastError();
String licenseDescribeAll();

// 返回当前有效授权的 tier（无效返回 0）
uint8_t licenseCurrentTier();
// 返回当前有效授权的到期时间（epoch，0 表示无效）
uint32_t licenseCurrentExpiry();
// 是否具有至少指定等级的有效授权
bool licenseAtLeast(uint8_t minTier);

#endif // LICENSE_H
