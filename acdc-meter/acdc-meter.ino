#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

#include "config.h"
#include "meter.h"
#include "isr.h"
#include "storage.h"
#include "network.h"
#include "api.h"
#include "html.h"
#include "secrets.h"

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
unsigned long lastSend = 0;

void setup()
{
  ESP.wdtEnable(8000); // 8 segundos

  Serial.begin(115200);

  EEPROM.begin(EEPROM_SIZE);
  storageLoad();

  A.lastCalc = millis();
  B.lastCalc = millis();
  A.emaAlpha = EMA_ALPHA_MIN;
  B.emaAlpha = EMA_ALPHA_MIN;
  A.windowStartMs = millis();
  A.windowPulses = 0;
  A.windowDurationMs = 30000; // arranca conservador
  B.windowStartMs = millis();
  B.windowPulses = 0;
  B.windowDurationMs = 30000; // arranca conservador

  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_A), isrA, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_B), isrB, FALLING);

  networkInit(server, httpUpdater);

  apiSetupRoutes(server);
}

void loop()
{
  ArduinoOTA.handle();
  server.handleClient();

  updateMeter(A);
  updateMeter(B);

  if (millis() - lastSend >= SEND_INTERVAL_MS)
  {
    lastSend = millis();
    sendThingSpeak();
    storageSave(); // persistencia segura
  }

  yield();
}
