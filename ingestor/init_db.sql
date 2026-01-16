CREATE TABLE IF NOT EXISTS metrics (
        ts TEXT PRIMARY KEY,
        a_w REAL,
        a_pulses REAL,
        a_kwh REAL,
        b_w REAL,
        b_pulses REAL,
        b_kwh REAL
    );

CREATE INDEX IF NOT EXISTS idx_metrics_ts ON metrics(ts);
