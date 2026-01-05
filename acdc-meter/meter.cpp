#include "meter.h"
#include "config.h"
#include "storage.h" // reset functions will persist

Meter A, B;

uint32_t getPulsesSafe(Meter &m)
{
    noInterrupts();
    uint32_t p = m.pulses;
    interrupts();
    return p;
}

unsigned long selectWindowMs(unsigned long pulses)
{
    if (pulses == 0)
        return 40000;
    if (pulses <= 2)
        return 25000;
    if (pulses <= 6)
        return 15000;
    if (pulses <= 12)
        return 8000;
    return 5000;
}

float computeDynamicAlpha(float watts)
{ // 🔽 dynamic according to power
    if (watts <= EMA_WATTS_LOW)
        return EMA_ALPHA_MIN;
    if (watts >= EMA_WATTS_HIGH)
        return EMA_ALPHA_MAX;

    float ratio = (watts - EMA_WATTS_LOW) / (EMA_WATTS_HIGH - EMA_WATTS_LOW);

    return EMA_ALPHA_MIN + ratio * (EMA_ALPHA_MAX - EMA_ALPHA_MIN);
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

        // Update dynamic alpha according to measured power
        m.emaAlpha = computeDynamicAlpha(wattsInstant);

        // EMA on power
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
    storageSave();
}

void resetPulses(Meter &m, uint32_t newPulses)
{
    noInterrupts();
    m.pulses = newPulses;
    m.lastPulses = newPulses;
    interrupts();
    storageSave();
}
