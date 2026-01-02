#include "config.h"
#include "meter.h"
#include "isr.h"
#include "storage.h"
#include "network.h"
#include "api.h"
#include "html.h"
#include "secrets.h"

void setup()
{
  ESP.wdtEnable(8000);
  Serial.begin(115200);

  EEPROM.begin(EEPROM_SIZE);
  loadEEPROM();

  initMeters();

  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_A), isrA, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_B), isrB, FALLING);

  setupNetwork();
  setupApi();
}

void loop()
{
  networkHandle();

  updateMeter(A);
  updateMeter(B);

  yield();
}
