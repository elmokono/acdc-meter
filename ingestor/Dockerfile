FROM python:3.11-slim

WORKDIR /app

# Copiar archivos
COPY requirements.txt .
COPY ingest.py .
COPY init_db.sql .

# Instalar dependencias
RUN pip install --no-cache-dir -r requirements.txt

# Crear directorio de datos
RUN mkdir -p /data

# Debug: verificar archivos
RUN ls -la /app/

# Ejecutar
ENTRYPOINT ["python3", "-u", "ingest.py"]
