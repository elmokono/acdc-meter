#!/usr/bin/env python3

import os
import time
import requests
import sqlite3
from datetime import datetime, timezone

# ================= CONFIG =================
ESP_URL = os.getenv("ESP_URL")  # ej: http://192.168.1.50/data
DB_PATH = os.getenv("DB_PATH", "energy.db")
TIMEOUT = 5

if not ESP_URL:
    raise RuntimeError("ESP_URL no está definido")

# ================= DB =================
def init_db():
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()

    cur.execute("""
    CREATE TABLE IF NOT EXISTS metrics (
        ts TEXT PRIMARY KEY,
        a_w REAL,
        a_pulses REAL,
        a_kwh REAL,
        b_w REAL,
        b_pulses REAL,
        b_kwh REAL
    )
    """)

    conn.commit()
    conn.close()

# ================= INGEST =================
def ingest_once():
    r = requests.get(ESP_URL, timeout=TIMEOUT)
    r.raise_for_status()

    data = r.json()

    ts = datetime.now(timezone.utc).isoformat(timespec="seconds")

    row = (
        ts,
        data["A"]["w_inst"],
        data["A"]["pulses"],
        data["A"]["kwh"],
        data["B"]["w_inst"],
        data["B"]["pulses"],
        data["B"]["kwh"],
    )

    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()

    cur.execute("""
        INSERT OR REPLACE INTO metrics
        (ts, a_w, a_pulses, a_kwh, b_w, b_pulses, b_kwh)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    """, row)

    conn.commit()
    conn.close()

    print(f"[OK] {ts}  A:{data['A']['w_inst']}W  B:{data['B']['w_inst']}W")

# ================= MAIN =================
if __name__ == "__main__":
    init_db()
    ingest_once()
