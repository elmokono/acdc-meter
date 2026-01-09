#!/usr/bin/env python3

import sqlite3
import yaml
from datetime import datetime, timezone

DB_PATH = "energy.db"
DEVICES_YAML = "devices.yaml"

# ----------------------------
# Helpers
# ----------------------------
def log(msg):
    print(f"[detect-events] {msg}")

def parse_ts(ts):
    return datetime.fromisoformat(ts).astimezone(timezone.utc)

# ----------------------------
# Load devices
# ----------------------------
with open(DEVICES_YAML) as f:
    DEVICES = yaml.safe_load(f)

# DEVICES[vivienda] = list[device]
# device: id, name, min_w, max_w, min_dur, max_dur, type

# ----------------------------
# DB
# ----------------------------
db = sqlite3.connect(DB_PATH)
db.row_factory = sqlite3.Row
cur = db.cursor()

cur.execute("""
CREATE TABLE IF NOT EXISTS events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT,
    vivienda TEXT,
    start_ts TEXT,
    end_ts TEXT,
    duration_s INTEGER,
    avg_w REAL
)
""")
db.commit()

# ----------------------------
# State
# ----------------------------
active = {}  
# key = (vivienda, device_id)
# value = {start_ts, start_w}

# ----------------------------
# Process metrics
# ----------------------------
rows = cur.execute(
    "SELECT ts, a_w, b_w FROM metrics ORDER BY ts"
).fetchall()

for r in rows:
    ts = parse_ts(r["ts"])

    for vivienda, watts in [("A", r["a_w"]), ("B", r["b_w"])]:
        if watts is None:
            continue

        for d in DEVICES.get(vivienda, []):
            key = (vivienda, d["id"])
            in_range = d["min_w"] <= watts <= d["max_w"]

            # ---- ON ----
            if in_range and key not in active:
                active[key] = {
                    "start_ts": ts,
                    "start_w": watts,
                }
                log(f"{ts} | {vivienda} | {d['id']} ON (w={int(watts)})")

            # ---- OFF ----
            if not in_range and key in active:
                start = active[key]["start_ts"]
                dur = int((ts - start).total_seconds())
                avg_w = active[key]["start_w"]

                log(f"{ts} | {vivienda} | {d['id']} OFF (dur={dur}s, w={int(avg_w)})")

                if d["min_dur"] <= dur <= d["max_dur"]:
                    cur.execute(
                        """
                        INSERT INTO events
                        (device_id, vivienda, start_ts, end_ts, duration_s, avg_w)
                        VALUES (?, ?, ?, ?, ?, ?)
                        """,
                        (
                            d["id"],
                            vivienda,
                            start.isoformat(),
                            ts.isoformat(),
                            dur,
                            avg_w,
                        ),
                    )
                    db.commit()
                    log(f"  ↳ EVENT saved ({d['id']})")
                else:
                    log(f"  ↳ discarded (duration out of range)")

                del active[key]

# ----------------------------
# Persist open events
# ----------------------------
for (vivienda, device_id), st in active.items():
    log(f"Persist ON state {device_id} since {st['start_ts']}")

log("Done")
