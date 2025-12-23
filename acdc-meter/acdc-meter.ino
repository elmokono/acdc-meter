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
#define WH_PER_PULSE (1000.0 / PULSES_PER_KWH)
#define MIN_PULSE_MS 150

#define SEND_INTERVAL_MS 15000
#define EEPROM_SIZE 64

#define EMA_ALPHA 0.2

/* ================= STRUCT ================= */
struct Meter {
  volatile uint32_t pulses;
  uint32_t lastPulses;

  float offsetKwh;
  float watts;
  float avgWatts;
  bool avgInit;

  unsigned long lastCalc;
};

/* ================= GLOBAL ================= */
Meter A, B;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
unsigned long lastSend = 0;

// ---------------- Pulse debounce filter ----------------
volatile unsigned long lastPulseMsA = 0;
volatile unsigned long lastPulseMsB = 0;

/* ================= ISR ================= */
ICACHE_RAM_ATTR void isrA() {
  unsigned long now = millis();
  if (now - lastPulseMsA >= MIN_PULSE_MS) {
    A.pulses++;
    lastPulseMsA = now;
  }
}

ICACHE_RAM_ATTR void isrB() {
  unsigned long now = millis();
  if (now - lastPulseMsB >= MIN_PULSE_MS) {
    B.pulses++;
    lastPulseMsB = now;
  }
}

uint32_t getPulsesSafe(Meter &m) {
  noInterrupts();
  uint32_t p = m.pulses;
  interrupts();
  return p;
}

/* ================= EEPROM ================= */
void loadEEPROM() {
  EEPROM.get(0, A.pulses);
  EEPROM.get(4, B.pulses);
  EEPROM.get(8, A.offsetKwh);
  EEPROM.get(12, B.offsetKwh);

  if (isnan(A.offsetKwh)) A.offsetKwh = 0;
  if (isnan(B.offsetKwh)) B.offsetKwh = 0;
}

void saveEEPROM() {
  uint32_t pA = getPulsesSafe(A);
  uint32_t pB = getPulsesSafe(B);
  EEPROM.put(0, pA);
  EEPROM.put(4, pB);
  EEPROM.put(8, A.offsetKwh);
  EEPROM.put(12, B.offsetKwh);
  EEPROM.commit();
}

/* ================= METER ================= */
void updateMeter(Meter &m) {
  unsigned long now = millis();
  if (now - m.lastCalc < 1000) return;

  uint32_t pulsesNow = getPulsesSafe(m);

  uint32_t diff = pulsesNow - m.lastPulses;
  m.lastPulses = pulsesNow;

  float wh = diff * WH_PER_PULSE;
  m.watts = wh * 3600.0;

  if (!m.avgInit) {
    m.avgWatts = m.watts;
    m.avgInit = true;
  } else {
    m.avgWatts = EMA_ALPHA * m.watts + (1.0 - EMA_ALPHA) * m.avgWatts;
  }

  m.lastCalc = now;
}

float totalKwh(Meter &m) {
  uint32_t p = getPulsesSafe(m);
  return (p * WH_PER_PULSE) / 1000.0 + m.offsetKwh;
}

void resetMeter(Meter &m, float newKwh) {
  noInterrupts();
  m.pulses = 0;
  m.lastPulses = 0;
  m.offsetKwh = newKwh;
  interrupts();
  saveEEPROM();
}

void resetPulses(Meter &m, uint32_t newPulses) {
  noInterrupts();
  m.pulses = newPulses;
  m.lastPulses = newPulses;
  interrupts();
  saveEEPROM();
}

/* ================= API ================= */
void handleData() {
  String json = "{";

  uint32_t pA, pB;
  noInterrupts();
  pA = A.pulses;
  pB = B.pulses;
  interrupts();

  json += "\"A\":{";
  json += "\"w\":" + String(A.watts, 1) + ",";
  json += "\"avg\":" + String(A.avgWatts, 1) + ",";
  json += "\"kwh\":" + String(totalKwh(A), 3) + ",";
  json += "\"pulses\":" + String(pA, 3);
  json += "},";

  json += "\"B\":{";
  json += "\"w\":" + String(B.watts, 1) + ",";
  json += "\"avg\":" + String(B.avgWatts, 1) + ",";
  json += "\"kwh\":" + String(totalKwh(B), 3) + ",";
  json += "\"pulses\":" + String(pB, 3);
  json += "}";

  json += "}";

  server.send(200, "application/json", json);
}

void handleReset() {
  String uri = server.uri();  // /reset/A/12.3
  uri.replace("/reset/", "");
  int s = uri.indexOf('/');
  if (s < 0) return server.send(400, "text/plain", "Bad request");

  String id = uri.substring(0, s);
  float val = uri.substring(s + 1).toFloat();

  if (id == "A") {
    resetMeter(A, val);
  } else if (id == "B") {
    resetMeter(B, val);
  } else if (id == "PA") {
    resetPulses(A, (uint32_t)val);
  } else if (id == "PB") {
    resetPulses(B, (uint32_t)val);
  } else {
    return server.send(404, "text/plain", "Meter not found");
  }

  server.send(200, "text/plain", "OK");
}

/* ================= THINGSPEAK ================= */
void sendThingSpeak() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;

  String url =
    "http://api.thingspeak.com/update?api_key="
    + String(THINGSPEAK_KEY)
    + "&field1=" + String(A.watts, 1)
    + "&field2=" + String(totalKwh(A), 3)
    + "&field3=" + String(A.pulses, 3)
    + "&field4=" + String(B.watts, 1)
    + "&field5=" + String(totalKwh(B), 3)
    + "&field6=" + String(B.pulses, 3);

  http.begin(client, url);
  http.GET();
  http.end();
}

/* ================= SETUP ================= */
void setup() {

  ESP.wdtEnable(8000);  // 8 segundos

  Serial.begin(115200);

  EEPROM.begin(EEPROM_SIZE);
  loadEEPROM();

  A.lastCalc = millis();
  B.lastCalc = millis();
  A.avgWatts = 0;
  B.avgWatts = 0;
  A.avgInit = false;
  B.avgInit = false;

  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_A), isrA, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_B), isrB, FALLING);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(300);

  ArduinoOTA.setHostname("acdc-meter");
  ArduinoOTA.begin();

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/data", handleData);
  server.onNotFound([]() {
    if (server.uri().startsWith("/reset/")) handleReset();
    else server.send(404, "text/plain", "Not found");
  });

  httpUpdater.setup(&server, "/update");

  server.begin();
}

/* ================= LOOP ================= */
void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  updateMeter(A);
  updateMeter(B);

  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    lastSend = millis();
    sendThingSpeak();
    saveEEPROM();  // persistencia segura
  }

  yield();
}
