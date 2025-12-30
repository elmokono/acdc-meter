#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP.h>
#include "secrets.h"
#include "html.h"
#include <ESP8266HTTPUpdateServer.h>

/* ================= CONFIG ================= */
#define PIN_A D5
#define PIN_B D6

#define PULSES_PER_KWH 2000.0
#define WATTS_AVG_WINDOW_MS 30000
#define WH_PER_PULSE (1000.0 / PULSES_PER_KWH)
#define WATTS_WINDOW_MS 10000UL // 10 segundos ventana

#define SEND_INTERVAL_MS 15000
#define EEPROM_SIZE 64

#define EMA_ALPHA_MIN 0.2  // (consumo bajo → muy suave)
#define EMA_ALPHA_MAX 0.35 // (consumo alto → más reactivo)

#define EMA_WATTS_LOW 200.0   // (debajo de esto → alpha mínimo)
#define EMA_WATTS_HIGH 3000.0 // (encima de esto → alpha máximo)

/* ================= STRUCT ================= */
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

/* ================= GLOBAL ================= */
Meter A, B;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
unsigned long lastSend = 0;
PersistedData eep;

/* ================= ISR ================= */
ICACHE_RAM_ATTR void isrA()
{
  A.pulses++;
  A.windowPulses++;
}

ICACHE_RAM_ATTR void isrB()
{
  B.pulses++;
  B.windowPulses++;
}

uint32_t getPulsesSafe(Meter &m)
{
  noInterrupts();
  uint32_t p = m.pulses;
  interrupts();
  return p;
}

unsigned long selectWindowMs(unsigned long pulses)
{
  if (pulses <= 1)
    return 30000;
  if (pulses <= 4)
    return 20000;
  if (pulses <= 9)
    return 10000;
  return 5000;
}

float computeDynamicAlpha(float watts)
{ // 🔽 NUEVO
  if (watts <= EMA_WATTS_LOW)
    return EMA_ALPHA_MIN;
  if (watts >= EMA_WATTS_HIGH)
    return EMA_ALPHA_MAX;

  float ratio = (watts - EMA_WATTS_LOW) / (EMA_WATTS_HIGH - EMA_WATTS_LOW);

  return EMA_ALPHA_MIN + ratio * (EMA_ALPHA_MAX - EMA_ALPHA_MIN);
}
/* ================= EEPROM ================= */
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

/* ================= METER ================= */
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

    // EMA suave encima (opcional pero recomendado)
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

/* ================= API ================= */
void handleData()
{
  String json = "{";

  uint32_t pA = getPulsesSafe(A);
  uint32_t pB = getPulsesSafe(B);

  // Opcional: timestamp UTC (segundos desde epoch)
  unsigned long ts = millis() / 1000;

  json += "\"ts\":" + String(ts) + ",";
  json += "\"A\":{";
  json += "\"w_inst\":" + String(A.watts, 1) + ",";
  json += "\"kwh\":" + String(totalKwh(A), 3) + ",";
  json += "\"alpha\":" + String(A.emaAlpha, 3) + ",";
  json += "\"pulses\":" + String(pA);
  json += "},";

  json += "\"B\":{";
  json += "\"w_inst\":" + String(B.watts, 1) + ",";
  json += "\"kwh\":" + String(totalKwh(B), 3) + ",";
  json += "\"alpha\":" + String(B.emaAlpha, 3) + ",";
  json += "\"pulses\":" + String(pB);
  json += "}";

  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(200, "application/json", json);
}

void handleReset()
{
  String uri = server.uri(); // /reset/A/12.3
  uri.replace("/reset/", "");
  int s = uri.indexOf('/');
  if (s < 0)
    return server.send(400, "text/plain", "Bad request");

  String id = uri.substring(0, s);
  float val = uri.substring(s + 1).toFloat();

  if (id == "A")
  {
    resetMeter(A, val);
  }
  else if (id == "B")
  {
    resetMeter(B, val);
  }
  else if (id == "PA")
  {
    resetPulses(A, (uint32_t)val);
  }
  else if (id == "PB")
  {
    resetPulses(B, (uint32_t)val);
  }
  else
  {
    return server.send(404, "text/plain", "Meter not found");
  }

  server.send(200, "text/plain", "OK");
}

/* ================= THINGSPEAK ================= */
void sendThingSpeak()
{
  if (WiFi.status() != WL_CONNECTED)
    return;

  WiFiClient client;
  HTTPClient http;

  String url =
      "http://api.thingspeak.com/update?api_key=" + String(THINGSPEAK_KEY) + "&field1=" + String(A.watts, 1) + "&field2=" + String(totalKwh(A), 3) + "&field3=" + String(A.pulses) + "&field4=" + String(B.watts, 1) + "&field5=" + String(totalKwh(B), 3) + "&field6=" + String(B.pulses);

  http.begin(client, url);
  http.GET();
  http.end();
}

/* ================= SETUP ================= */
void setup()
{

  ESP.wdtEnable(8000); // 8 segundos

  Serial.begin(115200);

  EEPROM.begin(EEPROM_SIZE);
  loadEEPROM();

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

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
    delay(300);

  ArduinoOTA.setHostname("acdc-meter");
  ArduinoOTA.begin();

  server.on("/", HTTP_GET, []()
            { server.send_P(200, "text/html", INDEX_HTML); });

  server.on("/data", handleData);
  server.onNotFound([]()
                    {
    if (server.method() == HTTP_OPTIONS) {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
      server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
      server.send(204);
    } else {
      if (server.uri().startsWith("/reset/")) {
        handleReset();
      } else {
        server.send(404, "text/plain", "Not found");
      }
    } });

  httpUpdater.setup(&server, "/update");

  server.begin();
}

/* ================= LOOP ================= */
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
    saveEEPROM(); // persistencia segura
  }

  yield();
}
