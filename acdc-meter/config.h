#pragma once

#include <Arduino.h>

// Pins
#define PIN_A D5
#define PIN_B D6

// Pulse / energy
#define PULSES_PER_KWH 2000.0
#define WH_PER_PULSE (1000.0 / PULSES_PER_KWH)

// Window and timing constants
#define WATTS_WINDOW_MS 10000UL
#define WATTS_AVG_WINDOW_MS 30000
#define SEND_INTERVAL_MS 15000

// EEPROM
#define EEPROM_SIZE 64

// Dynamic EMA settings
#define EMA_ALPHA_MIN 0.2
#define EMA_ALPHA_MAX 0.35
#define EMA_WATTS_LOW 200.0
#define EMA_WATTS_HIGH 3000.0
