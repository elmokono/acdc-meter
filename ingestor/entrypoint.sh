#!/bin/bash

set -e

# Inicializar base de datos leyendo init_db.sql
echo "Initializing database..."
python3 << 'EOF'
import sqlite3
import os

db_path = os.getenv('DB_PATH', 'energy.db')

# Leer y ejecutar init_db.sql
with open('init_db.sql', 'r') as f:
    sql_script = f.read()

try:
    conn = sqlite3.connect(db_path)
    conn.executescript(sql_script)
    conn.commit()
    conn.close()
    print('Database initialized successfully')
except Exception as e:
    print(f'Error initializing database: {e}')
    print(f'SQL:\n{sql_script}')
    raise
EOF

# Ejecutar el ingestor
echo "Starting ingestor..."
exec python3 ingest.py
