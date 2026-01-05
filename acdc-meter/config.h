#pragma once

// Pines
#define PIN_A D5
#define PIN_B D6

// Pulsos y ventanas
#define PULSES_PER_KWH 2000.0
#define WATTS_AVG_WINDOW_MS 30000
#define WH_PER_PULSE (1000.0 / PULSES_PER_KWH)
#define WATTS_WINDOW_MS 10000UL // 10 segundos ventana

// Envíos y EEPROM
#define SEND_INTERVAL_MS 15000
#define EEPROM_SIZE 64

// EMA dinámico
#define EMA_ALPHA_MIN 0.2  // (consumo bajo → muy suave)
#define EMA_ALPHA_MAX 0.35 // (consumo alto → más reactivo)

#define EMA_WATTS_LOW 200.0   // (debajo de esto → alpha mínimo)
#define EMA_WATTS_HIGH 3000.0 // (encima de esto → alpha máximo)
