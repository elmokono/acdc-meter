import datetime
from flask import Flask, jsonify, render_template, request, send_from_directory
import sqlite3

from pytz import timezone

DB = "energy.db"
app = Flask(__name__)

def query(sql, args=()):
    con = sqlite3.connect(DB)
    con.row_factory = sqlite3.Row
    cur = con.cursor()
    cur.execute(sql, args)
    rows = cur.fetchall()
    con.close()
    return [dict(r) for r in rows]

@app.route("/api/metrics")
def metrics():
    return jsonify(query("""
        SELECT ts, a_w, a_kwh, b_w, b_kwh
        FROM metrics
        ORDER BY ts
    """))

@app.route("/events")
def events():
    home = request.args.get("home", "A")
    since = datetime.now(timezone.utc) - datetime.timedelta(hours=24)

    rows = query("""
        SELECT ts_start, ts_end, device, watts_avg, duration_s
        FROM events
        WHERE home = ?
          AND ts_start >= ?
        ORDER BY ts_start
    """, (home, since.isoformat()))

    return jsonify([dict(r) for r in rows])

@app.route("/")
def index():
    return render_template("index.html")

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8881)
