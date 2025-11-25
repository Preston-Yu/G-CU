#ifndef PROVISIONING_H
#define PROVISIONING_H

#include <stdint.h>

struct ProvisioningContext {
  char serviceName[32];
  uint8_t uuid[16];
};

void provisioningBuildContext(ProvisioningContext &ctx, uint64_t mac);
void provisioningLogUuid(const ProvisioningContext &ctx);
void provisioningStart(const ProvisioningContext &ctx,
                       bool resetProvisioned,
                       const char *pop,
                       const char *serviceKey);

#endif // PROVISIONING_H
