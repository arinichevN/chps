#ifndef LIBPAS_GREEN_LIGHT_H
#define LIBPAS_GREEN_LIGHT_H

#include "acp/main.h"

typedef struct {
    SensorFTS sensor;
    float green_value;
    int active;
} GreenLight;

extern int greenLight_isGreen(GreenLight *item);

#endif

