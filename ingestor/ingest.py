import sqlite3
import yaml
from datetime import datetime, timezone
from collections import defaultdict

DB = "energy.db"
DEVICES_FILE = "devices.yaml"

def log(msg):
    print(f"[ingest] {msg}")

def load_devices():
    with open(DEVICES_FILE, "r") as f:
        return yaml.safe_load(f)

def utcnow():
    return datetime.now(timezone.utc)

def insert_event(cur, device_id, vivienda, start_ts, end_ts, duration_s, avg_w):
    cur.execute("""
        INSERT INTO events
        (device_id, vivienda, start_ts, end_ts, duration_s, avg_w)
        VALUES (?, ?, ?, ?, ?, ?)
    """, (
        device_id,
        vivienda,
        start_ts.isoformat(),
        end_ts.isoformat(),
        duration_s,
        avg_w
    ))

def ingest():
    conn = sqlite3.connect(DB)
    cur = conn.cursor()

    devices = load_devices()
    active = defaultdict(dict)  # home -> device_id -> {start_ts, start_w}

    for home, devs in devices.items():
        log(f"processing home {home}")

        # ---- metrics ----
        row = cur.execute("""
            SELECT ts, a_w, b_w
            FROM metrics
            ORDER BY ts DESC
            LIMIT 1
        """).fetchone()

        if not row:
            continue

        _, wA, wB = row
        watts = wA if home == "A" else wB

        if watts is None:
            continue

        for d in devs:
            # ---- SOLO DISPOSITIVOS POR CONSUMO ----
            if d.get("type") == "tasmota":
                # Deshabilitado a propósito (modelo incompatible)
                continue

            in_range = d["min_w"] <= watts <= d["max_w"]

            # ---- ON ----
            if in_range and d["id"] not in active[home]:
                log(f"{home} | {d['id']} ON (w={int(watts)})")
                active[home][d["id"]] = {
                    "start_ts": utcnow(),
                    "start_w": watts
                }

            # ---- OFF ----
            if not in_range and d["id"] in active[home]:
                st = active[home][d["id"]]
                end_ts = utcnow()
                dur = int((end_ts - st["start_ts"]).total_seconds())

                if d["min_dur"] <= dur <= d["max_dur"]:
                    log(f"{home} | {d['id']} OFF ({dur}s)")
                    insert_event(
                        cur,
                        device_id=d["id"],
                        vivienda=home,
                        start_ts=st["start_ts"],
                        end_ts=end_ts,
                        duration_s=dur,
                        avg_w=st["start_w"]
                    )
                    conn.commit()
                else:
                    log(f"{home} | {d['id']} discarded (dur={dur}s)")

                active[home].pop(d["id"], None)

    conn.close()
    log("done")

if __name__ == "__main__":
    ingest()
