#pragma once

#include <Arduino.h>
#include <EEPROM.h>

struct PersistedData
{
    uint32_t pulsesA;
    uint32_t pulsesB;
    float offsetKwhA;
    float offsetKwhB;
};

void loadEEPROM();
void saveEEPROM();
