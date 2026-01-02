#include "storage.h"
#include "meter.h"
#include "config.h"

static PersistedData eep;

void loadEEPROM()
{
    EEPROM.get(0, eep);
    noInterrupts();
    A.pulses = eep.pulsesA;
    A.offsetKwh = eep.offsetKwhA;
    B.pulses = eep.pulsesB;
    B.offsetKwh = eep.offsetKwhB;
    interrupts();

    if (isnan(A.offsetKwh))
        A.offsetKwh = 0;
    if (isnan(B.offsetKwh))
        B.offsetKwh = 0;
}

void saveEEPROM()
{
    noInterrupts();
    eep.pulsesA = A.pulses;
    eep.offsetKwhA = A.offsetKwh;
    eep.pulsesB = B.pulses;
    eep.offsetKwhB = B.offsetKwh;
    interrupts();

    EEPROM.put(0, eep);
    EEPROM.commit();
}
