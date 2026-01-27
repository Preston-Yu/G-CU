#ifndef MODEL_CONFIG_H
#define MODEL_CONFIG_H

#if (defined(GCU_MODEL_V21) && (defined(GCU_MODEL_V22C) || defined(GCU_MODEL_V23D))) || \
    (defined(GCU_MODEL_V22C) && defined(GCU_MODEL_V23D))
#error "Define only one of GCU_MODEL_V21, GCU_MODEL_V22C, GCU_MODEL_V23D."
#endif

#if defined(GCU_MODEL_V21)
#ifndef GCU_EXPECTED_MODEL
#define GCU_EXPECTED_MODEL "v2.1"
#endif
#ifndef GCU_HAS_BQ25180
#define GCU_HAS_BQ25180 0
#endif
#ifndef GCU_I2C_HZ
#define GCU_I2C_HZ 1000000
#endif
#elif defined(GCU_MODEL_V22C)
#ifndef GCU_EXPECTED_MODEL
#define GCU_EXPECTED_MODEL "v2.2.c"
#endif
#ifndef GCU_HAS_BQ25180
#define GCU_HAS_BQ25180 0
#endif
#ifndef GCU_I2C_HZ
#define GCU_I2C_HZ 1000000
#endif
#elif defined(GCU_MODEL_V23D)
#ifndef GCU_EXPECTED_MODEL
#define GCU_EXPECTED_MODEL "v2.3.d"
#endif
#ifndef GCU_HAS_BQ25180
#define GCU_HAS_BQ25180 1
#endif
#ifndef GCU_I2C_HZ
#define GCU_I2C_HZ 1000000
#endif
#ifndef GCU_BQ25180_I2C_HZ
#define GCU_BQ25180_I2C_HZ 400000
#endif
#else
#ifndef GCU_EXPECTED_MODEL
#define GCU_EXPECTED_MODEL "v2.1"
#endif
#ifndef GCU_HAS_BQ25180
#define GCU_HAS_BQ25180 0
#endif
#ifndef GCU_I2C_HZ
#define GCU_I2C_HZ 1000000
#endif
#endif

#endif
