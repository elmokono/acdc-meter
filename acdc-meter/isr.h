#pragma once
#include "meter.h"

// Las ISRs deben estar en RAM (ICACHE_RAM_ATTR)
ICACHE_RAM_ATTR void isrA()
{
    A.pulses++;
    A.windowPulses++;
}

ICACHE_RAM_ATTR void isrB()
{
    B.pulses++;
    B.windowPulses++;
}
