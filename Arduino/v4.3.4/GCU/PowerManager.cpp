#include "PowerManager.h"

#if GCU_HAS_BQ25180
#include "BQ25180.h"
#endif

void powerManagerSetup() {
#if GCU_HAS_BQ25180
  bq25180Setup();
#endif
}
