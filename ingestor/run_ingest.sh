#!/bin/bash

set -e

source config.env

cd "$HOME/energy-monitor"

./ingest.py >> ingest.log 2>&1
