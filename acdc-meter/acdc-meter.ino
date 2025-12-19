#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

/* =========================
   CONFIGURACIÓN HARDWARE
   ========================= */
#define PIN_A D5  // GPIO14
#define PIN_B D6  // GPIO12

#define PULSES_PER_KWH 2000.0
#define WH_PER_PULSE (1000.0 / PULSES_PER_KWH)

/* =========================
   WIFI
   ========================= */
const char* WIFI_SSID = "CASA-ALT";
const char* WIFI_PASS = "danielalberto2603";

/* =========================
   EEPROM
   ========================= */
#define EEPROM_MAGIC 0xA5A55A5A

struct PersistData {
  uint32_t magic;
  uint32_t pulsesA;
  uint32_t pulsesB;
  float offsetA;
  float offsetB;
};

PersistData cfg;

/* =========================
   MEDIDORES
   ========================= */
struct Meter {
  volatile uint32_t pulses = 0;
  uint32_t lastPulses = 0;
  float offset = 0;
};

Meter meterA, meterB;

/* =========================
   SERVER
   ========================= */
ESP8266WebServer server(80);

/* =========================
   INTERRUPCIONES
   ========================= */
void ICACHE_RAM_ATTR isrA() {
  meterA.pulses++;
}

void ICACHE_RAM_ATTR isrB() {
  meterB.pulses++;
}

/* =========================
   EEPROM HELPERS
   ========================= */
void loadEEPROM() {
  EEPROM.get(0, cfg);

  if (cfg.magic != EEPROM_MAGIC) {
    // EEPROM virgen o inválida
    cfg.magic = EEPROM_MAGIC;
    cfg.pulsesA = 0;
    cfg.pulsesB = 0;
    cfg.offsetA = 0;
    cfg.offsetB = 0;

    EEPROM.put(0, cfg);
    EEPROM.commit();
  }

  meterA.pulses = cfg.pulsesA;
  meterB.pulses = cfg.pulsesB;
  meterA.offset = cfg.offsetA;
  meterB.offset = cfg.offsetB;
}

void saveEEPROM() {
  cfg.magic = EEPROM_MAGIC;
  cfg.pulsesA = meterA.pulses;
  cfg.pulsesB = meterB.pulses;
  cfg.offsetA = meterA.offset;
  cfg.offsetB = meterB.offset;

  EEPROM.put(0, cfg);
  EEPROM.commit();
}

/* =========================
   CORS
   ========================= */
void sendCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

/* =========================
   API ENDPOINTS
   ========================= */
void handleData() {
  sendCORS();

  // Potencia instantánea (W)
  uint32_t diffA = meterA.pulses - meterA.lastPulses;
  uint32_t diffB = meterB.pulses - meterB.lastPulses;

  meterA.lastPulses = meterA.pulses;
  meterB.lastPulses = meterB.pulses;

  float wattsA = diffA * WH_PER_PULSE * 3600.0;
  float wattsB = diffB * WH_PER_PULSE * 3600.0;

  String json = "{";
  json += "\"A\":{";
  json += "\"kwh\":" + String(meterA.offset + meterA.pulses / PULSES_PER_KWH, 3) + ",";
  json += "\"w\":" + String(wattsA, 0);
  json += "},";
  json += "\"B\":{";
  json += "\"kwh\":" + String(meterB.offset + meterB.pulses / PULSES_PER_KWH, 3) + ",";
  json += "\"w\":" + String(wattsB, 0);
  json += "}";
  json += "}";

  server.send(200, "application/json", json);
}

void handleReset() {
  sendCORS();

  meterA.pulses = 0;
  meterB.pulses = 0;
  meterA.offset = 0;
  meterB.offset = 0;

  saveEEPROM();

  server.send(200, "application/json", "{\"status\":\"reset_ok\"}");
}

/* =========================
   SETUP
   ========================= */
void setup() {
  Serial.begin(115200);
  delay(100);

  EEPROM.begin(64);
  loadEEPROM();

  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_A), isrA, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_B), isrB, FALLING);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  server.on("/data", HTTP_GET, handleData);
  server.on("/reset", HTTP_POST, handleReset);
  server.onNotFound([]() {
    sendCORS();
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
}

/* =========================
   LOOP
   ========================= */
void loop() {
  server.handleClient();

  static unsigned long lastSave = 0;
  if (millis() - lastSave > 60000) {  // guardar cada 60 s
    lastSave = millis();
    saveEEPROM();
  }
}
