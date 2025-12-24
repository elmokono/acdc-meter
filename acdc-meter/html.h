#pragma once

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">

<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>ACDC Energy Monitor</title>

  <!-- Chart.js -->
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

  <!-- Font Awesome -->
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.1/css/all.min.css" />

  <style>
    body {
      margin: 0;
      font-family: Arial, sans-serif;
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

    @media (min-width: 900px) {
      .container {
        grid-template-columns: 1fr 1fr;
      }
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
  </style>
</head>

<body>

  <header>
    <i class="fa-solid fa-bolt"></i>
    Monitoreo de Energía
  </header>

  <div class="container">
    <div class="card">
      <h3 style="color:#1e88e5">
        <i class="fa-solid fa-house"></i> Vivienda A
      </h3>
      <canvas id="ca"></canvas>
      kWh: <span id="ka">0</span>
    </div>

    <div class="card">
      <h3 style="color:#fb8c00">
        <i class="fa-solid fa-house"></i> Vivienda B
      </h3>
      <canvas id="cb"></canvas>
      kWh: <span id="kb">0</span>
    </div>
  </div>

  <div class="card" style="margin:15px">
    <h3>📈 Comparación A / B</h3>
    <canvas id="call"></canvas>
  </div>

  <div class="card" style="margin:15px">
    <h3>
      <i class="fa-solid fa-chart-pie"></i>
      Energía acumulada
    </h3>
    <canvas id="pie"></canvas>
  </div>

  <div class="card" style="margin:15px">
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
        <i class="fa-solid fa-eraser"></i> Resetear
      </button>

      <span id="msg"></span>
    </div>
  </div>

  <script>
    const MAX_POINTS = 60;
    const INTERVAL_MS = 1000;

    function push(chart, values) {
      chart.data.labels.push("");

      values.forEach((v, i) => {
        chart.data.datasets[i].data.push(v);
      });

      if (chart.data.labels.length > MAX_POINTS) {
        chart.data.labels.shift();
        chart.data.datasets.forEach(d => d.data.shift());
      }

      chart.update();
    }

    function baseDatasets(instColor, winColor, avgColor) {
      return [
        {
          label: "Inst",
          data: [],
          borderColor: instColor,
          borderWidth: 1,
          tension: 0.1,
          pointRadius: 0
        },
        {
          label: "Win",
          data: [],
          borderColor: winColor,
          borderWidth: 5,
          tension: 0.3,
          pointRadius: 0
        },
        {
          label: "Avg",
          data: [],
          borderColor: avgColor,
          borderWidth: 1,
          tension: 0.4,
          pointRadius: 0
        }
      ];
    }

    const chartA = new Chart(
      document.getElementById("ca").getContext("2d"),
      {
        type: "line",
        data: { labels: [], datasets: baseDatasets("#bbdefb", "#1e88e5", "#0d47a1") },
        options: { animation: false, plugins: { legend: { display: false } } }
      }
    );

    const chartB = new Chart(
      document.getElementById("cb").getContext("2d"),
      {
        type: "line",
        data: { labels: [], datasets: baseDatasets("#ffe0b2", "#fb8c00", "#e65100") },
        options: { animation: false, plugins: { legend: { display: false } } }
      }
    );

    const chartAll = new Chart(
      document.getElementById("call").getContext("2d"),
      {
        type: "line",
        data: {
          labels: [],
          datasets: [
            ...baseDatasets("#bbdefb", "#1e88e5", "#0d47a1"),
            ...baseDatasets("#ffe0b2", "#fb8c00", "#e65100")
          ]
        },
        options: {
          animation: false,
          plugins: { legend: { position: "bottom" } },
          scales: { y: { title: { display: true, text: "Watts" } } }
        }
      }
    );

    const chartPie = new Chart(
      document.getElementById("pie").getContext("2d"),
      {
        type: "pie",
        data: {
          labels: ["A", "B"],
          datasets: [{
            data: [0, 0],
            backgroundColor: ["#1e88e5", "#fb8c00"]
          }]
        }
      }
    );

    async function resetMeter() {
      const id = meter.value;
      const kwh = parseFloat(kwhValue.value || 0);
      await fetch(`/reset/${id}/${kwh}`, { method: "POST" });
    }

    setInterval(async () => {
      try {
        const r = await fetch("http://192.168.0.24/data");
        if (!r.ok) return;
        const d = await r.json();

        push(chartA, [d.A.w_inst, d.A.w_win, d.A.avg]);
        push(chartB, [d.B.w_inst, d.B.w_win, d.B.avg]);
        push(chartAll, [
          d.A.w_inst, d.A.w_win, d.A.avg,
          d.B.w_inst, d.B.w_win, d.B.avg
        ]);

        ka.textContent = d.A.kwh.toFixed(3);
        kb.textContent = d.B.kwh.toFixed(3);

        chartPie.data.datasets[0].data = [d.A.kwh, d.B.kwh];
        chartPie.update();

      } catch {
        console.warn("Sin datos del ESP");
      }
    }, INTERVAL_MS);
  </script>

</body>
</html>
)rawliteral";
