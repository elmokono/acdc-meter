#pragma once

#include "meter.h"

// Small ISRs as inline functions so they can be included only where used.
static inline ICACHE_RAM_ATTR void isrA()
{
    A.pulses++;
    A.windowPulses++;
}

static inline ICACHE_RAM_ATTR void isrB()
{
    B.pulses++;
    B.windowPulses++;
}
