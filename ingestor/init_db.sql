CREATE TABLE IF NOT EXISTS metrics (
        ts TEXT PRIMARY KEY,
        a_w REAL,
        a_pulses REAL,
        a_kwh REAL,
        b_w REAL,
        b_pulses REAL,
        b_kwh REAL
    )

CREATE INDEX IF NOT EXISTS idx_raw_ts ON raw_data(ts);

-- dispositivos configurables
CREATE TABLE IF NOT EXISTS devices (
  id TEXT PRIMARY KEY,
  home TEXT,              -- 'A' | 'B'
  name TEXT,
  type TEXT,              -- 'step' | 'plateau' | 'modulated'
  min_w REAL,
  max_w REAL
);

-- eventos detectados
CREATE TABLE IF NOT EXISTS events (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  device_id TEXT,
  home TEXT,
  start_ts TEXT,
  end_ts TEXT,
  avg_power REAL,
  energy_kwh REAL
);

-- estado actual para detección online
CREATE TABLE IF NOT EXISTS device_state (
  device_id TEXT PRIMARY KEY,
  is_on INTEGER,
  last_change_ts TEXT
);