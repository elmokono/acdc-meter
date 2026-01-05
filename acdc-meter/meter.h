#pragma once

#include <Arduino.h>
#include <stdint.h>

struct PersistedData
{
    uint32_t pulsesA;
    uint32_t pulsesB;
    float offsetKwhA;
    float offsetKwhB;
};

struct Meter
{
    volatile uint32_t pulses;
    uint32_t lastPulses;

    float offsetKwh;
    float watts;
    float emaAlpha; // (alpha dinámico actual)

    unsigned long lastCalc;
    unsigned long windowStartMs;
    unsigned long windowPulses;
    unsigned long windowDurationMs;
};

extern Meter A;
extern Meter B;

// Meter API
uint32_t getPulsesSafe(Meter &m);
unsigned long selectWindowMs(unsigned long pulses);
float computeDynamicAlpha(float watts);
void updateMeter(Meter &m);
float totalKwh(Meter &m);
void resetMeter(Meter &m, float newKwh);
void resetPulses(Meter &m, uint32_t newPulses);
