#include "BQ25180.h"

#if GCU_HAS_BQ25180
#include "GCU.h"

namespace {

constexpr uint8_t kBq25180Address = 0x6A;
constexpr uint8_t kRegStat0 = 0x00;
constexpr uint8_t kRegStat1 = 0x01;
constexpr uint8_t kRegFlag0 = 0x02;
constexpr uint8_t kRegVbatCtrl = 0x03;
constexpr uint8_t kRegIchgCtrl = 0x04;
constexpr uint8_t kRegTmrIlim = 0x08;
constexpr uint8_t kRegMaskId = 0x0C;

constexpr uint8_t kMaskVbatReg = 0x7F;
constexpr uint8_t kMaskIchgCode = 0x7F;
constexpr uint8_t kMaskChgDisable = 0x80;
constexpr uint8_t kMaskIlim = 0x07;

bool g_bq25180Present = false;

class I2cClockGuard {
 public:
  explicit I2cClockGuard(uint32_t targetHz)
      : previousHz_(Wire.getClock()), changed_(false) {
    if (previousHz_ != targetHz) {
      Wire.setClock(targetHz);
      changed_ = true;
    }
  }

  ~I2cClockGuard() {
    if (changed_) {
      Wire.setClock(previousHz_);
    }
  }

 private:
  uint32_t previousHz_;
  bool changed_;
};

bool bq25180ReadReg(uint8_t reg, uint8_t& value) {
  I2cClockGuard clockGuard(GCU_BQ25180_I2C_HZ);
  Wire.beginTransmission(kBq25180Address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(kBq25180Address, static_cast<uint8_t>(1)) != 1) {
    return false;
  }
  value = Wire.read();
  return true;
}

bool bq25180WriteReg(uint8_t reg, uint8_t value) {
  I2cClockGuard clockGuard(GCU_BQ25180_I2C_HZ);
  Wire.beginTransmission(kBq25180Address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool bq25180UpdateBits(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t current = 0;
  if (!bq25180ReadReg(reg, current)) {
    return false;
  }
  uint8_t next = (current & ~mask) | (value & mask);
  if (next == current) {
    return true;
  }
  return bq25180WriteReg(reg, next);
}

uint8_t encodeVbatRegMv(uint16_t mv) {
  if (mv <= 3500) {
    return 0;
  }
  if (mv >= 4650) {
    return 115;
  }
  return static_cast<uint8_t>((mv - 3500 + 5) / 10);
}

uint8_t encodeIchgMa(uint16_t ma) {
  if (ma <= 5) {
    return 0;
  }
  if (ma <= 35) {
    return static_cast<uint8_t>(ma - 5);
  }
  if (ma < 40) {
    ma = 40;
  }
  uint16_t code = 31 + ((ma - 40 + 5) / 10);
  if (code > 127) {
    code = 127;
  }
  return static_cast<uint8_t>(code);
}

uint8_t encodeIlimMa(uint16_t ma) {
  const uint16_t kIlimOptions[] = {50, 100, 200, 300, 400, 500, 700, 1100};
  uint8_t bestIndex = 0;
  uint16_t bestDiff = 0xFFFF;
  for (uint8_t i = 0; i < 8; ++i) {
    uint16_t option = kIlimOptions[i];
    uint16_t diff = (ma > option) ? (ma - option) : (option - ma);
    if (diff < bestDiff) {
      bestDiff = diff;
      bestIndex = i;
    }
  }
  return bestIndex;
}

}  // namespace

void bq25180Setup() {
  uint8_t maskId = 0;
  if (!bq25180ReadReg(kRegMaskId, maskId)) {
    LOGW("BQ25180", "probe status=fail addr=0x%02X", kBq25180Address);
    g_bq25180Present = false;
    return;
  }
  g_bq25180Present = true;
  uint8_t deviceId = maskId & 0x0F;
  if (deviceId != 0x00) {
    LOGW("BQ25180", "device_id_mismatch id=0x%02X mask=0x%02X", deviceId, maskId);
  } else {
    LOGI("BQ25180", "detected addr=0x%02X mask=0x%02X", kBq25180Address, maskId);
  }

#if BQ25180_CONFIG_VBAT_MV > 0
  if (!bq25180SetBatteryRegMv(BQ25180_CONFIG_VBAT_MV)) {
    LOGW("BQ25180", "set_vbat_mv status=fail value=%u", BQ25180_CONFIG_VBAT_MV);
  }
#endif
#if BQ25180_CONFIG_ICHG_MA > 0
  if (!bq25180SetChargeCurrentMa(BQ25180_CONFIG_ICHG_MA)) {
    LOGW("BQ25180", "set_ichg_ma status=fail value=%u", BQ25180_CONFIG_ICHG_MA);
  }
#endif
#if BQ25180_CONFIG_ILIM_MA > 0
  if (!bq25180SetInputLimitMa(BQ25180_CONFIG_ILIM_MA)) {
    LOGW("BQ25180", "set_ilim_ma status=fail value=%u", BQ25180_CONFIG_ILIM_MA);
  }
#endif
#if BQ25180_CONFIG_CHARGE_ENABLE >= 0
  if (!bq25180EnableCharge(BQ25180_CONFIG_CHARGE_ENABLE != 0)) {
    LOGW("BQ25180", "charge_enable status=fail value=%d", BQ25180_CONFIG_CHARGE_ENABLE);
  }
#endif
}

bool bq25180IsPresent() {
  return g_bq25180Present;
}

bool bq25180ReadStatus(Bq25180Status& out) {
  if (!g_bq25180Present) {
    return false;
  }
  if (!bq25180ReadReg(kRegStat0, out.stat0)) {
    return false;
  }
  if (!bq25180ReadReg(kRegStat1, out.stat1)) {
    return false;
  }
  if (!bq25180ReadReg(kRegFlag0, out.flag0)) {
    return false;
  }
  return true;
}

bool bq25180SetChargeCurrentMa(uint16_t ma) {
  if (!g_bq25180Present) {
    return false;
  }
  uint8_t code = encodeIchgMa(ma);
  return bq25180UpdateBits(kRegIchgCtrl, kMaskIchgCode, code);
}

bool bq25180SetBatteryRegMv(uint16_t mv) {
  if (!g_bq25180Present) {
    return false;
  }
  uint8_t code = encodeVbatRegMv(mv);
  return bq25180UpdateBits(kRegVbatCtrl, kMaskVbatReg, code);
}

bool bq25180SetInputLimitMa(uint16_t ma) {
  if (!g_bq25180Present) {
    return false;
  }
  uint8_t code = encodeIlimMa(ma);
  return bq25180UpdateBits(kRegTmrIlim, kMaskIlim, code);
}

bool bq25180EnableCharge(bool enable) {
  if (!g_bq25180Present) {
    return false;
  }
  uint8_t value = enable ? 0x00 : kMaskChgDisable;
  return bq25180UpdateBits(kRegIchgCtrl, kMaskChgDisable, value);
}

#else

void bq25180Setup() {
}

bool bq25180IsPresent() {
  return false;
}

bool bq25180ReadStatus(Bq25180Status& out) {
  (void)out;
  return false;
}

bool bq25180SetChargeCurrentMa(uint16_t ma) {
  (void)ma;
  return false;
}

bool bq25180SetBatteryRegMv(uint16_t mv) {
  (void)mv;
  return false;
}

bool bq25180SetInputLimitMa(uint16_t ma) {
  (void)ma;
  return false;
}

bool bq25180EnableCharge(bool enable) {
  (void)enable;
  return false;
}

#endif
