#!/bin/bash

set -e

export ESP_URL="http://acdc-meter.local/data"
export DB_PATH="$HOME/energy-monitor/energy.db"

cd "$HOME/energy-monitor"

./ingest.py >> ingest.log 2>&1
