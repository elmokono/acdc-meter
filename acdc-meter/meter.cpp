#include "meter.h"
#include "config.h"
#include "storage.h" // saveEEPROM()

Meter A;
Meter B;

static unsigned long selectWindowMs(unsigned long pulses)
{
    if (pulses <= 1)
        return 30000;
    if (pulses <= 4)
        return 20000;
    if (pulses <= 9)
        return 10000;
    return 5000;
}

static float computeDynamicAlpha(float watts)
{
    if (watts <= EMA_WATTS_LOW)
        return EMA_ALPHA_MIN;
    if (watts >= EMA_WATTS_HIGH)
        return EMA_ALPHA_MAX;

    float ratio = (watts - EMA_WATTS_LOW) / (EMA_WATTS_HIGH - EMA_WATTS_LOW);
    return EMA_ALPHA_MIN + ratio * (EMA_ALPHA_MAX - EMA_ALPHA_MIN);
}

void initMeters()
{
    A.lastCalc = millis();
    B.lastCalc = millis();
    A.emaAlpha = EMA_ALPHA_MIN;
    B.emaAlpha = EMA_ALPHA_MIN;
    A.windowStartMs = millis();
    A.windowPulses = 0;
    A.windowDurationMs = 30000;
    B.windowStartMs = millis();
    B.windowPulses = 0;
    B.windowDurationMs = 30000;
}

uint32_t getPulsesSafe(Meter &m)
{
    noInterrupts();
    uint32_t p = m.pulses;
    interrupts();
    return p;
}

void updateMeter(Meter &m)
{
    unsigned long now = millis();
    if (now - m.lastCalc < 1000)
        return;

    unsigned long elapsed = now - m.windowStartMs;
    if (elapsed >= m.windowDurationMs)
    {
        m.windowDurationMs = selectWindowMs(m.windowPulses);

        float hours = elapsed / 3600000.0;
        float kwh = m.windowPulses / PULSES_PER_KWH;

        float wattsInstant = (kwh / hours) * 1000.0;

        // update dynamic alpha based on current watts
        m.emaAlpha = computeDynamicAlpha(m.watts);

        // EMA (exponential moving average)
        m.watts = m.watts + m.emaAlpha * (wattsInstant - m.watts);

        m.windowPulses = 0;
        m.windowStartMs = now;
    }

    m.lastCalc = now;
}

float totalKwh(Meter &m)
{
    uint32_t p = getPulsesSafe(m);
    return (p * WH_PER_PULSE) / 1000.0 + m.offsetKwh;
}

void resetMeter(Meter &m, float newKwh)
{
    noInterrupts();
    m.pulses = 0;
    m.lastPulses = 0;
    m.offsetKwh = newKwh;
    interrupts();
    saveEEPROM();
}

void resetPulses(Meter &m, uint32_t newPulses)
{
    noInterrupts();
    m.pulses = newPulses;
    m.lastPulses = newPulses;
    interrupts();
    saveEEPROM();
}
