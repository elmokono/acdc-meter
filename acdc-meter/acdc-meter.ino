#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP.h>
#include "secrets.h"
#include <ESP8266HTTPUpdateServer.h>

/* ================= CONFIG ================= */
#define PIN_A D5
#define PIN_B D6

#define PULSES_PER_KWH 2000.0
#define WH_PER_PULSE (1000.0 / PULSES_PER_KWH)

#define SEND_INTERVAL_MS 15000
#define EEPROM_SIZE 64

#define EMA_ALPHA 0.2

/* ================= STRUCT ================= */
struct Meter {
  volatile uint32_t pulses;
  uint32_t lastPulses;

  float offsetKwh;
  float watts;
  float avgWatts;  // <-- NUEVO

  unsigned long lastCalc;
};

/* ================= GLOBAL ================= */
Meter A, B;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
unsigned long lastSend = 0;

/* ================= ISR ================= */
ICACHE_RAM_ATTR void isrA() {
  A.pulses++;
}
ICACHE_RAM_ATTR void isrB() {
  B.pulses++;
}

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">

<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Energy Monitor</title>

  <!-- Chart.js -->
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

  <!-- Font Awesome -->
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.1/css/all.min.css" />

  <style>
    body {
      margin: 0;
      font-family: Arial;
      background: #fff;
      color: #222;
    }

    header {
      padding: 12px;
      text-align: center;
      font-size: 20px;
      font-weight: bold;
    }

    header i {
      color: #fbc02d;
      margin-right: 8px;
    }

    .container {
      display: grid;
      grid-template-columns: 1fr;
      gap: 20px;
      padding: 15px;
    }

    .card {
      border: 1px solid #ddd;
      border-radius: 10px;
      padding: 15px;
    }

    h3 i {
      margin-right: 6px;
    }

    .reset {
      display: flex;
      gap: 10px;
      flex-wrap: wrap;
      align-items: center;
    }

    input,
    select,
    button {
      padding: 6px;
      font-size: 14px;
    }

    button {
      cursor: pointer;
    }

    button i {
      margin-right: 4px;
    }

    @media (min-width: 900px) {
      .container {
        grid-template-columns: 1fr 1fr;
      }
    }
  </style>
</head>

<body>
  <header>
    <i class="fa-solid fa-bolt"></i>
    Monitoreo de Energía
  </header>

  <div class="container">
    <div class="card">
      <h3 style="color: #1e88e5">
        <i class="fa-solid fa-house"></i>
        Vivienda A
      </h3>
      <canvas id="ca"></canvas>
      kWh: <span id="ka">0</span>
    </div>

    <div class="card">
      <h3 style="color: #fb8c00">
        <i class="fa-solid fa-house"></i>
        Vivienda B
      </h3>
      <canvas id="cb"></canvas>
      kWh: <span id="kb">0</span>
    </div>
  </div>

  <div class="card" style="margin: 15px">
    <h3>📈 Comparación en tiempo real</h3>
    <canvas id="call"></canvas>
  </div>

  <div class="card" style="margin: 15px">
    <h3>
      <i class="fa-solid fa-chart-pie"></i>
      Comparación
    </h3>
    <canvas id="pie"></canvas>
  </div>

  <!-- ===== RESET FORM ===== -->
  <div class="card" style="margin: 15px">
    <h3>
      <i class="fa-solid fa-rotate-right"></i>
      Resetear medidor
    </h3>

    <div class="reset">
      <select id="meter">
        <option value="A">Vivienda A</option>
        <option value="B">Vivienda B</option>
      </select>

      <input id="kwhValue" type="number" step="0.001" placeholder="kWh inicial" />

      <button onclick="resetMeter()">
        <i class="fa-solid fa-eraser"></i>
        Resetear
      </button>

      <span id="msg"></span>
    </div>
  </div>

<!-- ===== SCRIPT (sin cambios funcionales) ===== -->
  <script>
    const max = 60;
    const INTERVAL_SEC = 1;

    // ================= CANVAS =================
    const caCtx = document.getElementById("ca").getContext("2d");
    const cbCtx = document.getElementById("cb").getContext("2d");
    const callCtx = document.getElementById("call").getContext("2d");
    const pieCtx = document.getElementById("pie").getContext("2d");

    // ================= CHART A =================
    const chartA = new Chart(caCtx, {
      type: "line",
      data: {
        labels: [],
        datasets: [
          { label: "W", data: [], borderColor: "#1e88e5", borderWidth: 1, tension: 0.2 },
          { label: "Promedio", data: [], borderColor: "#0d47a1", borderWidth: 3, tension: 0.4 }
        ],
      },
      options: { animation: false, plugins: { legend: { display: false } } },
    });

    // ================= CHART B =================
    const chartB = new Chart(cbCtx, {
      type: "line",
      data: {
        labels: [],
        datasets: [
          { label: "W", data: [], borderColor: "#fb8c00", borderWidth: 1, tension: 0.2 },
          { label: "Promedio", data: [], borderColor: "#e65100", borderWidth: 3, tension: 0.4 }
        ],
      },
      options: { animation: false, plugins: { legend: { display: false } } },
    });

    // ================= CHART ALL =================
    const chartAll = new Chart(callCtx, {
      type: "line",
      data: {
        labels: [],
        datasets: [
          { label: "A inst", data: [], borderColor: "#90caf9", borderWidth: 1, tension: 0.15 },
          { label: "A prom", data: [], borderColor: "#0d47a1", borderWidth: 3, tension: 0.4 },
          { label: "B inst", data: [], borderColor: "#ffcc80", borderWidth: 1, tension: 0.15 },
          { label: "B prom", data: [], borderColor: "#e65100", borderWidth: 3, tension: 0.4 },
        ],
      },
      options: {
        animation: false,
        plugins: { legend: { position: "bottom" } },
        scales: { y: { title: { display: true, text: "Watts" } } },
      },
    });

    // ================= PIE =================
    const chartPie = new Chart(pieCtx, {
      type: "pie",
      data: {
        labels: ["A", "B"],
        datasets: [{ data: [0, 0], backgroundColor: ["#1e88e5", "#fb8c00"] }],
      },
    });

    // ================= HELPERS =================
    const push = function(chart, values) {
      chart.data.labels.push("");

      values.forEach((v, i) => {
        chart.data.datasets[i].data.push(v);
      });

      if (chart.data.labels.length > max) {
        chart.data.labels.shift();
        chart.data.datasets.forEach(d => d.data.shift());
      }

      chart.update();
    }

    // ================= RESET =================
    async function resetMeter() {
      const id = meter.value;      // "A" | "B"
      const kwh = parseFloat(kwhValue.value || 0);

      await fetch(`/reset/${id}/${kwh}`, { method: "POST" });
    }

    // ================= UPDATE LOOP =================
    setInterval(async () => {
      try {
        const r = await fetch("/data");
        if (!r.ok) return;

        const d = await r.json();

        // ---- Charts individuales ----
        push(chartA, [d.A.w, d.A.avg]);
        push(chartB, [d.B.w, d.B.avg]);

        // ---- Chart combinado ----
        push(chartAll, [d.A.w, d.A.avg, d.B.w, d.B.avg]);

        // ---- kWh ----
        ka.innerText = d.A.kwh.toFixed(3);
        kb.innerText = d.B.kwh.toFixed(3);

        // ---- Pie ----
        chartPie.data.datasets[0].data = [d.A.kwh, d.B.kwh];
        chartPie.update();

      } catch {
        console.warn("Sin datos del ESP");
      }
    }, INTERVAL_SEC * 1000);
  </script>
  
</body>

</html>
)rawliteral";

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
  EEPROM.put(0, A.pulses);
  EEPROM.put(4, B.pulses);
  EEPROM.put(8, A.offsetKwh);
  EEPROM.put(12, B.offsetKwh);
  EEPROM.commit();
}

/* ================= METER ================= */
void updateMeter(Meter &m) {
  unsigned long now = millis();
  if (now - m.lastCalc < 1000) return;

  uint32_t diff = m.pulses - m.lastPulses;
  m.lastPulses = m.pulses;

  float wh = diff * WH_PER_PULSE;
  m.watts = wh * 3600.0;

  if (m.avgWatts == 0) m.avgWatts = m.watts;
  else m.avgWatts = EMA_ALPHA * m.watts + (1 - EMA_ALPHA) * m.avgWatts;

  m.lastCalc = now;
}

float totalKwh(Meter &m) {
  return (m.pulses * WH_PER_PULSE) / 1000.0 + m.offsetKwh;
}

void resetMeter(Meter &m, float newKwh) {
  noInterrupts();
  m.pulses = 0;
  m.lastPulses = 0;
  m.offsetKwh = newKwh;
  interrupts();
  saveEEPROM();
}

/* ================= API ================= */
void handleData() {
  String json = "{";

  json += "\"A\":{";
  json += "\"w\":" + String(A.watts, 1) + ",";
  json += "\"avg\":" + String(A.avgWatts, 1) + ",";
  json += "\"kwh\":" + String(totalKwh(A), 3);
  json += "},";

  json += "\"B\":{";
  json += "\"w\":" + String(B.watts, 1) + ",";
  json += "\"avg\":" + String(B.avgWatts, 1) + ",";
  json += "\"kwh\":" + String(totalKwh(B), 3);
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

  if (id == "A") resetMeter(A, val);
  else if (id == "B") resetMeter(B, val);
  else return server.send(404, "text/plain", "Meter not found");

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
