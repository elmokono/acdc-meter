#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include "secrets.h"

/* ================= CONFIG ================= */
#define PIN_A D5
#define PIN_B D6

#define PULSES_PER_KWH 2000.0
#define WH_PER_PULSE (1000.0 / PULSES_PER_KWH)

#define AVG_WINDOW_SEC 30
#define EMA_ALPHA 0.3

/* ================= SERVER ================= */
ESP8266WebServer server(80);

/* ================= STRUCT ================= */
struct Meter {
  volatile uint32_t pulses = 0;
  uint32_t lastPulses = 0;

  float wh_hist[AVG_WINDOW_SEC] = {0};
  uint8_t idx = 0;

  float offset = 0;
  float watts_ema = 0;

  unsigned long lastTick = 0;
};

Meter A, B;

/* ================= ISR ================= */
ICACHE_RAM_ATTR void isrA() { A.pulses++; }
ICACHE_RAM_ATTR void isrB() { B.pulses++; }

/* ================= EEPROM ================= */
void loadEEPROM() {
  EEPROM.get(0, A.pulses);
  EEPROM.get(8, B.pulses);
  EEPROM.get(16, A.offset);
  EEPROM.get(24, B.offset);

  if (isnan(A.offset)) A.offset = 0;
  if (isnan(B.offset)) B.offset = 0;
}

void saveEEPROM() {
  EEPROM.put(0, A.pulses);
  EEPROM.put(8, B.pulses);
  EEPROM.put(16, A.offset);
  EEPROM.put(24, B.offset);
  EEPROM.commit();
}

/* ================= METER ================= */
void updateMeter(Meter &m) {
  if (millis() - m.lastTick >= 1000) {
    m.lastTick += 1000;

    uint32_t diff = m.pulses - m.lastPulses;
    m.lastPulses = m.pulses;

    m.wh_hist[m.idx] = diff * WH_PER_PULSE;
    m.idx = (m.idx + 1) % AVG_WINDOW_SEC;
  }
}

float avgWatts(Meter &m) {
  float sum = 0;
  for (int i = 0; i < AVG_WINDOW_SEC; i++)
    sum += m.wh_hist[i];

  return (sum / AVG_WINDOW_SEC) * 3600.0;
}

float smoothWatts(Meter &m, float inst) {
  m.watts_ema = EMA_ALPHA * inst + (1 - EMA_ALPHA) * m.watts_ema;
  return m.watts_ema;
}

float totalKwh(Meter &m) {
  return (m.pulses * WH_PER_PULSE) / 1000.0 + m.offset;
}

/* ================= RESET ================= */
void resetMeter(Meter &m, float newKwh) {
  noInterrupts();

  m.pulses = 0;
  m.lastPulses = 0;
  m.offset = newKwh;
  m.watts_ema = 0;

  for (int i = 0; i < AVG_WINDOW_SEC; i++)
    m.wh_hist[i] = 0;

  m.idx = 0;
  interrupts();
}

/* ================= API ================= */
void handleData() {
  float wA = smoothWatts(A, avgWatts(A));
  float wB = smoothWatts(B, avgWatts(B));

  server.send(200, "application/json",
    "{"
    "\"A\":{\"w\":" + String(wA,1) + ",\"kwh\":" + String(totalKwh(A),3) + "},"
    "\"B\":{\"w\":" + String(wB,1) + ",\"kwh\":" + String(totalKwh(B),3) + "}"
    "}"
  );
}

void handleResetDynamic() {
  String uri = server.uri();   // /reset/A/157
  uri.replace("/reset/", "");

  int slash = uri.indexOf('/');
  if (slash < 0) {
    server.send(400, "text/plain", "Bad request");
    return;
  }

  String id = uri.substring(0, slash);
  float value = uri.substring(slash + 1).toFloat();

  if (id == "A") resetMeter(A, value);
  else if (id == "B") resetMeter(B, value);
  else {
    server.send(404, "text/plain", "Meter not found");
    return;
  }

  saveEEPROM();
  server.send(200, "text/plain", "OK");
}

/* ================= UI ================= */
void handleUI() {
  server.send(200, "text/html",
R"rawliteral(
<!DOCTYPE html><html lang="es"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Energy Monitor</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
body{margin:0;font-family:Arial;background:#fff;color:#222}
header{padding:12px;text-align:center;font-size:20px;font-weight:bold}
.container{display:grid;grid-template-columns:1fr;gap:20px;padding:15px}
.card{border:1px solid #ddd;border-radius:10px;padding:15px}
@media(min-width:900px){.container{grid-template-columns:1fr 1fr}}
</style></head>
<body>
<header>Monitoreo de Energía</header>

<div class="container">
<div class="card"><h3 style="color:#1e88e5">Vivienda A</h3><canvas id="ca"></canvas>kWh: <span id="ka">0</span></div>
<div class="card"><h3 style="color:#fb8c00">Vivienda B</h3><canvas id="cb"></canvas>kWh: <span id="kb">0</span></div>
</div>

<div class="card" style="margin:15px">
<h3>Comparación</h3><canvas id="pie"></canvas>
</div>

<script>
const max=60;
const ca=new Chart(ca,{type:'line',data:{labels:[],datasets:[{data:[],borderColor:'#1e88e5'}]},options:{animation:false}});
const cb=new Chart(cb,{type:'line',data:{labels:[],datasets:[{data:[],borderColor:'#fb8c00'}]},options:{animation:false}});
const pie=new Chart(pie,{type:'pie',data:{labels:['A','B'],datasets:[{data:[0,0],backgroundColor:['#1e88e5','#fb8c00']}]}});
function push(c,v){c.data.labels.push('');c.data.datasets[0].data.push(v);if(c.data.labels.length>max){c.data.labels.shift();c.data.datasets[0].data.shift();}c.update();}
setInterval(()=>fetch('/data').then(r=>r.json()).then(d=>{
push(ca,d.A.w);push(cb,d.B.w);
ka.innerText=d.A.kwh.toFixed(3);kb.innerText=d.B.kwh.toFixed(3);
pie.data.datasets[0].data=[d.A.kwh,d.B.kwh];pie.update();
}),1000);
</script>
</body></html>
)rawliteral");
}

/* ================= NOT FOUND ================= */
void handleNotFound() {
  if (server.method() == HTTP_POST && server.uri().startsWith("/reset/")) {
    handleResetDynamic();
    return;
  }
  server.send(404, "text/plain", "Not found");
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  EEPROM.begin(64);
  loadEEPROM();

  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_A), isrA, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_B), isrB, FALLING);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(300);

  server.on("/", handleUI);
  server.on("/data", handleData);
  server.onNotFound(handleNotFound);
  server.begin();
}

/* ================= LOOP ================= */
void loop() {
  server.handleClient();
  updateMeter(A);
  updateMeter(B);
}
