#pragma once

// Pins
#define PIN_A D5
#define PIN_B D6

// Pulses and windows
#define PULSES_PER_KWH 2000.0
#define WATTS_AVG_WINDOW_MS 30000
#define WH_PER_PULSE (1000.0 / PULSES_PER_KWH)
#define WATTS_WINDOW_MS 10000UL // 10 seconds window

// Send interval and EEPROM
#define SEND_INTERVAL_MS 15000
#define EEPROM_SIZE 64

// Dynamic EMA
#define EMA_ALPHA_MIN 0.2  // (low consumption → very smooth)
#define EMA_ALPHA_MAX 0.35 // (high consumption → more reactive)

#define EMA_WATTS_LOW 200.0   // (below this → min alpha)
#define EMA_WATTS_HIGH 3000.0 // (above this → max alpha)
