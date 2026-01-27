#ifndef BQ25180_H
#define BQ25180_H

#include "BatteryConfig.h"
#include "ModelConfig.h"
#include <stdint.h>

#ifndef BQ25180_CONFIG_VBAT_MV
#define BQ25180_CONFIG_VBAT_MV BATTERY_CHARGE_VOLTAGE_MV
#endif
#ifndef BQ25180_CONFIG_ICHG_MA
#define BQ25180_CONFIG_ICHG_MA BATTERY_CHARGE_CURRENT_MA
#endif
#ifndef BQ25180_CONFIG_ILIM_MA
#define BQ25180_CONFIG_ILIM_MA BATTERY_INPUT_LIMIT_MA
#endif
#ifndef BQ25180_CONFIG_CHARGE_ENABLE
#define BQ25180_CONFIG_CHARGE_ENABLE BATTERY_CHARGE_ENABLE
#endif

struct Bq25180Status {
  uint8_t stat0;
  uint8_t stat1;
  uint8_t flag0;
};

void bq25180Setup();
bool bq25180IsPresent();
bool bq25180ReadStatus(Bq25180Status& out);
bool bq25180SetChargeCurrentMa(uint16_t ma);
bool bq25180SetBatteryRegMv(uint16_t mv);
bool bq25180SetInputLimitMa(uint16_t ma);
bool bq25180EnableCharge(bool enable);

#endif
