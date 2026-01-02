#pragma once

#include <Arduino.h>
#include "config.h"

struct Meter
{
    volatile uint32_t pulses;
    uint32_t lastPulses;

    float offsetKwh;
    float watts;
    float emaAlpha;

    unsigned long lastCalc;
    unsigned long windowStartMs;
    unsigned long windowPulses;
    unsigned long windowDurationMs;
};

extern Meter A;
extern Meter B;

void initMeters();
void updateMeter(Meter &m);
float totalKwh(Meter &m);
void resetMeter(Meter &m, float newKwh);
void resetPulses(Meter &m, uint32_t newPulses);
