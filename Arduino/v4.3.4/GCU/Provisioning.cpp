#include "Provisioning.h"
#include "GCU.h"

#include <cstdio>
#include <ctime>

#ifndef __has_include
#define __has_include(x) 0
#endif

#if __has_include("esp_gap_ble_api.h")
#include "esp_gap_ble_api.h"
#define HAS_ESP_GAP_BLE 1
#else
#define HAS_ESP_GAP_BLE 0
#endif

namespace {

/**
 * @brief Generate a UUIDv1 using host time/micros and the device MAC.
 * @param mac Device MAC address.
 * @param out Output 16-byte UUID buffer.
 */
void uuidV1FromMac(uint64_t mac, uint8_t out[16]) {
  time_t now = time(nullptr);
  uint64_t unix_sec;
  if (now > 1609459200) { // If time is synced (> 2021-01-01)
    unix_sec = static_cast<uint64_t>(now);
  } else {
    unix_sec = static_cast<uint64_t>(millis() / 1000);
  }

  const uint64_t kUuidEpochDiff = 12219292800ULL;
  uint64_t ts100ns = (unix_sec + kUuidEpochDiff) * 10000000ULL
                     + (static_cast<uint64_t>(micros() % 1000000ULL) * 10ULL);

  uint32_t time_low = static_cast<uint32_t>(ts100ns & 0xFFFFFFFFULL);
  uint16_t time_mid = static_cast<uint16_t>((ts100ns >> 32) & 0xFFFFULL);
  uint16_t time_hi  = static_cast<uint16_t>((ts100ns >> 48) & 0x0FFFULL);
  time_hi |= (1 << 12); // Version bits: v1

  uint16_t clock_seq = static_cast<uint16_t>(esp_random() & 0x3FFF);

  out[0] = static_cast<uint8_t>((time_low >> 24) & 0xFF);
  out[1] = static_cast<uint8_t>((time_low >> 16) & 0xFF);
  out[2] = static_cast<uint8_t>((time_low >> 8) & 0xFF);
  out[3] = static_cast<uint8_t>(time_low & 0xFF);
  out[4] = static_cast<uint8_t>((time_mid >> 8) & 0xFF);
  out[5] = static_cast<uint8_t>(time_mid & 0xFF);
  out[6] = static_cast<uint8_t>((time_hi >> 8) & 0xFF);
  out[7] = static_cast<uint8_t>(time_hi & 0xFF);
  out[8] = static_cast<uint8_t>(((clock_seq >> 8) & 0x3F) | 0x80);
  out[9] = static_cast<uint8_t>(clock_seq & 0xFF);
  out[10] = static_cast<uint8_t>(mac >> 40);
  out[11] = static_cast<uint8_t>(mac >> 32);
  out[12] = static_cast<uint8_t>(mac >> 24);
  out[13] = static_cast<uint8_t>(mac >> 16);
  out[14] = static_cast<uint8_t>(mac >> 8);
  out[15] = static_cast<uint8_t>(mac);
}

} // namespace

/**
 * @brief Populate provisioning context (service name + UUID) for BLE.
 * @param ctx Provisioning context structure to fill.
 * @param mac Device MAC address used for uniqueness.
 */
void provisioningBuildContext(ProvisioningContext &ctx, uint64_t mac) {
  snprintf(ctx.serviceName, sizeof(ctx.serviceName), "PROV_%02X%02X%02X%02X%02X%02X",
           static_cast<uint8_t>(mac >> 40),
           static_cast<uint8_t>(mac >> 32),
           static_cast<uint8_t>(mac >> 24),
           static_cast<uint8_t>(mac >> 16),
           static_cast<uint8_t>(mac >> 8),
           static_cast<uint8_t>(mac));
  uuidV1FromMac(mac, ctx.uuid);
}

/**
 * @brief Dump the BLE provisioning UUID to the serial console.
 * @param ctx Provisioning context containing the UUID.
 */
void provisioningLogUuid(const ProvisioningContext &ctx) {
  char uuidStr[37] = {0};
  snprintf(uuidStr, sizeof(uuidStr),
           "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
           ctx.uuid[0], ctx.uuid[1], ctx.uuid[2], ctx.uuid[3],
           ctx.uuid[4], ctx.uuid[5],
           ctx.uuid[6], ctx.uuid[7],
           ctx.uuid[8], ctx.uuid[9],
           ctx.uuid[10], ctx.uuid[11], ctx.uuid[12], ctx.uuid[13], ctx.uuid[14], ctx.uuid[15]);
  LOGI("PROV", "ble_uuid=%s", uuidStr);
}

/**
 * @brief Start BLE provisioning with the prepared context and options.
 * @param ctx Provisioning context (service name + UUID).
 * @param resetProvisioned When true, erase stored credentials.
 * @param pop Proof-of-possession PIN string.
 * @param serviceKey Optional SoftAP password (NULL for open).
 */
void provisioningStart(const ProvisioningContext &ctx,
                       bool resetProvisioned,
                       const char *pop,
                       const char *serviceKey) {
  WiFiProv.beginProvision(
    NETWORK_PROV_SCHEME_BLE,
    NETWORK_PROV_SCHEME_HANDLER_FREE_BLE,
    NETWORK_PROV_SECURITY_1,
    pop,
    ctx.serviceName,
    serviceKey,
    const_cast<uint8_t *>(ctx.uuid),
    resetProvisioned
  );

#if HAS_ESP_GAP_BLE
  esp_ble_gap_set_device_name(ctx.serviceName);
#endif
}
